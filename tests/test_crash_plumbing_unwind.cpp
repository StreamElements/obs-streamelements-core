// The real unwind, over a real stack.
//
// test_crash_context_advance.cpp models the DECISION -- which frames are
// skipped and every condition that must abandon the walk -- and says plainly
// what it cannot do:
//
//     What is NOT modelled, and cannot be from a test: RtlVirtualUnwind
//     running against a real stack inside a dying process. That is the part
//     to be sceptical of.
//
// Half of that is now testable, and this is it. SECrashPlumbingUnwind.cpp
// depends on nothing but windows.h, so the production walk compiles here and
// runs against a stack this file actually builds: a chain of noinline
// functions standing in for the abort door, with the plumbing list pointing at
// those functions instead of at SentryAbortHandler and friends. Every
// RtlLookupFunctionEntry and RtlVirtualUnwind below is the shipping one.
//
// The half that remains untestable is the other clause: inside a dying
// process. A stack corrupt enough to fault the unwinder cannot be arranged
// here, which is what the production __try is for.
//
// Mutation-proven, and the gap is worth recording rather than hiding: four
// mutations of the production walk, three caught here -- the depth cap, the
// never-escaped guard, and the plumbing match itself. The fourth, deleting
// the strict-increase stack-pointer check, this file CANNOT catch, because
// that check only fires once the unwinder has lost the stack and a real
// stack will not do that on request. It is pinned instead by
// check_sentry_is_handed_the_fault_not_our_handler, which was confirmed to
// fail with the comparison removed. Four green cases here are not four
// guarded behaviours.
//
// Windows x64 only -- the unwind API is the subject, not an implementation
// detail -- and tests/CMakeLists.txt registers it only there.

#include <windows.h>

#include <cstdio>
#include <cstring>

#include "streamelements/SECrashPlumbingUnwind.hpp"

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

// Defeats tail calls and dead-code elimination: a call whose result is
// discarded and whose caller returns immediately can be turned into a jump,
// and a jump leaves NO frame -- which would quietly delete the very thing
// under test and leave a passing shorter walk.
static volatile long long g_sink = 0;

// The frame shape is the subject here, so the optimiser is not allowed a say
// in it. Without this the test would pass or fail by build configuration.
#pragma optimize("", off)

/* ================================================================= */

// What every scenario measures against: the function a given address belongs
// to, by the same rule the production walk uses.
static const void *FunctionBaseOf(const void *address)
{
	DWORD64 imageBase = 0;

	PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(
		(DWORD64)(uintptr_t)address, &imageBase, NULL);

	if (!entry)
		return NULL;

	return (const void *)(uintptr_t)(imageBase + entry->BeginAddress);
}

// Filled in by the innermost frame of whichever chain is running.
static CONTEXT g_captured;
static CONTEXT g_advanced;
static bool g_moved = false;
static const void *const *g_plumbing = NULL;
static size_t g_plumbingCount = 0;

// Stands in for SentryAbortHandler: captures the context and runs the walk,
// exactly where the production handler does.
__declspec(noinline) static void Handler(void)
{
	RtlCaptureContext(&g_captured);

	g_advanced = g_captured;
	g_moved = SEAdvanceContextPastPlumbing(&g_advanced, g_plumbing,
					       g_plumbingCount);

	g_sink += 1;
}

// The abort door, in the order it stacks up. Named for the production frames
// they stand in for.
__declspec(noinline) static void Raise(void)
{
	Handler();
	g_sink += 2;
}

__declspec(noinline) static void Abort(void)
{
	Raise();
	g_sink += 3;
}

__declspec(noinline) static void PurecallHandler(void)
{
	Abort();
	g_sink += 4;
}

__declspec(noinline) static void Purecall(void)
{
	PurecallHandler();
	g_sink += 5;
}

// The code that actually broke. Nothing below this is plumbing.
__declspec(noinline) static void Breaker(void)
{
	Purecall();
	g_sink += 6;
}

// The full door: five frames, exactly as SESentryCrashHandler.cpp lists them.
static const void *const kDoor[] = {
	(const void *)&Handler,  (const void *)&PurecallHandler,
	(const void *)&Abort,    (const void *)&Raise,
	(const void *)&Purecall,
};

/* ================================================================= */

// Ten frames of nothing but plumbing, for the depth cap.
#define DEEP_FRAME(n, inner)                           \
	__declspec(noinline) static void Deep##n(void) \
	{                                              \
		inner();                               \
		g_sink += 100 + n;                     \
	}

DEEP_FRAME(9, Handler)
DEEP_FRAME(8, Deep9)
DEEP_FRAME(7, Deep8)
DEEP_FRAME(6, Deep7)
DEEP_FRAME(5, Deep6)
DEEP_FRAME(4, Deep5)
DEEP_FRAME(3, Deep4)
DEEP_FRAME(2, Deep3)
DEEP_FRAME(1, Deep2)
DEEP_FRAME(0, Deep1)

static const void *const kAllPlumbing[] = {
	(const void *)&Handler, (const void *)&Deep9, (const void *)&Deep8,
	(const void *)&Deep7,   (const void *)&Deep6, (const void *)&Deep5,
	(const void *)&Deep4,   (const void *)&Deep3, (const void *)&Deep2,
	(const void *)&Deep1,   (const void *)&Deep0,
};

#pragma optimize("", on)

/* ================================================================= */

static void Run(const void *const *plumbing, size_t count, void (*entry)(void))
{
	std::memset(&g_captured, 0, sizeof(g_captured));
	std::memset(&g_advanced, 0, sizeof(g_advanced));
	g_moved = false;
	g_plumbing = plumbing;
	g_plumbingCount = count;

	entry();
}

/* ================================================================= */

