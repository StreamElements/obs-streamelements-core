#include "StreamElementsSentryCrashHandler.hpp"

#include "StreamElementsConfig.hpp"
#include "StreamElementsCrashConsentDialog.hpp"
#include "StreamElementsCrashContext.hpp"
#include "StreamElementsUtils.hpp"

#include <util/base.h>
#include <obs.h>

#include <sentry.h>

#include <string>
#include <vector>

#include <util/platform.h>

#include <signal.h>
#include <string.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#include <mach-o/dyld.h>

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

// Empty means crash reporting is inert: sentry_init() is skipped entirely rather
// than run against a bad DSN, so a build without the DSN behaves like
// STREAMELEMENTS_CRASH_HANDLER=none instead of failing at runtime.
#ifndef SE_SENTRY_DSN
#define SE_SENTRY_DSN ""
#endif

/* ================================================================= */

static bool s_initialized = false;

// Contact details from the last report the user filled in. Read once at startup
// so the crash path never has to touch the config to prefill the prompt.
static std::string s_userName;
static std::string s_userEmail;
static std::string s_userDiscord;

//
// The signals sentry's native backend installs handlers for. Ours go on first,
// before sentry_init, so that sentry saves them as its "previous" handlers and
// calls them back once the daemon has captured the minidump.
//
static const int s_signals[] = {SIGSEGV, SIGBUS,  SIGFPE,
				SIGILL,  SIGABRT, SIGTRAP};

static struct sigaction
	s_previousHandlers[sizeof(s_signals) / sizeof(s_signals[0])];

static volatile sig_atomic_t s_handledCrash = 0;

// Owns the stack walker and the remote module-of-interest list. Built at
// startup, because building it fetches that list over HTTP.
static StreamElementsCrashContext *s_crashContext = nullptr;

// Marker left behind when a crash could not be consented to at the time --
// see ChainedSignalHandler. Resolved once at init so the signal handler only
// has to open a path that already exists as a string.
static char s_pendingMarkerPath[512] = {0};

// Identity of the crash event, stamped by OnCrash.
//
// The prompt runs after the envelope has been written, so nothing the user
// types can be added to the event itself -- see the note in OnCrash. What we
// can do is send their answer as a separate User Feedback item pointing back at
// this id, which is why the id has to be chosen by us rather than left to
// sentry__ensure_event_id.
static sentry_uuid_t s_crashEventId;
static bool s_haveCrashEventId = false;

// The same id, pre-formatted. The signal handler writes it into the pending
// marker for the next launch to pick up, and formatting it there would mean
// calling sentry_uuid_as_string from a signal handler; doing it in OnCrash
// leaves that path with nothing but open/write/close.
static char s_crashEventIdString[37] = {0};

/* ================================================================= */

//
// Directory containing this dylib, inside the plugin bundle.
//
// Left unset, sentry-native looks for the sentry-crash daemon next to the host
// executable -- OBS.app/Contents/MacOS -- which is not a directory this plugin
// installs into. We ship the daemon beside our own binary and point the SDK at
// it, exactly as the Windows handler does.
//
static bool GetOwnModuleDirectory(std::string &result)
{
	Dl_info info;

	// Any symbol in this image will do; this function is a convenient one.
	if (!dladdr((const void *)&GetOwnModuleDirectory, &info) ||
	    !info.dli_fname)
		return false;

	const std::string path = info.dli_fname;

	const size_t slash = path.rfind('/');

	if (slash == std::string::npos)
		return false;

	result = path.substr(0, slash + 1);

	return true;
}

//
// Attributes worth having as searchable tags. Same reasoning and same short
// list as the Windows handler: tags are capped under WER there, and keeping the
// two platforms queryable the same way matters more than the cap does here.
//
static bool IsTagWorthyAttribute(const std::string &name)
{
	return name == "product" || name == "selive.api.calls";
}

//
// Puts everything StreamElementsCrashContext gathered onto the scope, so the
// event sentry builds a moment later carries it. Mirrors the Windows handler.
//
static void ArmSentryScope(const StreamElementsCrashContext::Result &context)
{
	sentry_value_t attributes = sentry_value_new_object();

	for (auto &attribute : context.attributes) {
		sentry_value_set_by_key(
			attributes, attribute.name.c_str(),
			sentry_value_new_string(attribute.value.c_str()));

		if (IsTagWorthyAttribute(attribute.name)) {
			sentry_set_tag(attribute.name.c_str(),
				       attribute.value.c_str());
		}
	}

	sentry_set_context("selive", attributes);

	if (context.notes.size()) {
		sentry_value_t async = sentry_value_new_object();

		sentry_value_set_by_key(
			async, "callStack",
			sentry_value_new_string(context.notes.c_str()));

		sentry_set_context("async_call_context", async);
	}

	for (auto &attachment : context.attachments)
		sentry_attach_file(attachment.path.c_str());
}

