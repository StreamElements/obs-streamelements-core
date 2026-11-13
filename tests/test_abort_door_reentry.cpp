// The abort door must not report the same death twice.
//
// Background, measured in Sentry over 90 days before this test existed:
//
//     crash.kind  title                events  machines
//     abort       handle_sigabrt        1,426       813
//     purecall    handle_sigabrt          178       172
//
// 1,604 events titled after `handle_sigabrt` -- sentry-native's own SIGABRT
// handler frame. Not one of them is a crash in `handle_sigabrt`; it is the
// innermost frame on the stack, and Sentry titles a native event by its
// innermost frame.
//
// They were produced here. `SentryAbortHandler` called sentry's displaced
// handler unconditionally once `HandleFatalException()` returned, and the only
// way it returns is re-entry -- every path through its outermost branch ends in
// TerminateProcess(). So the hand-back fired exactly when an outer pass was
// already in flight, and emitted a second event for one process death.
//
// Re-entry is not rare. The crash path puts modal windows up: our own consent
// prompt, then OBS's MessageBoxA in main_crash_handler(). A modal message loop
// keeps pumping, Qt timers keep firing on a half-destroyed process, and a
// second pure virtual call lands inside the first crash. SELIVE-3J carries both
// sequences in one trace, with MessageBoxA between them.
//
// Two things made the duplicate worse than noise. The module-of-interest gate
// and the consent prompt both live in HandleFatalException(), so the second
// event skipped both: it reported crashes the gate had already declined, and it
// uploaded without asking.
//
// Mirrors the depth handling in HandleFatalException() and the tail of
// SentryAbortHandler() from SESentryCrashHandler.cpp. The real ones need the
// CRT, Win32 and sentry; the decision under test is the handful of lines
// reproduced here.

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

/* ================================================================= */

// Mirror of the file statics.
static long g_insideExceptionFilter = 0;
static bool g_haveSentryAbortHandler = true;

// What we are counting.
static int g_ourEventsEmitted = 0;    // reports through our gated path
static int g_sentryEventsEmitted = 0; // reports through sentry's handler
static int g_consentPromptsShown = 0;
static int g_terminations = 0;

// Knobs for the scenarios below.
static bool g_moduleGateAllows = true;
static bool g_userConsents = true;

// Set when the outermost pass is made to return rather than terminate, which
// models a bug in our own crash path. That is the one case the hand-back to
// sentry exists for.
static bool g_outermostPassReturnsInsteadOfTerminating = false;

static void Reset()
{
	g_insideExceptionFilter = 0;
	g_haveSentryAbortHandler = true;
	g_ourEventsEmitted = 0;
	g_sentryEventsEmitted = 0;
	g_consentPromptsShown = 0;
	g_terminations = 0;
	g_moduleGateAllows = true;
	g_userConsents = true;
	g_outermostPassReturnsInsteadOfTerminating = false;
}

// Mirror of HandleFatalException(). Returns true if it RETURNED to its caller,
// false if it terminated the process. The real one returns
// EXCEPTION_CONTINUE_SEARCH or does not return at all; what matters to the
// decision under test is only which of those happened.
static bool HandleFatalException()
{
	if (++g_insideExceptionFilter == 1) {
		// The gate. A stack that never passed through our code is not
		// ours to report.
		if (g_moduleGateAllows) {
			++g_consentPromptsShown;

			if (g_userConsents)
				++g_ourEventsEmitted;
		}

		if (!g_outermostPassReturnsInsteadOfTerminating) {
			++g_terminations;
			return false; // TerminateProcess(); does not return
		}

		// Only reachable in the modelled-bug scenario.
		--g_insideExceptionFilter;
		return true;
	}

	// Re-entrant: an outer pass owns the report.
	--g_insideExceptionFilter;
	return true;
}

// Mirror of SentryAbortHandler()'s tail -- the decision under test.
static void AbortDoor()
{
	if (!HandleFatalException())
		return; // terminated inside

	// THE RULE. s_insideExceptionFilter is back to 0 only when the pass that
	// just returned was the outermost one, i.e. our own path failed to run
	// to completion. Degrading to sentry then beats silence; degrading to it
	// on an ordinary re-entry is a duplicate nobody consented to.
	if (g_insideExceptionFilter == 0 && g_haveSentryAbortHandler)
		++g_sentryEventsEmitted;

	++g_terminations;
}

/* ================================================================= */

// --- The first pass terminates, so the hand-back never runs ----------------
static void check_a_first_pass_never_reaches_the_hand_back()
{
	Reset();

	AbortDoor();

	check(g_ourEventsEmitted == 1,
	      "the first pass through the abort door must emit exactly one gated event");
	check(g_sentryEventsEmitted == 0,
	      "the first pass terminates inside HandleFatalException, so sentry's displaced handler must never run");
	check(g_terminations == 1, "the first pass must terminate the process");
}

