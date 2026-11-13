// CORE-860: the module-of-interest gate on the abort()/SIGABRT path.
//
// Background. sentry_init() installs two crash entry points on Windows: the
// top-level SEH filter, and a plain signal(SIGABRT, ...). We used to own only
// the first, so every abort() -- and therefore every _purecall, which is the
// whole double-destruction family -- went straight to Sentry with no consent
// prompt, no gate and no payload.
//
// Taking the second door introduces a hazard that does not exist on the first.
// An SEH crash gets its CONTEXT from the OS at the fault point, so our handler
// is not in it. A SIGABRT has no context, so we capture one ourselves with
// RtlCaptureContext -- from inside our own handler. That puts this plug-in on
// the stack of EVERY abort in the process, including OBS's and other plugins'.
// Fed to the gate unchanged, `hasMatchModuleOfInterest` would be true every
// time: not a gate, a rubber stamp, and every abort in OBS would raise a
// consent prompt in our name and upload someone else's crash.
//
// The fix is to drop the leading frames that belong to us before anything is
// recorded or tested, and to stop dropping at the first frame that is not ours.
//
// This mirrors MyStackWalker's frame filter from SECrashContext.cpp
// -- the real one derives from StackWalker and is entangled with CEF, HTTP and
// dbghelp, none of which belong in a unit test. The logic under test is the
// twenty lines of frame filtering, reproduced exactly.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

// Mirror of MyStackWalker (Windows branch) in SECrashContext.cpp.
struct Walker {
	std::vector<std::string> modulesOfInterest = {"obs-streamelements-core",
						      "obs-streamelements"};

	bool hasMatchModuleOfInterest = false;

	// The recorded stack -- what ends up in stack.txt.
	std::vector<std::string> recorded;

	void SetSkipLeadingOwnFrames(bool skip) { m_skippingOwnFrames = skip; }

	bool IsModuleOfInterest(const char *moduleName) const
	{
		if (!moduleName)
			return false;

		for (auto &filter : modulesOfInterest) {
#ifdef _WIN32
			if (_stricmp(filter.c_str(), moduleName) == 0)
#else
			if (strcasecmp(filter.c_str(), moduleName) == 0)
#endif
				return true;
		}

		return false;
	}

	void OnCallstackEntry(const char *moduleName)
	{
		if (m_skippingOwnFrames) {
			if (IsModuleOfInterest(moduleName))
				return;

			m_skippingOwnFrames = false;
		}

		recorded.push_back(moduleName ? moduleName : "");

		if (!hasMatchModuleOfInterest)
			hasMatchModuleOfInterest =
				IsModuleOfInterest(moduleName);
	}

	void Walk(const std::vector<const char *> &frames, bool skipOwnLeading)
	{
		SetSkipLeadingOwnFrames(skipOwnLeading);

		for (auto *frame : frames)
			OnCallstackEntry(frame);
	}

private:
	bool m_skippingOwnFrames = false;
};

// The stack an abort() produces once we hold the SIGABRT door. Frame 0 is our
// own handler, because that is where RtlCaptureContext was called.
static const std::vector<const char *> kForeignAbort = {
	"obs-streamelements-core", // SentryAbortHandler  <- ours, and only ours
	"ucrtbase",                // raise
	"ucrtbase",                // abort
	"vcruntime140",            // _purecall
	"Qt6Widgets",              // the actual fault
	"obs64",
};

// Same shape, but the object that was double-destructed is ours.
static const std::vector<const char *> kOwnAbort = {
	"obs-streamelements-core", // SentryAbortHandler
	"ucrtbase",                // raise
	"ucrtbase",                // abort
	"vcruntime140",            // _purecall
	"obs-streamelements-core", // the actual fault -- genuinely ours
	"Qt6Widgets",
	"obs64",
};

// --- The gate must reject a foreign abort ---------------------------------
static void check_foreign_abort_is_rejected()
{
	Walker w;
	w.Walk(kForeignAbort, true);

	check(!w.hasMatchModuleOfInterest,
	      "CORE-860: an abort with no plug-in frame below the handler must NOT be reported");

	check(w.recorded.size() == kForeignAbort.size() - 1,
	      "CORE-860: exactly the one handler frame should be dropped");

	check(!w.recorded.empty() && w.recorded.front() == "ucrtbase",
	      "CORE-860: skipping must stop at the first frame that is not ours");
}