// --- Before anything else: the listed addresses must be resolvable ---------
//
// The whole match is "this frame's function entry is byte-identical to that
// function's". If taking the address of a listed function yields something
// with no RUNTIME_FUNCTION -- an incremental-linking thunk is the way that
// happens -- then FunctionBaseOf() answers NULL for it, nothing ever matches,
// and the walk silently never fires. It would still fail safe, and it would
// still be useless, and no other case here would say so.
static void check_every_listed_function_has_an_entry()
{
	for (size_t i = 0; i < sizeof(kDoor) / sizeof(kDoor[0]); ++i)
		check(FunctionBaseOf(kDoor[i]) != NULL,
		      "a function named in the plumbing list must resolve to a function entry; if taking its address yields a thunk with no unwind data the match can never fire and the correction is inert");

	check(FunctionBaseOf((const void *)&Breaker) != NULL,
	      "the fault function must resolve too, or the positive cases below prove nothing");
}

// --- The purecall door lands on the fault ----------------------------------
static void check_the_whole_purecall_door_lands_on_the_breaker()
{
	Run(kDoor, sizeof(kDoor) / sizeof(kDoor[0]), Breaker);

	// Non-vacuity first: the captured context must really be inside the
	// handler, or "it moved" would mean nothing.
	check(FunctionBaseOf((const void *)(uintptr_t)g_captured.Rip) ==
		      FunctionBaseOf((const void *)&Handler),
	      "non-vacuity: the captured context must start inside the handler, which is the whole problem being corrected");

	check(g_moved,
	      "the real unwind must advance the context off the ordinary five-frame abort door");

	check(FunctionBaseOf((const void *)(uintptr_t)g_advanced.Rip) ==
		      FunctionBaseOf((const void *)&Breaker),
	      "it must land on the code that actually broke -- not on _purecall, and not on our handler");

	check(g_advanced.Rsp > g_captured.Rsp,
	      "the stack pointer must have risen: on x64 the frame we unwound to sits above the one we came from");
}

// --- The plain abort door, which has no purecall leg ------------------------
static void check_a_plain_abort_door_lands_on_its_caller()
{
	// The same chain, with only the three abort()-path frames listed. The
	// walk must stop at PurecallHandler, which is now not plumbing.
	static const void *const plainAbort[] = {
		(const void *)&Handler,
		(const void *)&Raise,
		(const void *)&Abort,
	};

	Run(plainAbort, sizeof(plainAbort) / sizeof(plainAbort[0]), Breaker);

	check(g_moved, "a three-frame door must advance too");

	check(FunctionBaseOf((const void *)(uintptr_t)g_advanced.Rip) ==
		      FunctionBaseOf((const void *)&PurecallHandler),
	      "it must stop at the first frame that is not listed, however many frames remain beneath it");
}

// --- The SEH door must not be touched --------------------------------------
//
// There the CONTEXT comes from the OS at the fault point, so the innermost
// frame is already the fault and there is nothing to skip.
static void check_a_stack_that_starts_outside_the_plumbing_is_left_alone()
{
	// Entering at Handler's own caller means the innermost frame is
	// Handler -- listed. Entering at a frame that is NOT listed is what
	// this needs, so the list omits the handler itself.
	static const void *const withoutHandler[] = {
		(const void *)&Raise,
		(const void *)&Abort,
		(const void *)&PurecallHandler,
	};

	Run(withoutHandler, sizeof(withoutHandler) / sizeof(withoutHandler[0]),
	    Breaker);

	check(!g_moved,
	      "a context whose innermost frame is not plumbing must report no change");

	check(g_advanced.Rip == g_captured.Rip &&
		      g_advanced.Rsp == g_captured.Rsp,
	      "and the original context must come back untouched");
}

// --- Fail safe: a walk that never escapes ----------------------------------
static void check_a_walk_that_never_escapes_keeps_the_original()
{
	Run(kAllPlumbing, sizeof(kAllPlumbing) / sizeof(kAllPlumbing[0]),
	    Deep0);

	check(!g_moved,
	      "eleven consecutive plumbing frames exceed the depth cap, and a walk that has not got clear of our own machinery must abandon rather than guess");

	check(g_advanced.Rip == g_captured.Rip &&
		      g_advanced.Rsp == g_captured.Rsp,
	      "the original context must be untouched after an abandoned walk");
}

// --- Fail safe: nothing to match against ------------------------------------
static void check_an_empty_list_moves_nothing()
{
	Run(kDoor, 0, Breaker);

	check(!g_moved,
	      "with no plumbing listed there is nothing to skip, so the context must not move");

	check(g_advanced.Rip == g_captured.Rip,
	      "the original context must be untouched");
}

// --- The guard rejects nonsense rather than walking it ----------------------
static void check_a_null_list_is_refused()
{
	CONTEXT context;
	RtlCaptureContext(&context);

	const DWORD64 rip = context.Rip;

	check(!SEAdvanceContextPastPlumbing(&context, NULL, 4),
	      "a null plumbing list must be refused, not dereferenced");
	check(context.Rip == rip, "and the context must be untouched");

	check(!SEAdvanceContextPastPlumbing(NULL, kDoor, 1),
	      "a null context must be refused");
}

/* ================================================================= */

int main()
{
	check_every_listed_function_has_an_entry();
	check_the_whole_purecall_door_lands_on_the_breaker();
	check_a_plain_abort_door_lands_on_its_caller();
	check_a_stack_that_starts_outside_the_plumbing_is_left_alone();
	check_a_walk_that_never_escapes_keeps_the_original();
	check_an_empty_list_moves_nothing();
	check_a_null_list_is_refused();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all crash plumbing unwind checks passed\n");
	return 0;
}
