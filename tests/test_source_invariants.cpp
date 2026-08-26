// Source-invariant regression gate.
//
// Reads production source files and asserts that specific buggy
// patterns identified in the crash-bug review are absent. These checks
// run in milliseconds and require no OBS / Qt / CEF dependencies.
//
// Each invariant references the bug it guards against; if a future edit
// re-introduces the pattern, the test fails with a clear message.

#include "source_paths.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

static int failures = 0;

static std::string slurp(const std::string &relpath)
{
	std::string full = std::string(se_tests::kRepoRoot) + "/" + relpath;
	std::ifstream in(full);
	if (!in) {
		std::fprintf(stderr, "FATAL: cannot open %s\n", full.c_str());
		std::exit(2);
	}
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static std::size_t count_matches(const std::string &haystack,
				 const std::regex &re)
{
	auto begin = std::sregex_iterator(haystack.begin(), haystack.end(), re);
	auto end = std::sregex_iterator();
	return static_cast<std::size_t>(std::distance(begin, end));
}

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

// --- C2: IsMatchingIndex must compare with ==, not assign with =.
//
// The original line was:  return m_index = index;
// which assigns and returns truthy/falsy of the assigned value rather
// than comparing. The fix is:  return m_index == index;
static void check_c2_video_encoder_template_match()
{
	auto src = slurp("streamelements/StreamElementsOutput.hpp");

	// The buggy assignment form must be gone. Match `m_index` followed
	// by `=` with no neighbouring `=` (i.e. real assignment, not `==`).
	std::regex bad(R"(return\s+m_index\s*=\s*[^=])");
	check(count_matches(src, bad) == 0,
	      "C2: StreamElementsOutput.hpp must not assign to m_index in IsMatchingIndex "
	      "(should be `return m_index == index;`)");

	// And the correct comparison form must be present.
	std::regex good(R"(return\s+m_index\s*==\s*index\s*;)");
	check(count_matches(src, good) >= 1,
	      "C2: StreamElementsOutput.hpp must contain `return m_index == index;`");
}

// --- C4: self-assignment of cefClientId in batchInvokeSeries leaves
// the field holding uninitialised heap memory which is then routed back
// to CEF clients as a callback id.
static void check_c4_no_self_assign_cefclientid()
{
	auto src = slurp("streamelements/StreamElementsApiMessageHandler.cpp");

	std::regex bad(
		R"(context\s*->\s*cefClientId\s*=\s*context\s*->\s*cefClientId)");
	check(count_matches(src, bad) == 0,
	      "C4: StreamElementsApiMessageHandler.cpp must not contain "
	      "`context->cefClientId = context->cefClientId` (self-assign)");
}

// --- C5: CefParseJSON returns null on malformed input. The result must
// not be passed straight into CefListValue::SetValue without a null
// check, because that dereferences a null CefRefPtr.
static void check_c5_cefparsejson_null_guarded()
{
	auto src = slurp("streamelements/StreamElementsApiMessageHandler.cpp");

	// The fix guards parsedValue against null before SetValue.
	// We assert one of two acceptable patterns:
	//   (a) an `if (!parsedValue ...) ...` near a CefParseJSON call, or
	//   (b) the parsed value is replaced with a default null CefValue.
	std::regex parseCall(R"(CefParseJSON\s*\()");
	check(count_matches(src, parseCall) >= 1,
	      "C5: expected at least one CefParseJSON call site to validate");

	// Acceptable guard: a null check on parsedValue between the
	// CefParseJSON call and the SetValue call.
	std::regex guarded(
		R"(CefParseJSON[\s\S]{0,400}?if\s*\(\s*!\s*parsedValue\b)");
	check(count_matches(src, guarded) >= 1,
	      "C5: CefParseJSON result must be null-checked before being passed to SetValue");
}

// --- C6: audio encoder bounds check must use `>=`, not `>`. The array
// has MAX_AUDIO_MIXES slots, valid indices are [0, MAX_AUDIO_MIXES).
static void check_c6_audio_encoder_bounds()
{
	auto src = slurp("streamelements/StreamElementsAudioComposition.cpp");

	std::regex bad(R"(if\s*\(\s*index\s*>\s*MAX_AUDIO_MIXES\s*\))");
	check(count_matches(src, bad) == 0,
	      "C6: audio encoder bound check must use `>=`, not `>` "
	      "(`if (index > MAX_AUDIO_MIXES)` allows one-past-end access)");

	std::regex good(R"(if\s*\(\s*index\s*>=\s*MAX_AUDIO_MIXES\s*\))");
	check(count_matches(src, good) >= 2,
	      "C6: expected `if (index >= MAX_AUDIO_MIXES)` in both streaming "
	      "and recording audio encoder accessors");
}

// --- C10: the `setCurrentProfile` API handler was registered twice, so
// the first registration was silently overwritten. Assert it now
// appears exactly once.
static void check_c10_no_duplicate_handler_setcurrentprofile()
{
	auto src = slurp("streamelements/StreamElementsApiMessageHandler.cpp");

	std::regex reg(R"(API_HANDLER_BEGIN\(\"setCurrentProfile\"\))");
	std::size_t count = count_matches(src, reg);
	if (count != 1) {
		std::fprintf(
			stderr,
			"FAIL: C10: setCurrentProfile is registered %zu time(s); expected exactly 1\n",
			count);
		++failures;
	}
}

// --- Menu manager must not be dereferenced unguarded in Initialize().
//
// Initialize() calls QApplication::sendPostedEvents() a few lines above,
// which runs deferred deletes, frontend callbacks and any modal dialog's own
// event loop while Initialize() is still on the stack. Observed in the field:
// OBS held OBSInit open in its modal update dialog for minutes, and by the
// time the next line ran m_menuManager was null. UpdateInternal then faulted
// on a null `this` at its own `if (!m_menu)` guard -- SYNC_ACCESS() locks a
// static mutex and touches no member, so that guard is the first read through
// `this`, which makes the null check the crash site instead of the
// protection.
//
// Minidump, pid 47436:
//   UpdateInternal+0x54  mov rcx,[rsi+18h]  rsi=0  ->  read of 0x18
static void check_menu_manager_update_guarded()
{
	auto src = slurp("streamelements/StreamElementsGlobalStateManager.cpp");

	// Every occurrence must be the guarded one. Counting both forms and
	// comparing is what distinguishes them: the call sits on its own line
	// under the guard, so a pattern anchored on the call alone matches the
	// guarded form too and proves nothing.
	std::regex any_call(R"(m_menuManager->Update\(\);)");
	std::regex guarded(
		R"(if \(m_menuManager\)[\s]*m_menuManager->Update\(\);)");

	std::size_t total = count_matches(src, any_call);
	std::size_t safe = count_matches(src, guarded);

	check(total > 0,
	      "Initialize() must still call m_menuManager->Update(); the invariant cannot be satisfied by deleting the call");

	if (total != safe) {
		std::fprintf(
			stderr,
			"FAIL: %zu of %zu m_menuManager->Update() call(s) lack an if (m_menuManager) guard\n",
			total - safe, total);
		++failures;
	}
}

// --- CORE-777: ~StreamElementsWidgetManager must not drain the Qt event
// queue while tearing down its dock widgets.
//
// The destructor used to call QApplication::sendPostedEvents() between
// QMainWindow::removeDockWidget() and delete. That dispatches any
// DeferredDelete already posted for the very widget about to be deleted,
// so the delete lands on an already-destructed object and Qt aborts in
// _purecall -- ~QObject deletes its QObjectData through a pure virtual
// destructor, and on the second pass the vtable has degraded to
// QObjectData, whose slot holds _purecall.
//
// Draining is unnecessary: ~QObject discards events posted to the object
// being destroyed.
static std::string strip_line_comments(const std::string &src)
{
	std::string out;
	out.reserve(src.size());

	for (std::size_t i = 0; i < src.size();) {
		if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
			while (i < src.size() && src[i] != '\n')
				++i;
		} else {
			out.push_back(src[i]);
			++i;
		}
	}

	return out;
}

