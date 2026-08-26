// CORE-864: the WER runtime exception module must decide correctly, and in the
// right order.
//
// Heap corruption and every __fastfail bypass SEH, so nothing inside the
// crashing process ever sees them. They are captured out of process, by
// se-crash-wer.dll running inside WerFault.exe, after we are already dead.
//
// That module makes one decision -- claim, or decline -- and getting it wrong
// is expensive in both directions:
//
//   claiming too much   : we report OBS's crashes and every other plugin's,
//                         which is exactly what the CORE-860 gate exists to
//                         stop, and we report them from users who never agreed;
//   claiming too little : the only crash class nothing else can capture stays
//                         invisible.
//
// The real module needs a live WerFault, a crashed process to walk and sentry's
// own module to forward to, so what is mirrored here is the decision itself:
// the three tests it applies, and the order it applies them in.
//
// The module's own end-to-end behaviour was verified separately against a real
// __fastfail, a real WerFault and a real sentry-crash daemon.

#include <cctype>
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

/* ================================================================= */

// The three codes that reach a WER module and nothing else.
static const unsigned long STATUS_FAIL_FAST_EXCEPTION = 0xC0000602UL;
static const unsigned long STATUS_STACK_BUFFER_OVERRUN = 0xC0000409UL;
static const unsigned long STATUS_HEAP_CORRUPTION = 0xC0000374UL;

// Codes that must NOT be claimed: SEH delivers them to the in-process filter,
// which applies its own gate and asks for consent.
static const unsigned long EXCEPTION_ACCESS_VIOLATION = 0xC0000005UL;
static const unsigned long EXCEPTION_CPP = 0xE06D7363UL;

struct Crash {
	unsigned long code = STATUS_STACK_BUFFER_OVERRUN;
	bool isFatal = true;

	// Module names on the crashed thread, innermost first.
	std::vector<std::string> stack;

	// What the registration block carried.
	std::vector<std::string> modulesOfInterest = {"obs-streamelements-core",
						      "obs-streamelements"};
};

// Which tests the module actually performed, in order. Recorded so the ordering
// invariant can be asserted rather than assumed.
struct Trace {
	bool testedFatal = false;
	bool testedCode = false;
	bool walkedStack = false;
	bool forwarded = false;
};

static bool IsNativeWerException(unsigned long code)
{
	return code == STATUS_FAIL_FAST_EXCEPTION ||
	       code == STATUS_STACK_BUFFER_OVERRUN ||
	       code == STATUS_HEAP_CORRUPTION;
}

static bool IsModuleOfInterest(const Crash &crash, const std::string &name)
{
	for (const auto &filter : crash.modulesOfInterest) {
		// Case-insensitive, matching both the in-process gate and
		// _stricmp in the module.
		if (filter.size() != name.size())
			continue;

		bool equal = true;

		for (std::size_t i = 0; i < filter.size(); ++i) {
			if (std::tolower((unsigned char)filter[i]) !=
			    std::tolower((unsigned char)name[i])) {
				equal = false;
				break;
			}
		}

		if (equal)
			return true;
	}

	return false;
}

//
// Mirror of OutOfProcessExceptionEventCallback's decision, in the same order:
// fatal, then code, then the stack walk, then forward.
//
// Consent is deliberately absent. This crash class cannot ask -- the process is
// gone before any handler of ours runs -- and gating it on an answer to some
// earlier prompt would mean the first fast-fail a user ever hits is always the
// one that goes unreported. See check_no_prior_consent_is_required.
//
static bool Decide(const Crash &crash, Trace &trace)
{
	trace.testedFatal = true;

	if (!crash.isFatal)
		return false;

	trace.testedCode = true;

	if (!IsNativeWerException(crash.code))
		return false;

	trace.walkedStack = true;

	bool ours = false;

	for (const auto &frame : crash.stack) {
		if (IsModuleOfInterest(crash, frame)) {
			ours = true;
			break;
		}
	}

	if (!ours)
		return false;

	trace.forwarded = true;

	// Ownership is whatever sentry's module says. We never claim on our own
	// account -- claiming something sentry will not report would suppress
	// WER's normal handling for a crash that then goes nowhere.
	return true;
}