// --- ...and this is what makes the difference ------------------------------
//
// The negative control. Without the skip, the very same stack passes, which is
// precisely the rubber stamp this change exists to prevent. If this ever starts
// failing, the skip has stopped being load-bearing and the test above is
// passing for the wrong reason.
static void check_negative_control_without_skip()
{
	Walker w;
	w.Walk(kForeignAbort, false);

	check(w.hasMatchModuleOfInterest,
	      "CORE-860: negative control -- without the skip this foreign abort would be reported");
}

// --- A genuine fault of ours must still pass -------------------------------
static void check_own_abort_is_reported()
{
	Walker w;
	w.Walk(kOwnAbort, true);

	check(w.hasMatchModuleOfInterest,
	      "CORE-860: an abort whose fault really is ours must still be reported");

	// The skip is one-shot: the second own-module frame is the real fault
	// and must survive both the record and the verdict.
	check(w.recorded.size() == kOwnAbort.size() - 1,
	      "CORE-860: only the leading handler frame may be dropped, not later own frames");

	bool ownFrameRecorded = false;
	for (auto &m : w.recorded) {
		if (m == "obs-streamelements-core")
			ownFrameRecorded = true;
	}

	check(ownFrameRecorded,
	      "CORE-860: the real faulting frame must remain in the recorded stack");
}

// --- The SEH path must be untouched ----------------------------------------
static void check_seh_path_unchanged()
{
	const std::vector<const char *> foreign = {"Qt6Widgets", "obs64",
						   "ntdll"};
	const std::vector<const char *> ours = {"obs-streamelements-core",
						"Qt6Widgets", "obs64"};

	Walker a;
	a.Walk(foreign, false);
	check(!a.hasMatchModuleOfInterest,
	      "CORE-860: SEH gate must still reject a foreign crash");
	check(a.recorded.size() == foreign.size(),
	      "CORE-860: the SEH path must drop no frames at all");

	Walker b;
	b.Walk(ours, false);
	check(b.hasMatchModuleOfInterest,
	      "CORE-860: SEH gate must still accept our own crash");
	check(b.recorded.size() == ours.size(),
	      "CORE-860: the SEH path must drop no frames at all");
}

// --- Consecutive leading own frames all go ---------------------------------
//
// The handler is one frame today, but inlining, a helper, or a future change
// could make it several. Skipping is defined by "still ours", not by a count.
static void check_multiple_leading_own_frames()
{
	const std::vector<const char *> frames = {
		"obs-streamelements-core",
		"obs-streamelements",
		"obs-streamelements-core",
		"ucrtbase",
		"Qt6Widgets",
	};

	Walker w;
	w.Walk(frames, true);

	check(!w.hasMatchModuleOfInterest,
	      "CORE-860: a run of leading own frames must all be skipped");
	check(w.recorded.size() == 2,
	      "CORE-860: only the leading run of own frames may be dropped");
}

// --- Module matching stays case-insensitive --------------------------------
static void check_case_insensitive()
{
	const std::vector<const char *> frames = {"OBS-StreamElements-Core",
						  "ucrtbase", "Qt6Widgets"};

	Walker w;
	w.Walk(frames, true);

	check(!w.hasMatchModuleOfInterest,
	      "CORE-860: module matching must stay case-insensitive when skipping");
}

// --- A null module name must not crash the walk ----------------------------
static void check_null_module_name()
{
	Walker w;
	w.SetSkipLeadingOwnFrames(true);
	w.OnCallstackEntry(nullptr);

	check(!w.hasMatchModuleOfInterest,
	      "CORE-860: a null module name must not pass the gate");
}