static void check_widget_manager_dtor_does_not_drain_events()
{
	auto src = slurp("streamelements/StreamElementsWidgetManager.cpp");

	// Isolate the destructor body: from its signature to the first
	// closing brace in column 0. Scanning the whole file would trip over
	// the deliberate drains in PushCentralWidget() and friends.
	const std::string sig =
		"StreamElementsWidgetManager::~StreamElementsWidgetManager()";

	auto start = src.find(sig);
	check(start != std::string::npos,
	      "CORE-777: ~StreamElementsWidgetManager() not found -- update this invariant");
	if (start == std::string::npos)
		return;

	auto end = src.find("\n}", start);
	check(end != std::string::npos,
	      "CORE-777: could not find the end of ~StreamElementsWidgetManager()");
	if (end == std::string::npos)
		return;

	std::string body = src.substr(start, end - start);

	// Comments are stripped first: the fix leaves a comment naming the
	// call it must not make, and that must not satisfy the check either
	// way round.
	std::string code = strip_line_comments(body);

	check(code.find("sendPostedEvents") == std::string::npos,
	      "CORE-777: ~StreamElementsWidgetManager() must not call QApplication::sendPostedEvents() -- it turns a pending deleteLater() into a double delete");

	// And the widget must be reached through a QPointer, so a dock
	// destroyed behind our back reads back as null rather than dangling.
	check(code.find("QPointer<QDockWidget>") != std::string::npos,
	      "CORE-777: ~StreamElementsWidgetManager() must hold each dock widget in a QPointer before deleting it");
}

