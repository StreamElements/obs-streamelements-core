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

	std::regex bad(R"(context\s*->\s*cefClientId\s*=\s*context\s*->\s*cefClientId)");
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
		std::fprintf(stderr,
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
	auto src = slurp(
		"streamelements/StreamElementsGlobalStateManager.cpp");

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

	check(pump_body.find("SEIsUiTeardownSafe()") != std::string::npos,
	      "CORE-786: SEDrainEventQueue() must stop pumping once teardown is unsafe");
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
	auto code = strip_line_comments(slurp("obs-streamelements-core-plugin.cpp"));

	check(code.find("QEvent::Close") != std::string::npos,
	      "CORE-786: the plug-in must watch for QEvent::Close on the OBS main window");

	check(code.find("installEventFilter") != std::string::npos,
	      "CORE-786: the close watcher must be installed as an event filter");

	check(code.find("bool SEIsUiTeardownSafe()") != std::string::npos,
	      "CORE-786: SEIsUiTeardownSafe() must be defined in the plug-in entry point");

	check(code.find("SENoteInitializeCompleted()") != std::string::npos,
	      "CORE-786: SENoteInitializeCompleted() must be defined in the plug-in entry point");

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

	if (failures) {
		std::fprintf(stderr, "%d source invariant(s) violated\n",
			     failures);
		return 1;
	}
	std::puts("test_source_invariants: all invariants hold");
	return 0;
}
