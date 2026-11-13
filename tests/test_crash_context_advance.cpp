// The abort door must hand sentry the fault, not our own handler.
//
// CORE-2267: Sentry titles a native event by its INNERMOST frame. The abort
// door captures its CONTEXT inside SentryAbortHandler, so the innermost frames
// are always some permutation of
//
//     SentryAbortHandler <- raise <- abort <- SentryPurecallHandler
//       <- _purecall <- the code that actually broke
//
// and every event lands under one of a handful of titles that name our
// plumbing. WalkStack() already corrects for this on our own copy of the stack
// (skipOwnLeadingFrames); the event never got the same correction.
//
// AdvanceContextPastCrashPlumbing() walks the synthetic context forward until
// the frame is no longer one of ours, and sentry is handed that.
//
// What is modelled here is the DECISION -- which frames are skipped, and every
// condition that must abandon the walk and keep the original context. What is
// NOT modelled, and cannot be from a test: RtlVirtualUnwind running against a
// real stack inside a dying process. That is the part to be sceptical of.
//
// The design rule the cases below exist to pin: this must fail SAFE. Any doubt
// at all -- a missing function entry, a stack pointer that does not move the
// right way, a walk that never escapes our own code -- keeps the original
// context, so the worst case is exactly today's behaviour and never worse.

#include <cstdio>
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

// One frame as the walk sees it.
struct Frame {
	unsigned long long rip;
	unsigned long long sp;
	bool plumbing; // function entry matches one of ours exactly
	bool hasEntry; // RtlLookupFunctionEntry found an entry
};

struct Context {
	unsigned long long rip;
	unsigned long long sp;
};

// Mirror of AdvanceContextPastCrashPlumbingImpl. Returns true if `ctx` was
// advanced; on false the caller keeps its original, so `ctx` must be untouched.
static bool AdvanceContext(Context *ctx, const std::vector<Frame> &stack)
{
	const Context original = *ctx;

	std::size_t index = 0;
	for (; index < stack.size(); ++index)
		if (stack[index].rip == ctx->rip)
			break;

	if (index == stack.size())
		return false;

	Context work = *ctx;
	unsigned long long lastSp = work.sp;

	for (int depth = 0; depth < 8; ++depth) {
		if (index >= stack.size())
			return false;

		if (!stack[index].plumbing)
			break;

		if (!stack[index].hasEntry)
			return false;

		++index;

		if (index >= stack.size())
			return false;

		work.rip = stack[index].rip;
		work.sp = stack[index].sp;

		// Stacks grow down: a frame we unwound TO must sit above the
		// one we came from.
		if (!work.rip || work.sp <= lastSp)
			return false;

		lastSp = work.sp;
	}

	if (index < stack.size() && stack[index].plumbing)
		return false; // never got clear of our own machinery

	if (work.rip == original.rip)
		return false; // nothing moved

	*ctx = work;
	return true;
}

/* ================================================================= */

// The purecall door, as it really stacks up. Addresses ascend because the
// stack grows down and we are walking outwards.
static std::vector<Frame> PurecallStack()
{
	return {
		{0x1000, 0x100, true, true},  // SentryAbortHandler
		{0x1100, 0x200, true, true},  // raise
		{0x1200, 0x300, true, true},  // abort
		{0x1300, 0x400, true, true},  // SentryPurecallHandler
		{0x1400, 0x500, true, true},  // _purecall
		{0x9000, 0x600, false, true}, // the code that actually broke
		{0x9100, 0x700, false, true},
	};
}

// --- The purecall door lands on the fault ----------------------------------
static void check_it_walks_past_the_whole_purecall_door()
{
	Context ctx{0x1000, 0x100};

	check(AdvanceContext(&ctx, PurecallStack()),
	      "the walk must succeed on the ordinary purecall stack");
	check(ctx.rip == 0x9000,
	      "it must land on the first frame that is not ours -- the code that actually broke -- not on _purecall and not on our handler");
}

// --- The plain abort door, which has no purecall leg ------------------------
static void check_it_walks_past_a_plain_abort()
{
	std::vector<Frame> stack = {
		{0x1000, 0x100, true, true},  // SentryAbortHandler
		{0x1100, 0x200, true, true},  // raise
		{0x1200, 0x300, true, true},  // abort
		{0x8000, 0x400, false, true}, // whoever called abort()
	};

	Context ctx{0x1000, 0x100};

	check(AdvanceContext(&ctx, stack), "a plain abort must advance too");
	check(ctx.rip == 0x8000,
	      "it must land on the caller of abort(), the three CRT/handler frames skipped");
}