//
// Identifies the reporter on every event. Mirrors the Windows handler.
//
static void SetSentryUser(const std::string &name, const std::string &email,
			  const std::string &discord)
{
	sentry_value_t user = sentry_value_new_object();

	sentry_value_set_by_key(
		user, "id",
		sentry_value_new_string(GetComputerSystemUniqueId().c_str()));

	if (name.size()) {
		sentry_value_set_by_key(user, "username",
					sentry_value_new_string(name.c_str()));
	}

	if (email.size()) {
		sentry_value_set_by_key(user, "email",
					sentry_value_new_string(email.c_str()));
	}

	if (discord.size()) {
		sentry_value_set_by_key(
			user, "discord",
			sentry_value_new_string(discord.c_str()));
	}

	sentry_set_user(user);
}

static void PersistContactDetails(const std::string &name,
				  const std::string &email,
				  const std::string &discord)
{
	auto config = StreamElementsConfig::GetInstance();

	if (!config)
		return;

	if (name.size() && name != s_userName) {
		config->SetCrashReportUserName(name);

		s_userName = name;
	}

	if (email.size() && email != s_userEmail) {
		config->SetCrashReportUserEmail(email);

		s_userEmail = email;
	}

	if (discord.size() && discord != s_userDiscord) {
		config->SetCrashReportUserDiscord(discord);

		s_userDiscord = discord;
	}
}

/* ================================================================= */

// Progress window.
//
// Collect() zips the configuration tree and walks the stack, which takes long
// enough to be noticed -- and on macOS it runs in OnCrash, i.e. *before* the
// consent dialog appears. Without this the user sees OBS simply stop
// responding, with no indication that anything is happening at all.
//
// The sentry-crash daemon cannot report this for us: it is headless and tells
// the crashing process nothing beyond its own log file. So we show the phases
// we can observe ourselves.
//
// The moving bar is a Core Animation layer rather than an NSProgressIndicator
// on purpose. Animations are committed to the render server and keep running
// there, so the bar still moves while this thread is blocked inside Collect().
// Anything driven by the main run loop would sit frozen -- which is the exact
// impression we are trying to remove.

enum {
	CRASH_PROGRESS_COLLECTING = 0,
	CRASH_PROGRESS_UPLOADING = 1,
};

static NSWindow *s_progressPanel = nil;
static NSTextField *s_progressLabel = nil;

static NSString *CrashProgressStatus(int phase)
{
	switch (phase) {
	case CRASH_PROGRESS_UPLOADING:
		return @"Uploading the crash report…";
	default:
		return @"Collecting diagnostic information…";
	}
}

static NSTextField *CreateProgressLabel(NSRect frame, NSFont *font,
					NSColor *color, NSString *text)
{
	NSTextField *label = [[NSTextField alloc] initWithFrame:frame];

	[label setStringValue:text];
	[label setFont:font];
	[label setTextColor:color];
	[label setBezeled:NO];
	[label setDrawsBackground:NO];
	[label setEditable:NO];
	[label setSelectable:NO];

	return label;
}