// --- CORE-786: the widget maps must hold QPointer, not raw pointers.
//
// Docks are children of the OBS main window (addDockWidget), and browser
// widgets are children of their dock. Qt owns both and destroys them with
// their parent -- which, on the OBSInit re-entrancy path, happens before
// ~StreamElementsWidgetManager runs. Raw pointers in these maps went stale
// and were then deleted a second time.
//
// Guarding at the point of use is not enough and was the gap in the first
// version of this fix: a QPointer constructed from an already-dangling raw
// pointer is born non-null. The map itself has to hold the QPointer.
static void check_widget_maps_hold_qpointer()
{
	auto wm = strip_line_comments(
		slurp("streamelements/StreamElementsWidgetManager.hpp"));

	check(wm.find("std::map<std::string, QPointer<QDockWidget>>") !=
		      std::string::npos,
	      "CORE-786: m_dockWidgets must be std::map<std::string, QPointer<QDockWidget>>");
	check(wm.find("std::map<std::string, QDockWidget*>") ==
		      std::string::npos,
	      "CORE-786: m_dockWidgets must not hold raw QDockWidget*");

	auto bwm = strip_line_comments(
		slurp("streamelements/StreamElementsBrowserWidgetManager.hpp"));

	check(bwm.find("QPointer<StreamElementsBrowserWidget>>") !=
		      std::string::npos,
	      "CORE-786: m_browserWidgets must hold QPointer<StreamElementsBrowserWidget>");
	check(bwm.find("std::map<std::string, StreamElementsBrowserWidget*>") ==
		      std::string::npos,
	      "CORE-786: m_browserWidgets must not hold raw StreamElementsBrowserWidget*");

	// The composition view widgets are hand-deleted, so they must be
	// QPointer too or the delete can run twice.
	auto bw = strip_line_comments(
		slurp("streamelements/StreamElementsBrowserWidget.hpp"));

	check(bw.find("QPointer<QWidget> m_activeVideoCompositionViewWidgetContainer") !=
		      std::string::npos,
	      "CORE-786: m_activeVideoCompositionViewWidgetContainer must be a QPointer");
	check(bw.find("QPointer<StreamElementsVideoCompositionViewWidget>") !=
		      std::string::npos,
	      "CORE-786: m_activeVideoCompositionViewWidget must be a QPointer");
}

// --- CORE-786: every event pump must go through SEDrainEventQueue().
//
// One choke point for every event pump the plug-in runs. A bare
// QApplication::sendPostedEvents() is how OBS's modal update dialog and its
// queued close() get dispatched from inside our own Initialize().
//
// IsObsInitFinished() distinguishes OBS's own event loop from a nested one by
// counting the pumps we run ourselves. A bare QApplication::sendPostedEvents()
// anywhere in the plugin is invisible to that counter, so the gate could open
// while OBSInit() is still on the stack -- which is the exact condition that
// corrupts the widget tree. The wrapper in StreamElementsUtils.cpp is the only
// legitimate caller.
static void check_event_pumps_are_counted()
{
	static const char *const kSources[] = {
		"streamelements/StreamElementsBrowserWidgetManager.cpp",
		"streamelements/StreamElementsGlobalStateManager.cpp",
		"streamelements/StreamElementsNativeOBSControlsManager.cpp",
		"streamelements/StreamElementsWidgetManager.cpp",
		"streamelements/StreamElementsWorkerManager.cpp",
		"streamelements/StreamElementsReportIssueDialog.cpp",
	};

	for (const char *const relpath : kSources) {
		auto code = strip_line_comments(slurp(relpath));

		if (code.find("sendPostedEvents") != std::string::npos) {
			std::fprintf(
				stderr,
				"FAIL: CORE-786: %s calls sendPostedEvents() directly; use SEDrainEventQueue() so the pump is counted\n",
				relpath);
			++failures;
		}
	}

	// And the wrapper itself must still contain exactly one real pump.
	auto utils = strip_line_comments(
		slurp("streamelements/StreamElementsUtils.cpp"));

	std::regex pump(R"(QApplication::sendPostedEvents\(\);)");

	check(count_matches(utils, pump) == 1,
	      "CORE-786: StreamElementsUtils.cpp must contain exactly one QApplication::sendPostedEvents(), inside SEDrainEventQueue()");

	check(utils.find("void SEDrainEventQueue()") != std::string::npos,
	      "CORE-786: SEDrainEventQueue() must exist in StreamElementsUtils.cpp");

	// The pump must stop once the close has been caught: every further
	// dispatch feeds events into a world that is being torn down.
	auto pump_at = utils.find("void SEDrainEventQueue()");
	auto pump_end = utils.find("\n}", pump_at);
	auto pump_body = utils.substr(pump_at, pump_end - pump_at);

	check(pump_body.find("SEIsEventPumpAllowed()") != std::string::npos,
	      "CORE-786: SEDrainEventQueue() must gate on SEIsEventPumpAllowed()");
}