// --- The SEH door must not be touched --------------------------------------
//
// There the CONTEXT comes from the OS at the fault point and we are not on the
// stack at all, so there is nothing to skip and advancing would be wrong.
static void check_a_stack_that_starts_at_the_fault_is_left_alone()
{
	std::vector<Frame> stack = {
		{0x9000, 0x100, false, true},
		{0x9100, 0x200, false, true},
	};

	Context ctx{0x9000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "a stack whose innermost frame is already the fault must report no change");
	check(ctx.rip == 0x9000 && ctx.sp == 0x100,
	      "and must leave the context exactly as it was");
}

// --- Fail safe: never escapes our own machinery -----------------------------
static void check_a_stack_that_is_all_ours_keeps_the_original()
{
	std::vector<Frame> stack = {
		{0x1000, 0x100, true, true},
		{0x1100, 0x200, true, true},
		{0x1200, 0x300, true, true},
	};

	Context ctx{0x1000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "a walk that never reaches a frame outside our plumbing must fail rather than guess");
	check(ctx.rip == 0x1000 && ctx.sp == 0x100,
	      "and the original context must be untouched");
}

// --- Fail safe: no function entry -------------------------------------------
static void check_a_missing_function_entry_keeps_the_original()
{
	std::vector<Frame> stack = PurecallStack();
	stack[2].hasEntry = false; // abort has no unwind data

	Context ctx{0x1000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "a frame with no RUNTIME_FUNCTION cannot be unwound, so the walk must abandon and keep the original");
	check(ctx.rip == 0x1000, "original context preserved");
}

// --- Fail safe: the stack pointer must strictly increase --------------------
//
// The tell that an unwind has lost the stack. Without it a corrupted frame
// chain can loop or walk backwards into nonsense.
static void check_a_stack_pointer_that_does_not_rise_keeps_the_original()
{
	std::vector<Frame> stack = PurecallStack();
	stack[3].sp = stack[2].sp; // did not move

	Context ctx{0x1000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "a stack pointer that fails to strictly increase means the walk is lost and must abandon");
	check(ctx.rip == 0x1000, "original context preserved");

	// And strictly, not merely monotonically.
	std::vector<Frame> backwards = PurecallStack();
	backwards[2].sp = 0x50;

	Context ctx2{0x1000, 0x100};
	check(!AdvanceContext(&ctx2, backwards),
	      "a stack pointer that goes backwards must abandon");
}

// --- Fail safe: a null instruction pointer ----------------------------------
static void check_a_null_rip_keeps_the_original()
{
	std::vector<Frame> stack = PurecallStack();
	stack[4].rip = 0;

	Context ctx{0x1000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "unwinding to a null instruction pointer must abandon");
	check(ctx.rip == 0x1000, "original context preserved");
}

// --- Fail safe: the depth cap ------------------------------------------------
//
// The door is five frames deep. Anything past a handful means the walk is not
// describing the stack we think it is.
static void check_the_depth_cap_abandons_rather_than_running_on()
{
	std::vector<Frame> stack;
	for (int i = 0; i < 12; ++i)
		stack.push_back({(unsigned long long)(0x1000 + i * 0x100),
				 (unsigned long long)(0x100 + i * 0x100), true,
				 true});
	stack.push_back({0x9000, 0x2000, false, true});

	Context ctx{0x1000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "more plumbing frames than the door can have means the walk is wrong; it must abandon rather than keep going");
	check(ctx.rip == 0x1000, "original context preserved");
}

// --- Non-vacuity -------------------------------------------------------------
//
// If the skip rule were removed the walk would do nothing at all, and every
// case above would still "pass" by returning false. This proves the rule is
// what produces the movement.
static void check_the_skip_is_what_moves_the_context()
{
	std::vector<Frame> stack = PurecallStack();

	// Mark nothing as ours: there is then nothing to skip.
	for (auto &f : stack)
		f.plumbing = false;

	Context ctx{0x1000, 0x100};

	check(!AdvanceContext(&ctx, stack),
	      "non-vacuity: with no frame recognised as ours the context must not move, which is what makes the positive cases meaningful");
	check(ctx.rip == 0x1000, "original context preserved");
}

/* ================================================================= */

int main()
{
	check_it_walks_past_the_whole_purecall_door();
	check_it_walks_past_a_plain_abort();
	check_a_stack_that_starts_at_the_fault_is_left_alone();
	check_a_stack_that_is_all_ours_keeps_the_original();
	check_a_missing_function_entry_keeps_the_original();
	check_a_stack_pointer_that_does_not_rise_keeps_the_original();
	check_a_null_rip_keeps_the_original();
	check_the_depth_cap_abandons_rather_than_running_on();
	check_the_skip_is_what_moves_the_context();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all crash context advance checks passed\n");
	return 0;
}