static void StartCrashProgress(int phase)
{
	// AppKit is main-thread only. Off the main thread we cannot show this,
	// exactly as we cannot show the consent prompt -- see ChainedSignalHandler.
	if (![NSThread isMainThread])
		return;

	@autoreleasepool {
		if (s_progressPanel) {
			[s_progressLabel
				setStringValue:CrashProgressStatus(phase)];
		} else {
			const CGFloat width = 420;
			const CGFloat height = 140;

			s_progressPanel = [[NSPanel alloc]
				initWithContentRect:NSMakeRect(0, 0, width,
							       height)
					  styleMask:NSWindowStyleMaskTitled
					    backing:NSBackingStoreBuffered
					      defer:NO];

			[s_progressPanel setTitle:@"SE.Live"];
			[s_progressPanel setLevel:NSFloatingWindowLevel];
			[s_progressPanel setHidesOnDeactivate:NO];

			// Defaults to YES for a window created this way, which
			// would have -close release it out from under the
			// static below. We drop the panel by ordering it out.
			[s_progressPanel setReleasedWhenClosed:NO];

			[s_progressPanel center];

			NSView *content = [s_progressPanel contentView];
			[content setWantsLayer:YES];

			[content
				addSubview:
					CreateProgressLabel(
						NSMakeRect(20, height - 52,
							   width - 40, 22),
						[NSFont boldSystemFontOfSize:14],
						[NSColor labelColor],
						@"Sending your crash report")];

			s_progressLabel = CreateProgressLabel(
				NSMakeRect(20, height - 78, width - 40, 20),
				[NSFont systemFontOfSize:12],
				[NSColor labelColor],
				CrashProgressStatus(phase));

			[content addSubview:s_progressLabel];

			[content
				addSubview:CreateProgressLabel(
						   NSMakeRect(20, 16,
							      width - 40, 34),
						   [NSFont systemFontOfSize:11],
						   [NSColor secondaryLabelColor],
						   @"This can take up to a "
						   @"minute. OBS will close by "
						   @"itself once the report has "
						   @"been sent.")];

			const CGFloat trackWidth = width - 40;
			const CGFloat trackHeight = 6;
			const CGFloat chunkWidth = trackWidth / 4;

			NSView *track = [[NSView alloc]
				initWithFrame:NSMakeRect(20, height - 104,
							 trackWidth,
							 trackHeight)];

			[track setWantsLayer:YES];

			CALayer *trackLayer = [track layer];
			trackLayer.backgroundColor =
				[[NSColor separatorColor] CGColor];
			trackLayer.cornerRadius = trackHeight / 2;
			trackLayer.masksToBounds = YES;

			CALayer *chunk = [CALayer layer];
			chunk.backgroundColor =
				[[NSColor controlAccentColor] CGColor];
			chunk.cornerRadius = trackHeight / 2;
			chunk.frame = CGRectMake(0, 0, chunkWidth, trackHeight);

			[trackLayer addSublayer:chunk];
			[content addSubview:track];

			// Indeterminate: there is no progress figure to report,
			// so animate rather than invent a percentage.
			CABasicAnimation *slide = [CABasicAnimation
				animationWithKeyPath:@"position.x"];

			slide.fromValue = @(-chunkWidth / 2);
			slide.toValue = @(trackWidth + chunkWidth / 2);
			slide.duration = 1.4;
			slide.repeatCount = HUGE_VALF;

			[chunk addAnimation:slide forKey:@"slide"];
		}

		[s_progressPanel makeKeyAndOrderFront:nil];
		[s_progressPanel displayIfNeeded];

		// Hands the layer tree and its animation to the render server
		// now. Without this the window would be committed only when the
		// run loop next turned -- which, on this thread, is never.
		[CATransaction flush];
	}
}

static void StopCrashProgress()
{
	if (![NSThread isMainThread] || !s_progressPanel)
		return;

	@autoreleasepool {
		[s_progressPanel orderOut:nil];

		s_progressPanel = nil;
		s_progressLabel = nil;

		[CATransaction flush];
	}
}

/* ================================================================= */

