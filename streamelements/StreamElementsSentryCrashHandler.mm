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

#include <signal.h>
#include <dlfcn.h>
#include <execinfo.h>
#include <mach-o/dyld.h>

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
	if (s_crashContext) {
		s_crashContext->WalkStack(nullptr);

		sentry_value_set_by_key(
			event, "selive_module_on_stack",
			sentry_value_new_bool(s_crashContext->ShouldReport()));

		ArmSentryScope(s_crashContext->Collect());
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
static void ChainedSignalHandler(int signum, siginfo_t *info, void *context)
{
	// One crash only. A fault raised while the prompt is up must not
	// re-enter this.
	if (s_handledCrash) {
		return;
	}

	s_handledCrash = 1;

	if (s_initialized) {
		const auto consent = StreamElementsCrashConsentDialog::Prompt(
			s_userName, s_userEmail, s_userDiscord);

		if (consent.consented) {
			PersistContactDetails(consent.name, consent.email,
					      consent.discord);

			SetSentryUser(consent.name, consent.email,
				      consent.discord);

			if (consent.description.size()) {
				sentry_value_t report =
					sentry_value_new_object();

				sentry_value_set_by_key(
					report, "description",
					sentry_value_new_string(
						consent.description.c_str()));

				sentry_set_context("user_report", report);
			}

			// Releases what the daemon already wrote. Until this
			// call the envelope sits in the cache unsent, which is
			// what makes "Don't send" mean it.
			sentry_user_consent_give();
		} else {
			sentry_user_consent_revoke();
		}
	}

	// Hand on to whoever held this signal before we did, so OBS's own
	// handling still runs.
	const size_t count = sizeof(s_signals) / sizeof(s_signals[0]);

	for (size_t i = 0; i < count; ++i) {
		if (s_signals[i] != signum)
			continue;

		struct sigaction *previous = &s_previousHandlers[i];

		if (previous->sa_flags & SA_SIGINFO) {
			if (previous->sa_sigaction)
				previous->sa_sigaction(signum, info, context);
		} else if (previous->sa_handler != SIG_DFL &&
			   previous->sa_handler != SIG_IGN) {
			previous->sa_handler(signum);
		}

		break;
	}
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
	// uploading blindly, so a refusal still stops the minidump. Envelopes
	// captured while consent is withheld are cached, and flushed if consent
	// is given later -- possibly not until the next launch, since a dying
	// process may not survive long enough to finish the upload.
	//
	sentry_options_set_require_user_consent(options, 1);

	std::string moduleDirectory;

	if (GetOwnModuleDirectory(moduleDirectory)) {
		const std::string daemonPath = moduleDirectory + "sentry-crash";

		sentry_options_set_handler_path(options, daemonPath.c_str());
	} else {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not resolve own module directory; falling back to the SDK's default sentry-crash lookup, which searches next to the host executable");
	}

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