/* ================================================================= */
//
// SELIVE-230: the fixture above is wrong about the real stack, and that is
// why the defect survived CORE-860.
//
// Every case so far puts our handler INNERMOST, because `RtlCaptureContext`
// is called there. On the plain abort door that is true. On the PURECALL
// door it is not, and the crash archive attached to SELIVE-230's event --
// which records a module path per frame, unlike the Sentry stack -- says so:
//
//     ucrtbase.dll                 raise
//     ucrtbase.dll                 abort
//     obs-streamelements-core.dll  SentryPurecallHandler
//     VCRUNTIME140.dll             purecall
//     ...\Elgato\...\VideoCaptureFilter.ax   DllMain   (x3)
//     KERNEL32.DLL / ntdll.dll
//
// `raise` is innermost. So a skip defined as "while the frame is ours"
// disengages on frame 0 having skipped NOTHING, and the verdict then latches
// on `SentryPurecallHandler` two frames down. Rubber stamp -- the exact thing
// CORE-860 set out to prevent -- by a route frame OWNERSHIP cannot see. The
// fault in that event is a pure virtual call inside a third-party DirectShow
// capture filter. None of it is ours.
//
// The fix matches the DOOR instead of the MODULE: five named functions,
// matched byte-for-byte on their function entry. The distinction is
// load-bearing. "Skip CRT frames too", by module name, would also skip a
// genuine fault in our own code -- silent under-reporting, strictly worse
// than the over-reporting it cures -- which is why the last case below
// exists.
//
// The real match is on function-entry addresses (SEIsCrashPlumbingFrame).
// Here a frame is a (module, function) pair and the door is matched by
// function name, which models the same decision without needing a process.

struct Frame {
	const char *module;
	const char *function;
};

// The door, as SESentryCrashHandler.cpp's CrashPlumbingFunctions() lists it.
static bool IsDoorFunction(const char *function)
{
	static const char *const door[] = {
		"SentryAbortHandler", "SentryPurecallHandler", "abort", "raise",
		"_purecall",
	};

	for (auto *name : door) {
		if (function && std::strcmp(name, function) == 0)
			return true;
	}

	return false;
}

// Mirror of the FIXED walker: ours, or the door, is skipped while arming.
struct PlumbingWalker {
	std::vector<std::string> modulesOfInterest = {"obs-streamelements-core",
						      "obs-streamelements"};

	bool hasMatchModuleOfInterest = false;
	bool useDoor = true;
	std::vector<std::string> recorded;

	bool IsModuleOfInterest(const char *moduleName) const
	{
		if (!moduleName)
			return false;

		for (auto &filter : modulesOfInterest) {
#ifdef _WIN32
			if (_stricmp(filter.c_str(), moduleName) == 0)
#else
			if (strcasecmp(filter.c_str(), moduleName) == 0)
#endif
				return true;
		}

		return false;
	}

	void Walk(const std::vector<Frame> &frames, bool skipOwnLeading)
	{
		bool skipping = skipOwnLeading;

		for (auto &frame : frames) {
			if (skipping) {
				// The door only, or -- without one -- ownership only.
				// Never both: see the production note.
				if (useDoor) {
					if (IsDoorFunction(frame.function))
						continue;
				} else {
					if (IsModuleOfInterest(frame.module))
						continue;
				}

				skipping = false;
			}

			recorded.push_back(frame.module ? frame.module : "");

			if (!hasMatchModuleOfInterest)
				hasMatchModuleOfInterest =
					IsModuleOfInterest(frame.module);
		}
	}
};

// SELIVE-230, frame for frame.
static const std::vector<Frame> kPurecallDoorForeign = {
	{"ucrtbase", "raise"},
	{"ucrtbase", "abort"},
	{"obs-streamelements-core", "SentryPurecallHandler"},
	{"vcruntime140", "_purecall"},
	{"VideoCaptureFilter", "DllMain"}, // the actual fault: Elgato's
	{"VideoCaptureFilter", "DllMain"},
	{"kernel32", "BaseThreadInitThunk"},
	{"ntdll", "RtlUserThreadStart"},
};

// The same door, but the pure virtual call really is on one of our objects.
static const std::vector<Frame> kPurecallDoorOurs = {
	{"ucrtbase", "raise"},
	{"ucrtbase", "abort"},
	{"obs-streamelements-core", "SentryPurecallHandler"},
	{"vcruntime140", "_purecall"},
	{"obs-streamelements-core", "SEWidgetManager::~SEWidgetManager"},
	{"Qt6Widgets", "QWidget::event"},
	{"obs64", "main"},
};

