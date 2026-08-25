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
// This mirrors MyStackWalker's frame filter from StreamElementsCrashContext.cpp
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

// Mirror of MyStackWalker (Windows branch) in StreamElementsCrashContext.cpp.
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

int main()
{
	check_foreign_abort_is_rejected();
	check_negative_control_without_skip();
	check_own_abort_is_reported();
	check_seh_path_unchanged();
	check_multiple_leading_own_frames();
	check_case_insensitive();
	check_null_module_name();

	if (failures) {
		std::fprintf(stderr, "%d crash-gate check(s) failed\n",
			     failures);
		return 1;
	}

	std::puts("test_crash_gate_leading_frames: all checks passed");
	return 0;
}