//
// Runs inside sentry's crash path, before the daemon is told to write the
// minidump. This is the only point at which the event and the scope can still
// be changed: once the envelope is written there is no public API to add to it.
//
static sentry_value_t OnCrash(const sentry_ucontext_t *uctx,
			      sentry_value_t event, void *user_data)
{
	UNUSED_PARAMETER(uctx);
	UNUSED_PARAMETER(user_data);

	// First thing, and before anything below can run a nested run loop.
	// See IsCrashReportingInProgress() -- without this a host API call
	// queued before the fault gets delivered into the consent dialog's
	// modal loop and faults again on the already-crashed thread.
	SetCrashReportingInProgress();

	//
	// Deliberately not the place to filter. Discarding here would suppress
	// the event but not the minidump: the daemon notify that follows is
	// gated only on an atomic state flag and never consults this hook's
	// return value (sentry_crash_handler.c, around the sentry_handle_exception
	// call). Suppression is the consent gate's job instead -- see the class
	// comment.
	//
	// It is, however, the last point at which the scope can still be
	// changed: the envelope is written immediately after, and there is no
	// public API to add to one that already exists. So the whole payload is
	// gathered here rather than alongside the prompt.
	//
	// Which is also why the event id is assigned here. Left alone,
	// sentry__ensure_event_id picks one a moment later, inside the envelope
	// the daemon writes, and we never see it -- sentry_handle_exception
	// returns void. Setting it first means the prompt still has something to
	// attach the user's description to. ensure_event_id keeps any non-nil
	// value already present, so this simply wins.
	{
		s_crashEventId = sentry_uuid_new_v4();
		sentry_uuid_as_string(&s_crashEventId, s_crashEventIdString);

		sentry_value_set_by_key(
			event, "event_id",
			sentry_value_new_string(s_crashEventIdString));

		s_haveCrashEventId = true;
	}

	if (s_crashContext) {
		// Up before the slow part, not after: this hook runs ahead of
		// the consent dialog, so without it the whole of WalkStack and
		// Collect is dead air with the app unresponsive.
		StartCrashProgress(CRASH_PROGRESS_COLLECTING);

		s_crashContext->WalkStack(nullptr);

		sentry_value_set_by_key(
			event, "selive_module_on_stack",
			sentry_value_new_bool(s_crashContext->ShouldReport()));

		ArmSentryScope(s_crashContext->Collect());

		// The consent dialog is next, and it owns the screen from here.
		StopCrashProgress();
	}

	return event;
}

//
// Runs *after* the daemon has captured the minidump.
//
// sentry's macOS handler calls the handler it displaced once the dump is safely
// written, which is what makes this ordering possible at all -- dump first, ask
// second. Its Windows counterpart never does this.
//
//
// Restores the default disposition for `signum` and re-raises it, which
// terminates the process the way the signal would have.
//
// Returning from a handler for a fault signal does not: the faulting
// instruction simply re-executes, faults again, and the process spins forever.
// Every path out of the handler below that cannot hand the signal on has to
// come through here instead.
//
static void DieWithSignal(int signum)
{
	struct sigaction dfl = {};

	dfl.sa_handler = SIG_DFL;
	sigemptyset(&dfl.sa_mask);

	sigaction(signum, &dfl, NULL);

	raise(signum);
}

//
// Leaves a note for the next launch that a crash is still sitting in the cache
// unsent, either because we could not ask about it or because the upload did
// not finish. The contents are the crash's event id, so the deferred prompt can
// attach what the user types to the right crash; an empty marker is still a
// valid marker, it just means feedback has nowhere to point.
//
// Called from the signal handler, so nothing here does more than open/write/
// close: the path was resolved at init and the id formatted in OnCrash exactly
// so this function has neither to build.
//
static void WritePendingConsentMarker()
{
	if (!s_pendingMarkerPath[0])
		return;

	const int fd =
		open(s_pendingMarkerPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);

	if (fd < 0)
		return;

	if (s_crashEventIdString[0]) {
		const ssize_t ignored = write(fd, s_crashEventIdString,
					      strlen(s_crashEventIdString));

		UNUSED_PARAMETER(ignored);
	}

	close(fd);
}

