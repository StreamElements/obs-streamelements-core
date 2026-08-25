// CORE-863: the out-of-memory handler must observe and chain, not seize.
//
// Background. BugSplat installed five process-global CRT hooks; the Sentry
// migration dropped all five, and CORE-860 recovered two. This is the
// allocation one -- _set_new_handler plus _set_new_mode.
//
// The design decision under test is that our handler is NOT what BugSplat's
// was. BugSplat's memory_depleted() force-crashes the process on any allocation
// failure, and _set_new_mode(1) extends that to plain malloc -- which would
// preempt libobs's own bmalloc -> bcrash handling. A plug-in does not get to
// override the host's allocation-failure policy.
//
// So ours must:
//   - record that an OOM happened,
//   - hand back the 1MB guard buffer at the moment memory ran out, not later,
//   - chain to whoever held the handler before us and honour a retry request,
//   - and otherwise return 0, preserving standard semantics exactly:
//     operator new throws std::bad_alloc, malloc returns NULL.
//
// Mirrors SentryNewHandler and the crash.kind precedence from
// StreamElementsSentryCrashHandler.cpp. The real ones need the CRT and sentry;
// the logic under test is the twenty lines reproduced here.

#include <cstdio>
#include <cstddef>
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

#ifndef _WIN32
#define __cdecl
#endif

typedef int(__cdecl *NewHandler)(size_t);

static long g_sawOutOfMemory = 0;
static long g_abortIsPurecall = 0;
static int g_guardReleaseCount = 0;
static bool g_guardBufferHeld = true;

static NewHandler g_previousNewHandler = nullptr;

// Mirror of StreamElementsCrashContext::ReleaseGuardBuffer -- idempotent.
static void ReleaseGuardBuffer()
{
	++g_guardReleaseCount;

	if (!g_guardBufferHeld)
		return;

	g_guardBufferHeld = false;
}

// Mirror of SentryNewHandler.
static int SentryNewHandler(size_t size)
{
	g_sawOutOfMemory = 1;

	ReleaseGuardBuffer();

	if (g_previousNewHandler)
		return g_previousNewHandler(size);

	return 0;
}

// Mirror of the crash.kind selection in HandleFatalException.
//
// `fromAbortDoor` distinguishes the two entry points. It matters more than it
// looks: an uncaught std::bad_alloc does NOT reach abort() on MSVC -- `throw`
// raises a real SEH exception, so the OOM case arrives with fromAbortDoor
// false, through the filter.
static const char *CrashKind(bool fromAbortDoor)
{
	return g_abortIsPurecall  ? "purecall"
	       : g_sawOutOfMemory ? "oom"
	       : fromAbortDoor    ? "abort"
				  : "exception";
}

static void Reset()
{
	g_sawOutOfMemory = 0;
	g_abortIsPurecall = 0;
	g_guardReleaseCount = 0;
	g_guardBufferHeld = true;
	g_previousNewHandler = nullptr;
}

/* ================================================================= */

// --- Standard semantics are preserved --------------------------------------
//
// Returning 0 is what makes operator new throw std::bad_alloc and malloc return
// NULL. Returning non-zero tells the CRT to retry the allocation, so a handler
// that returned non-zero unconditionally would spin forever.
static void check_returns_zero_without_a_chain()
{
	Reset();

	check(SentryNewHandler(4096) == 0,
	      "CORE-863: with no previous handler the new handler must return 0, or operator new never throws and malloc never returns NULL");
}

// --- It records the OOM ----------------------------------------------------
static void check_records_the_oom()
{
	Reset();

	check(CrashKind(true) == std::string("abort"),
	      "CORE-863: before any allocation failure the abort door must report a plain abort");
	check(CrashKind(false) == std::string("exception"),
	      "CORE-863: before any allocation failure the SEH door must report an exception, not an abort");

	SentryNewHandler(4096);

	check(g_sawOutOfMemory == 1,
	      "CORE-863: the handler must record that an allocation failed");
	check(CrashKind(true) == std::string("oom"),
	      "CORE-863: an abort after an allocation failure must be reported as an OOM");
}

// --- The guard buffer is released here, not in Collect() -------------------
//
// This is the whole reason the reserve exists: the moment memory ran out is the
// moment the crash path needs headroom.
static void check_releases_the_guard_buffer()
{
	Reset();

	check(g_guardBufferHeld,
	      "CORE-863: the guard buffer should be held before any failure");

	SentryNewHandler(4096);

	check(!g_guardBufferHeld,
	      "CORE-863: the new handler must hand the guard buffer back immediately");
}