// --- The defect, and the control that proves the fixture is the point ------
static void check_the_old_rule_rubber_stamps_the_purecall_door()
{
	PlumbingWalker w;
	w.useDoor = false; // the rule as it shipped
	w.Walk(kPurecallDoorForeign, true);

	check(w.hasMatchModuleOfInterest,
	      "non-vacuity: with ownership-only skipping, SELIVE-230's stack MUST be claimed -- that is the defect, and if this stops failing the fixture has stopped modelling it");

	check(!w.recorded.empty() && w.recorded.front() == "ucrtbase",
	      "non-vacuity: the skip must have disengaged on frame 0 having skipped nothing, which is the mechanism");
}

// --- The fix ---------------------------------------------------------------
static void check_the_door_rule_declines_a_foreign_purecall()
{
	PlumbingWalker w;
	w.Walk(kPurecallDoorForeign, true);

	check(!w.hasMatchModuleOfInterest,
	      "SELIVE-230: a purecall whose fault is in a third-party module must NOT be claimed, however many of our handler frames sit above it");

	check(!w.recorded.empty() && w.recorded.front() == "VideoCaptureFilter",
	      "SELIVE-230: the recorded stack must begin at the first frame that is neither ours nor the door -- the actual fault");
}

// --- ...without swallowing a genuine fault of ours -------------------------
//
// The whole reason the door is matched by FUNCTION and not by module. A rule
// that skipped ucrtbase and vcruntime by name would skip our own faulting
// frame too, and we would silently stop reporting our own crashes.
static void check_the_door_rule_still_reports_our_own_purecall()
{
	PlumbingWalker w;
	w.Walk(kPurecallDoorOurs, true);

	check(w.hasMatchModuleOfInterest,
	      "SELIVE-230: a purecall on one of OUR objects must still be reported -- under-reporting our own crashes is worse than the over-reporting this fixes");

	check(!w.recorded.empty() &&
		      w.recorded.front() == "obs-streamelements-core",
	      "SELIVE-230: the walk must stop skipping at our genuine faulting frame, not consume it");
}

// --- The plain abort door is unaffected ------------------------------------
static void check_the_door_rule_leaves_the_plain_abort_door_alone()
{
	const std::vector<Frame> plainForeign = {
		{"obs-streamelements-core", "SentryAbortHandler"},
		{"ucrtbase", "raise"},
		{"ucrtbase", "abort"},
		{"Qt6Widgets", "QWidget::event"},
		{"obs64", "main"},
	};

	PlumbingWalker w;
	w.Walk(plainForeign, true);

	check(!w.hasMatchModuleOfInterest,
	      "SELIVE-230: the plain abort door must still be declined when the fault is not ours");
	check(!w.recorded.empty() && w.recorded.front() == "Qt6Widgets",
	      "SELIVE-230: and must now skip raise/abort as well, landing on the fault");
}

// --- The SEH path must not arm this at all ---------------------------------
static void check_the_door_rule_is_not_applied_to_an_seh_crash()
{
	PlumbingWalker w;
	w.Walk(kPurecallDoorForeign, false);

	check(w.hasMatchModuleOfInterest,
	      "SELIVE-230: with no arming, nothing is skipped -- an SEH CONTEXT comes from the OS at the fault point and must be judged as-is");
	check(w.recorded.size() == kPurecallDoorForeign.size(),
	      "SELIVE-230: the SEH path must drop no frames at all");
}

int main()
{
	check_foreign_abort_is_rejected();
	check_negative_control_without_skip();
	check_own_abort_is_reported();
	check_seh_path_unchanged();
	check_multiple_leading_own_frames();
	check_case_insensitive();
	check_null_module_name();

	// SELIVE-230: the purecall door, where the innermost frames are the
	// CRT's rather than ours.
	check_the_old_rule_rubber_stamps_the_purecall_door();
	check_the_door_rule_declines_a_foreign_purecall();
	check_the_door_rule_still_reports_our_own_purecall();
	check_the_door_rule_leaves_the_plain_abort_door_alone();
	check_the_door_rule_is_not_applied_to_an_seh_crash();

	if (failures) {
		std::fprintf(stderr, "%d crash-gate check(s) failed\n",
			     failures);
		return 1;
	}

	std::puts("test_crash_gate_leading_frames: all checks passed");
	return 0;
}