static void ChainedSignalHandler(int signum, siginfo_t *info, void *context)
{
	// One crash only, and only one thread's. A plain read-then-write would
	// let two threads faulting at once both pass, and put up two dialogs.
	if (!__sync_bool_compare_and_swap(&s_handledCrash, 0, 1)) {
		// Another thread is already handling a crash. Do not return:
		// this thread's fault is unresolved, so returning would spin on
		// the faulting instruction.
		DieWithSignal(signum);

		return;
	}

	// Also set in OnCrash, which normally runs first. Repeated here because
	// this handler still runs when sentry_init() failed and there is no
	// OnCrash to reach.
	SetCrashReportingInProgress();

	if (s_initialized) {
		const auto consent = StreamElementsCrashConsentDialog::Prompt(
			s_userName, s_userEmail, s_userDiscord);

		if (consent.consented) {
			StartCrashProgress(CRASH_PROGRESS_UPLOADING);

			PersistContactDetails(consent.name, consent.email,
					      consent.discord);

			// Deliberately NOT sentry_set_user() here. The crash
			// envelope was sealed by the daemon before this prompt
			// ever appeared, so a scope change now reaches no
			// event at all -- it would only apply to some later
			// one, and this process is about to die. The identity
			// that travels with the crash is the one set at init;
			// what the user typed just now travels as feedback.
			if (consent.description.size() && s_haveCrashEventId) {
				sentry_capture_feedback(
					sentry_value_new_feedback(
						consent.description.c_str(),
						consent.email.size()
							? consent.email.c_str()
							: nullptr,
						consent.name.size()
							? consent.name.c_str()
							: nullptr,
						&s_crashEventId));
			}

			// Releases what the daemon already wrote. Until this
			// call the envelope sits in the cache unsent, which is
			// what makes "Don't send" mean it.
			sentry_user_consent_give();

			// And this is what actually pushes it out.
			//
			// Consent only moves the envelope into the retry queue;
			// the upload itself happens on the background worker.
			// Without a flush we returned immediately, chained to
			// OBS's handler, and the process died with the report
			// still queued -- and the "Uploading the crash report"
			// window, which has nothing else to cover, was on
			// screen for microseconds. Same 60s budget and the same
			// reasoning as the Windows handler's shutdown timeout.
			if (sentry_flush(60000) != 0) {
				// Timed out: the envelope is still cached and
				// this process is about to die. Consent is reset
				// at every startup (see the constructor), so
				// without a marker nothing would ever pick this
				// up again and the report would be stranded on
				// disk forever.
				WritePendingConsentMarker();
			}
		} else if (consent.prompted) {
			// An actual "Don't send". Drop it.
			sentry_user_consent_revoke();
		} else {
			// We could not ask -- the crash was off the main
			// thread, where AppKit cannot be driven. Revoking here
			// would discard the report, and since most crashes
			// happen on worker threads that would silently throw
			// away the majority of them.
			//
			// Leave consent untouched so the envelope stays cached,
			// and drop a marker for the next launch to notice and
			// ask about.
			WritePendingConsentMarker();
		}
	}

	// Before the host handler: OBS does its own crash reporting from here,
	// and a floating window of ours must not sit on top of it.
	StopCrashProgress();

	// Hand on to whoever held this signal before we did, so OBS's own
	// handling still runs.
	//
	// Normally this handler is not the one the kernel invoked: sentry's is,
	// and it calls us back after the daemon has captured the dump, then
	// re-raises once we return. But if sentry_init() failed we are still
	// installed and there is nobody behind us, so every path that does not
	// actually hand the signal on must terminate rather than return.
	const size_t count = sizeof(s_signals) / sizeof(s_signals[0]);

	for (size_t i = 0; i < count; ++i) {
		if (s_signals[i] != signum)
			continue;

		struct sigaction *previous = &s_previousHandlers[i];

		if ((previous->sa_flags & SA_SIGINFO) &&
		    previous->sa_sigaction) {
			previous->sa_sigaction(signum, info, context);

			return;
		}

		if (!(previous->sa_flags & SA_SIGINFO) &&
		    previous->sa_handler != SIG_DFL &&
		    previous->sa_handler != SIG_IGN) {
			previous->sa_handler(signum);

			return;
		}

		break;
	}

	// Nothing to hand on to -- the previous disposition was SIG_DFL or
	// SIG_IGN. Returning here would spin on the faulting instruction.
	DieWithSignal(signum);
}

static void InstallChainedHandlers()
{
	const size_t count = sizeof(s_signals) / sizeof(s_signals[0]);

	for (size_t i = 0; i < count; ++i) {
		struct sigaction action = {};

		action.sa_sigaction = ChainedSignalHandler;
		action.sa_flags = SA_SIGINFO | SA_ONSTACK;
		sigemptyset(&action.sa_mask);

		sigaction(s_signals[i], &action, &s_previousHandlers[i]);
	}
}

/* ================================================================= */