// --- CORE-786: dock widgets must not be deleted unguarded.
static void check_dock_deletion_is_gated()
{
	// Every dock teardown site must go through the shared helper.
	static const char *const kSources[] = {
		"streamelements/StreamElementsWidgetManager.cpp",
		"streamelements/StreamElementsWorkerManager.cpp",
	};

	for (const char *const relpath : kSources) {
		auto s = strip_line_comments(slurp(relpath));

		if (s.find("SEDeleteDockWidgetWhenSafe") == std::string::npos) {
			std::fprintf(
				stderr,
				"FAIL: CORE-786: %s must destroy dock widgets through SEDeleteDockWidgetWhenSafe()\n",
				relpath);
			++failures;
		}
	}

	auto utils = strip_line_comments(
		slurp("streamelements/StreamElementsUtils.cpp"));

	check(utils.find("SEIsUiTeardownSafe()") != std::string::npos,
	      "CORE-786: SEDeleteDockWidgetWhenSafe() must gate on SEIsUiTeardownSafe()");

	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsWidgetManager.cpp"));

	// The two raw deletion forms are legitimate inside the helper and
	// nowhere else, so cut the helper out before looking for them.
	check(code.find("delete dock.data();") == std::string::npos,
	      "CORE-786: bare `delete dock.data();` outside SafeDeleteDockWidget()");
	check(code.find("dock->deleteLater();") == std::string::npos,
	      "CORE-786: bare `dock->deleteLater();` outside SafeDeleteDockWidget()");
}

// --- CORE-786: the plug-in must watch the OBS main window for close.
//
// OBSBasic::OBSInit() starts AutoUpdateThread before it emits
// FINISHED_LOADING, and that thread ends with a queued close() on the main
// window. If the close lands before Initialize() finishes, our object graph is
// half-built and must not be torn down. QEvent::Close on the main window is
// the signal; it is delivered before OBSBasic::closeEvent(), which is what
// fires EXIT.
static void check_obs_close_is_watched()
{
	auto code = strip_line_comments(
		slurp("obs-streamelements-core-plugin.cpp"));

	check(code.find("QEvent::Close") != std::string::npos,
	      "CORE-786: the plug-in must watch for QEvent::Close on the OBS main window");

	check(code.find("installEventFilter") != std::string::npos,
	      "CORE-786: the close watcher must be installed as an event filter");

	check(code.find("bool SEIsUiTeardownSafe()") != std::string::npos,
	      "CORE-786: SEIsUiTeardownSafe() must be defined in the plug-in entry point");

	check(code.find("SENoteInitializeCompleted()") != std::string::npos,
	      "CORE-786: SENoteInitializeCompleted() must be defined in the plug-in entry point");

	// Pumping the event queue anywhere under Initialize() is what nests
	// OBS's EXIT dispatch inside its FINISHED_LOADING dispatch, so the
	// whole call has to run with pumping disabled.
	check(code.find("bool SEIsEventPumpAllowed()") != std::string::npos,
	      "CORE-786: SEIsEventPumpAllowed() must be defined in the plug-in entry point");

	check(code.find("s_initializeInProgress") != std::string::npos,
	      "CORE-786: an in-progress flag must cover the whole of Initialize()");

	check(code.find("StreamElementsInitializeScope initializeScope;") !=
		      std::string::npos,
	      "CORE-786: Initialize() must be called inside StreamElementsInitializeScope");

	// The gate has to actually be consulted on the teardown path, and the
	// unsafe branch must leak rather than destroy.
	check(code.find("StreamElementsGlobalStateManager::Leak()") !=
		      std::string::npos,
	      "CORE-786: the EXIT path must leak rather than tear down when teardown is unsafe");

	// Removing our callback mutates the vector OBSStudioAPI::on_event() is
	// iterating. Harmless normally, fatal when EXIT is dispatched from
	// inside another on_event(), so it must be gated.
	auto rm = code.find("obs_frontend_remove_event_callback(");
	check(rm != std::string::npos,
	      "CORE-786: EXIT-path callback removal not found -- update this invariant");
	if (rm != std::string::npos) {
		auto before = code.rfind("SEIsUiTeardownSafe()", rm);
		check(before != std::string::npos && rm - before < 200,
		      "CORE-786: obs_frontend_remove_event_callback() must be gated on SEIsUiTeardownSafe()");
	}

	// The filter must never swallow the event -- OBS has to close exactly
	// as it would without us.
	check(code.find("return QObject::eventFilter(watched, event);") !=
		      std::string::npos,
	      "CORE-786: the close watcher must not consume events");

	// And Initialize() must record completion, or the gate never opens.
	auto gsm = strip_line_comments(
		slurp("streamelements/StreamElementsGlobalStateManager.cpp"));

	check(gsm.find("SENoteInitializeCompleted();") != std::string::npos,
	      "CORE-786: Initialize() must call SENoteInitializeCompleted() on completion");
}