/* ================================================================= */

static void check_fastfail_in_our_module_is_claimed()
{
	Crash crash;
	crash.code = STATUS_STACK_BUFFER_OVERRUN;
	crash.stack = {"obs-streamelements-core", "obs", "KERNELBASE"};

	Trace trace;

	check(Decide(crash, trace),
	      "CORE-864: a fast-fail through our own module must be claimed -- nothing else on Windows can capture it");
	check(trace.forwarded,
	      "CORE-864: a claimed crash must be forwarded to sentry's module, which owns the handshake with the daemon");
}

static void check_heap_corruption_in_our_module_is_claimed()
{
	Crash crash;
	crash.code = STATUS_HEAP_CORRUPTION;
	crash.stack = {"ntdll", "obs-streamelements-core"};

	Trace trace;

	check(Decide(crash, trace),
	      "CORE-864: heap corruption reaching our code must be claimed; Windows terminates before any SEH dispatch");
}

static void check_fail_fast_exception_code_is_claimed()
{
	Crash crash;
	crash.code = STATUS_FAIL_FAST_EXCEPTION;
	crash.stack = {"obs-streamelements-core"};

	Trace trace;

	check(Decide(crash, trace),
	      "CORE-864: STATUS_FAIL_FAST_EXCEPTION must be handled alongside the other two");
}

// --- The gate ------------------------------------------------------------
//
// This is the CORE-860 decision, applied to the one crash class that never
// reaches the in-process filter. Losing it here loses it everywhere.
static void check_crash_in_another_plugin_is_declined()
{
	Crash crash;
	crash.stack = {"some-other-plugin", "obs", "KERNELBASE"};

	Trace trace;

	check(!Decide(crash, trace),
	      "CORE-864: a fast-fail that never passed through our code is not ours to report");
	check(trace.walkedStack,
	      "CORE-864: the verdict must come from actually walking the stack, not from refusing to look");
	check(!trace.forwarded,
	      "CORE-864: a declined crash must never reach sentry's module");
}

static void check_crash_in_obs_itself_is_declined()
{
	Crash crash;
	crash.stack = {"obs", "Qt6Core", "KERNELBASE"};

	Trace trace;

	check(!Decide(crash, trace),
	      "CORE-864: OBS's own fast-fail crashes belong to OBS, not to us");
}

static void check_module_match_is_case_insensitive()
{
	Crash crash;
	crash.stack = {"OBS-StreamElements-Core"};

	Trace trace;

	check(Decide(crash, trace),
	      "CORE-864: module names come from the loader and their case is not ours to predict");
}

static void check_remote_module_list_is_honoured()
{
	// settings.json replaces the built-in list wholesale. The registration
	// block carries whatever the in-process gate ended up with, so that the
	// two cannot disagree.
	Crash crash;
	crash.modulesOfInterest = {"obs-browser"};
	crash.stack = {"obs-browser", "obs"};

	Trace trace;

	check(Decide(crash, trace),
	      "CORE-864: the module list from the registration block must be used, not a hard-coded one");

	Crash ours;
	ours.modulesOfInterest = {"obs-browser"};
	ours.stack = {"obs-streamelements-core"};

	Trace t2;

	check(!Decide(ours, t2),
	      "CORE-864: when the remote list replaces the defaults, the WER gate must narrow with the in-process gate rather than keeping its own answer");
}