StreamElementsSentryCrashHandler::StreamElementsSentryCrashHandler()
{
	if (s_initialized)
		return;

	const std::string dsn = SE_SENTRY_DSN;

	if (dsn.empty()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: no Sentry DSN was compiled in; crash reporting is disabled");
		return;
	}

	//
	// Installed BEFORE sentry_init, and that order is the whole design.
	//
	// sentry's handler saves whatever it displaces and calls it back after
	// the daemon has captured the minidump. Going first here means going
	// *last* at crash time, which is exactly where the prompt belongs: the
	// dump already exists, so a slow or abandoned dialog cannot cost us the
	// crash data.
	//
	InstallChainedHandlers();

	sentry_options_t *options = sentry_options_new();

	sentry_options_set_dsn(options, dsn.c_str());

	const std::string release = "obs-streamelements-core@" +
				    GetStreamElementsPluginVersionString();
	sentry_options_set_release(options, release.c_str());

	//
	// Nothing leaves the machine until sentry_user_consent_give().
	//
	// This is what lets the prompt run after the dump and still mean
	// something: the daemon reads consent out of shared memory rather than
	// uploading blindly, so a refusal still stops the minidump.
	//
	sentry_options_set_require_user_consent(options, 1);

	//
	// ...and these two are what make the sentence above true.
	//
	// require_user_consent ON ITS OWN DISCARDS the report. In
	// sentry__capture_envelope, a withheld-consent envelope is only written
	// to the cache when cache_keep or http_retry is set; otherwise it is
	// dropped on the spot. A live crash test logged
	//
	//   INFO discarding envelope due to missing user consent
	//
	// about a second after the fault -- while the consent dialog was still
	// on screen, and therefore long before it was answered. The whole
	// dump-first-ask-second design rests on the envelope surviving until the
	// answer, so without these the prompt could never deliver anything and
	// "Send report" was a button that did nothing.
	//
	// http_retry additionally means a report that could not go out now --
	// consent given too late, network down, process dying mid-upload -- is
	// picked up on a later run instead of being lost.
	//
	sentry_options_set_cache_keep(options, SENTRY_CACHE_KEEP_OFFLINE);
	sentry_options_set_http_retry(options, 1);

	std::string moduleDirectory;

	if (GetOwnModuleDirectory(moduleDirectory)) {
		const std::string daemonPath = moduleDirectory + "sentry-crash";

		sentry_options_set_handler_path(options, daemonPath.c_str());
	} else {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not resolve own module directory; falling back to the SDK's default sentry-crash lookup, which searches next to the host executable");
	}

	// Where the SDK queues envelopes and minidumps until they are sent.
	//
	// Left unset, sentry-native uses `.sentry-native` *relative to the
	// current working directory*, and a macOS app launched the way users
	// launch one -- Finder, Dock, `open` -- has a working directory of `/`.
	// That is the sealed system volume: `mkdir /.sentry-native` fails with
	// "Read-only file system" for root, never mind a normal user.
	// sentry__path_create_dir_all then fails, sentry_init() returns
	// non-zero, and crash reporting is off for the whole session:
	//
	//   Crash Handler: backend is Sentry
	//   Crash Handler: sentry_init() failed
	//
	// It looked fine in development only because a build launched from a
	// shell inherits that shell's writable working directory. Verified both
	// ways on 2026-08-17: via `open` it fails, and with the identical binary
	// started from a writable directory it logs "Sentry initialized".
	//
	// The plugin config directory is per-user, writable without elevation,
	// and already holds the rest of our state. Same choice as the Windows
	// handler, which hit the same bug with `Program Files` in place of `/`.
	char *databasePath = obs_module_config_path("sentry-db");

	if (databasePath) {
		sentry_options_set_database_path(options, databasePath);

		// Logged because a report that never arrives is diagnosed by
		// looking for a stranded envelope, and that is only possible if
		// the log says where to look.
		blog(LOG_INFO,
		     "obs-streamelements-core: StreamElements: Crash Handler: database path is %s",
		     databasePath);

		bfree(databasePath);
	} else {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not resolve the plugin config directory; falling back to the SDK's default database path, which is relative to the working directory and therefore unwritable under Finder");
	}

	// 60s, for the reason documented in the Windows handler: with the default
	// an observed crash wrote a complete minidump and then lost it when
	// teardown cancelled the upload in flight. macOS has not been seen to do
	// this, but the exposure is the same and the cost of waiting is a delay
	// in a process that is already terminating.
	sentry_options_set_shutdown_timeout(options, 60000);

	sentry_options_set_minidump_mode(options, SENTRY_MINIDUMP_MODE_SMART);

	sentry_options_set_crash_reporting_mode(
		options, SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP);

	sentry_options_set_on_crash(options, OnCrash, nullptr);

	if (sentry_init(options) != 0) {
		blog(LOG_ERROR,
		     "obs-streamelements-core: StreamElements: Crash Handler: sentry_init() failed");
		return;
	}

	auto config = StreamElementsConfig::GetInstance();

	if (config) {
		s_userName = config->GetCrashReportUserName();
		s_userEmail = config->GetCrashReportUserEmail();
		s_userDiscord = config->GetCrashReportUserDiscord();
	}

	SetSentryUser(s_userName, s_userEmail, s_userDiscord);

	// Resolved here, once, so the signal handler never has to build a path.
	{
		char configPath[512];

		if (os_get_config_path(
			    configPath, sizeof(configPath),
			    "obs-studio/plugin_config/obs-streamelements") >
		    0) {
			os_mkdirs(configPath);

			snprintf(s_pendingMarkerPath,
				 sizeof(s_pendingMarkerPath),
				 "%s/sentry-consent-pending", configPath);
		}
	}

	//
	// A crash from a previous run that we could not ask about at the time,
	// because it happened off the main thread. Its envelope is still in the
	// cache, unsent. We are on the main thread now, so ask.
	//
	// This is what keeps "could not ask" from meaning "silently dropped":
	// most crashes happen on worker threads, so without this the majority
	// of reports would never be offered to the user at all.
	//
	if (s_pendingMarkerPath[0] && os_file_exists(s_pendingMarkerPath)) {
		// The marker holds the id of the crash it stands for, so the
		// answer can be attached to that event rather than floating
		// free. Read before unlinking, obviously.
		char *markerContents =
			os_quick_read_utf8_file(s_pendingMarkerPath);

		sentry_uuid_t pendingEventId = sentry_uuid_nil();

		if (markerContents) {
			pendingEventId =
				sentry_uuid_from_string(markerContents);

			bfree(markerContents);
		}

		os_unlink(s_pendingMarkerPath);

		const auto consent = StreamElementsCrashConsentDialog::Prompt(
			s_userName, s_userEmail, s_userDiscord);

		if (consent.consented) {
			PersistContactDetails(consent.name, consent.email,
					      consent.discord);

			// Unlike the crash-time path, this one is a fresh
			// process: setting the user here is worth doing because
			// everything this session reports will carry it.
			SetSentryUser(consent.name, consent.email,
				      consent.discord);

			if (consent.description.size() &&
			    !sentry_uuid_is_nil(&pendingEventId)) {
				sentry_capture_feedback(
					sentry_value_new_feedback(
						consent.description.c_str(),
						consent.email.size()
							? consent.email.c_str()
							: nullptr,
						consent.name.size()
							? consent.name.c_str()
							: nullptr,
						&pendingEventId));
			}

			// Releases whatever the previous run left cached, and
			// then pushes it while we still hold consent -- the
			// revoke below is unconditional, so leaving this to the
			// background retry queue would race it and strand the
			// report. Shorter budget than the crash path: the user
			// is looking at a working OBS, not a dying one.
			StartCrashProgress(CRASH_PROGRESS_UPLOADING);

			sentry_user_consent_give();
			sentry_flush(30000);

			StopCrashProgress();
		}
	}

	//
	// Consent is per crash, never a standing preference. Unconditional, and
	// last, so it covers both branches above.
	//
	// sentry-native persists the answer to <database>/user-consent and
	// treats it as a global gate, which means a "yes" given for one crash
	// silently pre-authorises the next one: the daemon uploads the moment
	// the fault happens, before the dialog is on screen and regardless of
	// what the user then clicks. Observed exactly that -- a second crash
	// produced no "caching envelope" line at all, and the report was gone
	// before it could be declined or described.
	//
	// Resetting here means every crash is answered on its own terms, with
	// its own description, which is the only reading of "consent" that
	// means anything. Note revoking does not touch anything already in the
	// cache; it only closes the gate.
	//
	sentry_user_consent_revoke();

	// After sentry_init, and deliberately: constructing this fetches the
	// remote module-of-interest list over HTTP, which is not something to
	// do while a signal handler might already be arriving.
	s_crashContext = new StreamElementsCrashContext();

	s_initialized = true;

	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: Sentry initialized (%s)",
	     release.c_str());
}

void StreamElementsSentryCrashHandler::StopAsyncHangDetection()
{
	// There is no hang detection on macOS, matching the BugSplat handler.
}

StreamElementsSentryCrashHandler::~StreamElementsSentryCrashHandler()
{
	// Deliberately not calling sentry_close(), and deliberately leaving the
	// signal handlers installed: tearing the reporter down early loses
	// exceptions thrown during shutdown, which is when a fair number of them
	// happen. Matches the BugSplat handler on both platforms.
}