// --- CORE-860: both of sentry's crash entry points must be ours.
//
// sentry_init() installs a top-level SEH filter AND a signal(SIGABRT, ...)
// handler. Owning only the first sent every abort() -- every _purecall, so the
// whole double-destruction family -- straight to Sentry with no consent prompt,
// no module-of-interest gate and no payload.
static void check_both_sentry_doors_are_owned()
{
	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsSentryCrashHandler.cpp"));

	check(code.find("SetUnhandledExceptionFilter(SentryExceptionFilter)") !=
		      std::string::npos,
	      "CORE-860: the SEH filter must still be installed");

	check(code.find("signal(SIGABRT,") != std::string::npos,
	      "CORE-860: the SIGABRT door must be taken too, or abort() bypasses the gate and the consent prompt");

	check(code.find("_set_purecall_handler(") != std::string::npos,
	      "CORE-860: a pure virtual call must be identified as such, not arrive as an anonymous abort");

	// Both doors must funnel into the same path, or the gate and the
	// consent prompt exist on only one of them.
	check(count_matches(code, std::regex("HandleFatalException\\(")) >= 3,
	      "CORE-860: both entry points must call the shared HandleFatalException()");

	// The SEH path takes its CONTEXT from the OS at the fault point; the
	// abort path captures its own, from inside our handler. Only the latter
	// may drop leading frames -- see check_abort_path_drops_own_frames.
	check(code.find("HandleFatalException(pExceptionInfo, false)") !=
		      std::string::npos,
	      "CORE-860: the SEH path must NOT skip leading frames");

	check(code.find("HandleFatalException(&pointers, true)") !=
		      std::string::npos,
	      "CORE-860: the abort path MUST skip its own leading frames");

	// Identity has to survive a future bypass of the crash path.
	check(code.find("ArmStableSentryTags();") != std::string::npos,
	      "CORE-860: the stable identity tags must be armed at init, not only on the crash path");
}

// --- CORE-860: the abort path must not rubber-stamp the gate.
//
// The SIGABRT handler captures its own CONTEXT, so this plug-in is on the stack
// of every abort in the process. Without dropping those leading frames, the
// module-of-interest verdict is true for everyone's crashes.
static void check_abort_path_drops_own_frames()
{
	auto hpp = strip_line_comments(
		slurp("streamelements/StreamElementsCrashContext.hpp"));

	check(hpp.find("bool skipOwnLeadingFrames") != std::string::npos,
	      "CORE-860: WalkStack() must offer the leading-frame skip");

	auto cpp = strip_line_comments(
		slurp("streamelements/StreamElementsCrashContext.cpp"));

	check(cpp.find("SetSkipLeadingOwnFrames(skipOwnLeadingFrames)") !=
		      std::string::npos,
	      "CORE-860: WalkStack() must pass the skip flag through to the walker");

	// The skip must run before the frame is recorded and before the verdict
	// is taken, and must stop at the first frame that is not ours.
	auto skip = cpp.find("if (m_skippingOwnFrames) {");
	check(skip != std::string::npos,
	      "CORE-860: the walker must implement the leading-frame skip");

	if (skip != std::string::npos) {
		auto verdict = cpp.find("hasMatchModuleOfInterest =", skip);
		check(verdict != std::string::npos && verdict > skip,
		      "CORE-860: the skip must be applied before the module-of-interest verdict");

		auto stops = cpp.find("m_skippingOwnFrames = false;", skip);
		check(stops != std::string::npos && stops - skip < 200,
		      "CORE-860: skipping must stop at the first frame that is not ours");
	}
}