// --- The regression: a re-entrant abort must not report again --------------
//
// This is the 1,604-event case. Our consent prompt or OBS's MessageBoxA is up,
// the modal loop dispatches a Qt timer, a second pure virtual call aborts on
// the same thread, and the abort door is entered again beneath the first.
static void check_a_reentrant_abort_does_not_emit_a_second_event()
{
	Reset();

	// Outer pass, in flight: it is sitting in the consent prompt.
	check(++g_insideExceptionFilter == 1,
	      "test setup: the outer pass must hold the counter at 1");

	// The modal loop pumps, and a second abort arrives on the same thread.
	AbortDoor();

	check(g_sentryEventsEmitted == 0,
	      "a re-entrant abort must NOT hand back to sentry's handler: the outer pass owns this death, and the second event skips both the module gate and the consent prompt");
	check(g_ourEventsEmitted == 0,
	      "a re-entrant abort must not emit a gated event either; HandleFatalException's outermost branch is what reports");
	check(g_consentPromptsShown == 0,
	      "a re-entrant abort must not raise a second consent prompt");
	check(g_insideExceptionFilter == 1,
	      "a re-entrant pass must leave the outer pass's depth untouched");
}

// --- The gate is not bypassed by the re-entry ------------------------------
//
// The worst shape of the old behaviour: the module-of-interest gate declines a
// crash that is not ours, and the re-entrant hand-back reports it anyway.
static void check_a_declined_crash_is_not_reported_by_the_reentry()
{
	Reset();
	g_moduleGateAllows = false;

	AbortDoor(); // first pass: declined, terminates

	check(g_ourEventsEmitted == 0,
	      "the module gate must suppress a crash that never passed through our code");
	check(g_sentryEventsEmitted == 0,
	      "a declined crash must not be reported through sentry's handler either");

	// And again, with the decline happening beneath an outer pass.
	Reset();
	g_moduleGateAllows = false;
	++g_insideExceptionFilter;

	AbortDoor();

	check(g_sentryEventsEmitted == 0,
	      "re-entry must not become a second door around the module-of-interest gate");
}

// --- Consent is not bypassed either ----------------------------------------
static void check_a_refused_consent_is_not_overridden_by_the_reentry()
{
	Reset();
	g_userConsents = false;
	++g_insideExceptionFilter; // outer pass, prompt refused, still unwinding

	AbortDoor();

	check(g_ourEventsEmitted == 0, "a refused consent must emit nothing");
	check(g_sentryEventsEmitted == 0,
	      "a refused consent must not be overridden by a re-entrant hand-back to sentry, which uploads silently");
}

// --- The safety net it was written for still works -------------------------
//
// The reason the hand-back exists: if our own path fails to run to completion,
// degrading to sentry's old behaviour beats reporting nothing at all. That is
// the outermost pass returning, which it is not supposed to do.
static void check_an_outermost_pass_that_returns_still_degrades_to_sentry()
{
	Reset();
	g_outermostPassReturnsInsteadOfTerminating = true;

	AbortDoor();

	check(g_insideExceptionFilter == 0,
	      "test setup: an outermost pass that returns must leave the depth at 0");
	check(g_sentryEventsEmitted == 1,
	      "if our own crash path returns instead of terminating, the hand-back to sentry must still fire -- that is the safety net, and conditioning it must not remove it");
}

// --- No handler to chain to ------------------------------------------------
static void check_a_missing_sentry_handler_is_not_called()
{
	Reset();
	g_outermostPassReturnsInsteadOfTerminating = true;
	g_haveSentryAbortHandler = false;

	AbortDoor();

	check(g_sentryEventsEmitted == 0,
	      "signal() can fail to return a previous handler; a null one must not be called");
	check(g_terminations == 1,
	      "the abort door must terminate even when there is nothing to chain to");
}

// --- abort() must never return to its caller -------------------------------
static void check_every_path_terminates()
{
	Reset();
	AbortDoor();
	check(g_terminations >= 1, "first pass must terminate");

	Reset();
	++g_insideExceptionFilter;
	AbortDoor();
	check(g_terminations >= 1, "re-entrant pass must terminate");

	Reset();
	g_outermostPassReturnsInsteadOfTerminating = true;
	AbortDoor();
	check(g_terminations >= 1,
	      "a returning outermost pass must still terminate; abort() returning to its caller is undefined behaviour");
}

/* ================================================================= */

int main()
{
	check_a_first_pass_never_reaches_the_hand_back();
	check_a_reentrant_abort_does_not_emit_a_second_event();
	check_a_declined_crash_is_not_reported_by_the_reentry();
	check_a_refused_consent_is_not_overridden_by_the_reentry();
	check_an_outermost_pass_that_returns_still_degrades_to_sentry();
	check_a_missing_sentry_handler_is_not_called();
	check_every_path_terminates();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all abort-door re-entry checks passed\n");
	return 0;
}