// --- Consent -------------------------------------------------------------
//
// There is none, and that is the decision: this class is reported on implicit
// consent.
//
// The alternative -- honour an answer given at some earlier crash prompt --
// sounds safer and is worse. A user's first fast-fail would always go
// unreported, and a user who never has an ordinary crash would never report one
// at all. It is also a narrower disclosure than it sounds: a WER report carries
// a minidump and the tags armed at startup, never the configuration archive,
// the screenshot of the user's screen, or their description of what they were
// doing, because nothing is left running to collect them. The crash-time prompt
// is what asks about that material; this is a different, much smaller payload,
// and the prompt says so.
static void check_no_prior_consent_is_required()
{
	// A first-ever fast-fail, on a profile that has never seen a prompt.
	Crash crash;
	crash.stack = {"obs-streamelements-core"};

	Trace trace;

	check(Decide(crash, trace),
	      "CORE-864: a fast-fail must be reported without any prior answer -- there is no moment at which this crash could have asked");
	check(trace.forwarded,
	      "CORE-864: it must reach sentry's module, which is what actually captures the dump");
}

// --- Codes we must not touch ---------------------------------------------

static void check_access_violation_is_declined()
{
	Crash crash;
	crash.code = EXCEPTION_ACCESS_VIOLATION;
	crash.stack = {"obs-streamelements-core"};

	Trace trace;

	check(!Decide(crash, trace),
	      "CORE-864: an access violation reaches the in-process filter, which gates and prompts; claiming it here would bypass both");
	check(!trace.walkedStack,
	      "CORE-864: a non-WER exception must be rejected on the code alone, before anything expensive");
}

static void check_cpp_exception_is_declined()
{
	Crash crash;
	crash.code = EXCEPTION_CPP;
	crash.stack = {"obs-streamelements-core"};

	Trace trace;

	check(!Decide(crash, trace),
	      "CORE-864: an unhandled C++ exception raises 0xE06D7363 and lands in our SEH filter; the WER module must leave it alone");
}

static void check_non_fatal_is_declined()
{
	// WER reports non-fatal events too. Claiming one would suppress WER's
	// normal handling for a process that is not even dying.
	Crash crash;
	crash.isFatal = false;
	crash.stack = {"obs-streamelements-core"};

	Trace trace;

	check(!Decide(crash, trace),
	      "CORE-864: a non-fatal WER event must be declined");
	check(!trace.testedCode,
	      "CORE-864: fatality is the very first test");
}

// --- Negative control ----------------------------------------------------
//
// If the gate were removed -- if the module claimed everything fatal, the way
// sentry's own module does -- these are the crashes we would start reporting.
// Kept so that a change which quietly drops the gate has to fail a test that
// says exactly what it costs.
static void check_ungated_module_would_claim_other_peoples_crashes()
{
	const std::vector<std::string> foreign[] = {
		{"some-other-plugin", "obs"},
		{"obs", "Qt6Core"},
		{"nvidia-video-effects", "obs"},
	};

	int wouldClaimUngated = 0;
	int claimedByUs = 0;

	for (const auto &stack : foreign) {
		Crash crash;
		crash.stack = stack;

		Trace trace;

		// Ungated: fatal and one of the three codes is the whole test.
		if (crash.isFatal && IsNativeWerException(crash.code))
			++wouldClaimUngated;

		if (Decide(crash, trace))
			++claimedByUs;
	}

	check(wouldClaimUngated == 3,
	      "CORE-864: negative control -- an ungated module claims every fatal fast-fail in the process");
	check(claimedByUs == 0,
	      "CORE-864: the gate must decline all three, which is the entire difference between registering ours and registering sentry's");
}

int main()
{
	check_fastfail_in_our_module_is_claimed();
	check_heap_corruption_in_our_module_is_claimed();
	check_fail_fast_exception_code_is_claimed();

	check_crash_in_another_plugin_is_declined();
	check_crash_in_obs_itself_is_declined();
	check_module_match_is_case_insensitive();
	check_remote_module_list_is_honoured();

	check_no_prior_consent_is_required();

	check_access_violation_is_declined();
	check_cpp_exception_is_declined();
	check_non_fatal_is_declined();

	check_ungated_module_would_claim_other_peoples_crashes();

	if (failures) {
		std::fprintf(stderr, "%d WER gate check(s) failed\n", failures);
		return 1;
	}

	std::puts("test_wer_gate: all checks passed");
	return 0;
}