// --- CORE-861: a failing sentry_init() must say why, and must not depend on
// the process working directory.
//
// Every way sentry_init() can fail logs through SENTRY_WARN, which is compiled
// in but discarded unless options->debug is set. Without the logger installed,
// an install with no crash reporting at all produced one line -- "sentry_init()
// failed" -- and no way to find out why.
//
// Separately, obs_module_config_path() is relative on a portable install, and a
// relative path handed to sentry is resolved against the process working
// directory, which no plug-in controls.
static void check_sentry_init_is_diagnosable()
{
	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsSentryCrashHandler.cpp"));

	check(code.find("sentry_options_set_logger(") != std::string::npos,
	      "CORE-861: sentry's own diagnostics must be routed into the OBS log");

	// The logger is inert unless debug is enabled -- that is the whole
	// reason the failures were invisible.
	check(code.find("sentry_options_set_debug(options, 1)") !=
		      std::string::npos,
	      "CORE-861: sentry debug must be on, or the logger above is never called");

	// ...but the default level must stay above DEBUG, or every start-up
	// writes a wall of SDK chatter into the user's log.
	check(code.find("sentry_options_set_logger_level(") !=
		      std::string::npos,
	      "CORE-861: the logger level must be set, or the default DEBUG stream floods the log");

	check(code.find("SENTRY_LEVEL_WARNING") != std::string::npos,
	      "CORE-861: the default logger level must be WARNING so failures still explain themselves");

	// The database path must not be left relative.
	//
	// Anchored on the call expression, not on the name: the function's own
	// definition contains the name too, so a bare find() still matches
	// after the call site is removed and proves nothing.
	std::regex resolvesConfigPath(
		R"(ResolveAgainstHostExecutable\(\s*utf8_to_wstring\(databasePath\)\s*\))");
	check(count_matches(code, resolvesConfigPath) == 1,
	      "CORE-861: the config path handed to sentry must go through ResolveAgainstHostExecutable()");

	// The resolved value, not the raw one, is what sentry must receive.
	std::regex setsResolved(
		R"(sentry_options_set_database_pathw\(\s*options,\s*resolved\.c_str\(\))");
	check(count_matches(code, setsResolved) == 1,
	      "CORE-861: sentry_options_set_database_pathw() must be given the resolved path");

	// And the MAX_PATH ceiling must be reported rather than left as a silent
	// absence of crash reports. Anchored on the emitted message, so that
	// deleting the blog() is what fails -- renaming the constant is not.
	std::regex warnsOnLimit(
		R"(blog\(LOG_WARNING[\s\S]{0,400}?MAX_PATH limit of)");
	check(count_matches(code, warnsOnLimit) >= 1,
	      "CORE-861: a database path too close to MAX_PATH must be warned about explicitly");

	check(code.find(
		      "resolved.size() + kLongestSentryChildPath >= MAX_PATH") !=
		      std::string::npos,
	      "CORE-861: the MAX_PATH check must account for the files sentry creates inside the database directory, not just the directory itself");
}

// --- CORE-862: queued Qt tasks must not run once crash reporting has begun.
//
// The consent prompt is a native Win32 modal dialog, and every Win32 modal loop
// dispatches the private message Qt's event dispatcher uses to drain its
// posted-event queue. So putting the prompt up runs whatever was queued -- and a
// task that calls QDialog::exec() blocks the crash path behind an unrelated
// modal dialog.
static void check_queued_tasks_suppressed_during_crash()
{
	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsUtils.cpp"));

	// Both queued paths -- QtPostTask's executor and QtDelayTask's timer
	// lambda -- must skip the task AND release the waiter. Dropping a task
	// without calling finish() does not remove the deadlock, it moves it
	// onto whichever thread called QtExecSync and is blocked in
	// result.wait().
	std::regex gateReleasesWaiter(
		R"(if \(IsCrashReportingInProgress\(\)\)\s*\{\s*finish\(\);\s*return;)");
	check(count_matches(code, gateReleasesWaiter) == 2,
	      "CORE-862: both queued paths must call finish() before returning, or a QtExecSync caller is left blocked forever");

	// The same-thread QtExecSync shortcut bypasses the queue entirely and
	// runs the task inline, so it needs its own gate -- and the crashing
	// thread is usually this one. Negated form, hence a separate check.
	std::regex execSyncGated(
		R"(if \(!IsCrashReportingInProgress\(\)\)\s*task\(\);)");
	check(count_matches(code, execSyncGated) == 1,
	      "CORE-862: the QtExecSync same-thread path must be gated too -- it never touches the queue");

	// A dropped task must not be reported as having run: the gate has to sit
	// above the running flag, not below it.
	auto gate = code.find("if (IsCrashReportingInProgress())");
	auto running = code.find("item->running = true;");
	check(gate != std::string::npos && running != std::string::npos &&
		      gate < running,
	      "CORE-862: the crash gate must precede item->running, or dropped tasks appear as running in async-context.json");
}