// --- Releasing twice is harmless -------------------------------------------
//
// HandleFatalException also releases it, and both may run.
static void check_guard_release_is_idempotent()
{
	Reset();

	SentryNewHandler(4096);
	SentryNewHandler(8192);
	ReleaseGuardBuffer(); // as HandleFatalException does

	check(g_guardReleaseCount == 3,
	      "CORE-863: every release attempt should be counted by this mirror");
	check(!g_guardBufferHeld,
	      "CORE-863: repeated release must stay released and must not fault");
}

/* ================================================================= */

static int g_previousCalledWith = 0;
static int g_previousReturns = 0;

static int PreviousHandler(size_t size)
{
	g_previousCalledWith = (int)size;

	return g_previousReturns;
}

// --- Chaining: an upstream retry wins --------------------------------------
//
// If whoever held the handler before us can free something and asks for a
// retry, that is a better outcome than a crash, and it is theirs to decide.
static void check_chains_and_honours_retry()
{
	Reset();
	g_previousNewHandler = &PreviousHandler;
	g_previousCalledWith = 0;
	g_previousReturns = 1; // "I freed something, try again"

	const int result = SentryNewHandler(12345);

	check(g_previousCalledWith == 12345,
	      "CORE-863: the previous handler must be called, with the original size");
	check(result == 1,
	      "CORE-863: an upstream retry request must be passed through, not swallowed");
}

// --- Chaining: an upstream refusal is passed through too -------------------
static void check_chains_refusal()
{
	Reset();
	g_previousNewHandler = &PreviousHandler;
	g_previousReturns = 0; // "I cannot help either"

	check(SentryNewHandler(4096) == 0,
	      "CORE-863: an upstream refusal must still end in 0 so the standard path runs");
}

// --- Even a chained retry still records the OOM ----------------------------
//
// The allocation may yet succeed, but it failed once, and that is worth knowing
// if the process dies later.
static void check_records_even_when_retried()
{
	Reset();
	g_previousNewHandler = &PreviousHandler;
	g_previousReturns = 1;

	SentryNewHandler(4096);

	check(g_sawOutOfMemory == 1,
	      "CORE-863: a recovered allocation failure must still be recorded");
	check(!g_guardBufferHeld,
	      "CORE-863: the guard buffer is released on any failure, recovered or not");
}

/* ================================================================= */

// --- Precedence: purecall outranks a sticky OOM flag -----------------------
//
// The OOM flag is inferred and never cleared. A failure that was caught and
// handled leaves it set forever, so it must not relabel a later, unrelated
// pure-virtual crash.
static void check_purecall_outranks_stale_oom()
{
	Reset();

	SentryNewHandler(4096); // handled fine, long ago
	g_abortIsPurecall = 1;  // and now something else kills us

	check(CrashKind(true) == std::string("purecall"),
	      "CORE-863: a definite, proximate purecall must outrank a sticky OOM flag");
}

// --- ...but OOM outranks a bare abort, on either door ---
//
// This is the std::bad_alloc -> terminate -> abort path the handler exists for.
static void check_oom_outranks_bare_abort()
{
	Reset();

	SentryNewHandler(4096);

	// Through the SEH door, because that is how an uncaught bad_alloc
	// actually arrives -- verified by a real OOM, not assumed.
	check(CrashKind(false) == std::string("oom"),
	      "CORE-863: an uncaught bad_alloc arrives through the SEH filter and must still be reported as an OOM");
	check(CrashKind(true) == std::string("oom"),
	      "CORE-863: and so must one that arrives through the abort door");
}

int main()
{
	check_returns_zero_without_a_chain();
	check_records_the_oom();
	check_releases_the_guard_buffer();
	check_guard_release_is_idempotent();
	check_chains_and_honours_retry();
	check_chains_refusal();
	check_records_even_when_retried();
	check_purecall_outranks_stale_oom();
	check_oom_outranks_bare_abort();

	if (failures) {
		std::fprintf(stderr, "%d OOM-handler check(s) failed\n",
			     failures);
		return 1;
	}

	std::puts("test_crash_oom_handler: all checks passed");
	return 0;
}