// --- CORE-863: the allocation door must be owned, chained, and not seized.
//
// BugSplat installed five process-global CRT hooks; the Sentry migration
// dropped all five and CORE-860 recovered two. This is the allocation one.
//
// The design constraint is as important as the hook itself: BugSplat's
// memory_depleted() force-crashes on any allocation failure, which would
// preempt libobs's own bmalloc -> bcrash handling. Ours observes and chains.
static void check_oom_handler_observes_and_chains()
{
	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsSentryCrashHandler.cpp"));

	check(code.find("_set_new_handler(SentryNewHandler)") !=
		      std::string::npos,
	      "CORE-863: the allocation door must be taken, or an OOM is reported as a generic abort at best");

	// malloc failures must route through it too -- libobs allocates through
	// bmalloc -> malloc, which is the largest allocator in the process.
	check(code.find("_set_new_mode(1)") != std::string::npos,
	      "CORE-863: _set_new_mode(1) must route malloc failures through the handler as well");

	// The displaced handler must be kept and called: an upstream handler
	// that can free memory and ask for a retry has to still win.
	check(code.find("s_previousNewHandler = _set_new_handler(") !=
		      std::string::npos,
	      "CORE-863: the displaced new handler must be captured for chaining");

	std::regex chains(
		R"(if \(s_previousNewHandler\)\s*return s_previousNewHandler\(size\);)");
	check(count_matches(code, chains) == 1,
	      "CORE-863: the handler must chain and pass the upstream answer through, not swallow a retry request");

	// And it must otherwise return 0 -- that is what preserves standard
	// semantics (bad_alloc thrown, malloc returns NULL) rather than seizing
	// the host's policy the way BugSplat's terminator() did.
	auto handler = code.find("int __cdecl SentryNewHandler(size_t size)");
	check(handler != std::string::npos,
	      "CORE-863: SentryNewHandler not found -- update this invariant");

	if (handler != std::string::npos) {
		auto body = code.substr(handler, 700);

		check(body.find("return 0;") != std::string::npos,
		      "CORE-863: the handler must return 0 when nothing upstream can help, so operator new still throws and malloc still returns NULL");

		check(body.find("terminator") == std::string::npos &&
			      body.find("TerminateProcess") ==
				      std::string::npos,
		      "CORE-863: the handler must NOT force-crash the process -- that would preempt libobs's own bmalloc failure handling");

		// The guard buffer exists for exactly this moment.
		check(body.find("ReleaseGuardBuffer();") != std::string::npos,
		      "CORE-863: the new handler must hand the guard buffer back at the moment memory ran out");
	}

	// crash.kind precedence: a sticky, inferred OOM flag must not relabel a
	// definite, proximate purecall.
	std::regex kindPrecedence(
		R"(s_abortIsPurecall\s*\?\s*"purecall"\s*:\s*s_sawOutOfMemory\s*\?\s*"oom"\s*:\s*fromAbortDoor\s*\?\s*"abort"\s*:\s*"exception")");
	check(count_matches(code, kindPrecedence) == 1,
	      "CORE-863: crash.kind must rank purecall above the sticky OOM flag, OOM above a bare abort, and distinguish the two doors");

	// And it must be set on the SHARED path, not in the abort handler.
	// An uncaught std::bad_alloc never reaches abort() on MSVC -- `throw`
	// raises a real SEH exception (0xE06D7363), so it lands in the filter.
	// Tagging only in the abort handler left the OOM this exists for
	// untagged, which a test run caught.
	auto shared = code.find(
		"static LONG HandleFatalException(PEXCEPTION_POINTERS pExceptionInfo,");
	auto tag = code.find("sentry_set_tag(\"crash.kind\", kind);");
	auto abortDoor = code.find("static void __cdecl SentryAbortHandler(");

	check(shared != std::string::npos && tag != std::string::npos &&
		      abortDoor != std::string::npos,
	      "CORE-863: crash.kind wiring not found -- update this invariant");

	if (shared != std::string::npos && tag != std::string::npos &&
	    abortDoor != std::string::npos) {
		check(tag > shared && tag < abortDoor,
		      "CORE-863: crash.kind must be set on the shared crash path, not only in the abort handler -- an uncaught bad_alloc arrives through the SEH filter");
	}
}

// --- CORE-863: the guard buffer must be released before anything allocates.
//
// It is reserved so that collection has headroom on an exhausted process. It
// used to be handed back on the first line of Collect(), which runs after the
// stack walk and after the consent prompt -- both of which allocate.
static void check_guard_buffer_released_first()
{
	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsSentryCrashHandler.cpp"));

	auto entry = code.find(
		"static LONG HandleFatalException(PEXCEPTION_POINTERS pExceptionInfo,");
	check(entry != std::string::npos,
	      "CORE-863: HandleFatalException not found -- update this invariant");

	if (entry == std::string::npos)
		return;

	auto release = code.find(
		"StreamElementsCrashContext::ReleaseGuardBuffer();", entry);
	auto walk = code.find("WalkStack(", entry);

	check(release != std::string::npos,
	      "CORE-863: the crash path must release the guard buffer");
	check(walk != std::string::npos,
	      "CORE-863: WalkStack call not found -- update this invariant");

	if (release != std::string::npos && walk != std::string::npos) {
		check(release < walk,
		      "CORE-863: the guard buffer must be released BEFORE the stack walk, which allocates");
	}
}

// --- CORE-862: the consent prompt must be bounded.
//
// The choke point in __QtPostTask_Impl stops OUR queued work from opening a
// modal dialog inside the prompt. It cannot stop OBS's own posted calls or
// another plug-in's, and nothing can -- a nested modal loop is above us on the
// stack and no other thread can unwind it. So the prompt needs a deadline, or a
// crashed process can sit there indefinitely holding the user's machine.
static void check_consent_prompt_is_bounded()
{
	auto code = strip_line_comments(
		slurp("streamelements/StreamElementsSentryCrashHandler.cpp"));

	check(code.find("CRASH_PROMPT_DEADLINE_MS") != std::string::npos,
	      "CORE-862: the consent prompt must have a deadline");

	// Armed immediately before the prompt and released immediately after --
	// scoped to the prompt alone, because everything after it is already
	// bounded and a watchdog spanning the upload would have to outlast it.
	auto arm = code.find("ArmPromptWatchdog();");
	auto prompt = code.find("StreamElementsCrashConsentDialog::Prompt(");
	auto disarm = code.find("DisarmPromptWatchdog();");

	check(arm != std::string::npos && prompt != std::string::npos &&
		      disarm != std::string::npos,
	      "CORE-862: prompt watchdog wiring not found -- update this invariant");

	if (arm != std::string::npos && prompt != std::string::npos &&
	    disarm != std::string::npos) {
		check(arm < prompt,
		      "CORE-862: the watchdog must be armed BEFORE the prompt, or it cannot bound it");
		check(disarm > prompt,
		      "CORE-862: the watchdog must be released AFTER the prompt returns");
	}

	// TerminateProcess, not exit() and not abort(): both run code on the way
	// out, and abort() would re-enter our own SIGABRT handler.
	auto watchdog = code.find("PromptWatchdogThreadProc");
	check(watchdog != std::string::npos,
	      "CORE-862: watchdog thread proc not found -- update this invariant");

	if (watchdog != std::string::npos) {
		auto body = code.substr(watchdog, 900);

		check(body.find("TerminateProcess") != std::string::npos,
		      "CORE-862: the watchdog must terminate the process when the deadline passes");
		check(body.find("abort()") == std::string::npos &&
			      body.find("exit(") == std::string::npos,
		      "CORE-862: the watchdog must not use abort() or exit() -- abort() re-enters our own SIGABRT handler");
	}
}

int main()
{
	check_c2_video_encoder_template_match();
	check_c4_no_self_assign_cefclientid();
	check_c5_cefparsejson_null_guarded();
	check_c6_audio_encoder_bounds();
	check_c10_no_duplicate_handler_setcurrentprofile();
	check_menu_manager_update_guarded();
	check_widget_manager_dtor_does_not_drain_events();
	check_widget_maps_hold_qpointer();
	check_event_pumps_are_counted();
	check_dock_deletion_is_gated();
	check_obs_close_is_watched();
	check_both_sentry_doors_are_owned();
	check_abort_path_drops_own_frames();
	check_sentry_init_is_diagnosable();
	check_queued_tasks_suppressed_during_crash();
	check_oom_handler_observes_and_chains();
	check_guard_buffer_released_first();
	check_consent_prompt_is_bounded();

	if (failures) {
		std::fprintf(stderr, "%d source invariant(s) violated\n",
			     failures);
		return 1;
	}
	std::puts("test_source_invariants: all invariants hold");
	return 0;
}
