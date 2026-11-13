#include "SECrashPlumbingUnwind.hpp"

#if defined(_M_AMD64)

static PVOID FunctionBaseOf(DWORD64 address)
{
	DWORD64 imageBase = 0;

	PRUNTIME_FUNCTION entry =
		RtlLookupFunctionEntry(address, &imageBase, NULL);

	if (!entry)
		return NULL;

	return (PVOID)(imageBase + entry->BeginAddress);
}

static bool IsPlumbingFrame(DWORD64 rip, const void *const *plumbing,
			    size_t plumbingCount)
{
	PVOID base = FunctionBaseOf(rip);

	if (!base)
		return false;

	for (size_t i = 0; i < plumbingCount; ++i) {
		if (!plumbing[i])
			continue;

		if (FunctionBaseOf((DWORD64)(uintptr_t)plumbing[i]) == base)
			return true;
	}

	return false;
}

static bool AdvanceContextPastPlumbingImpl(CONTEXT *context,
					   const void *const *plumbing,
					   size_t plumbingCount)
{
	CONTEXT work = *context;
	DWORD64 lastSp = work.Rsp;

	// Bounded: the door is five frames deep at most, so anything past a
	// handful means the walk is not describing the stack we think it is.
	for (int depth = 0; depth < 8; ++depth) {
		if (!IsPlumbingFrame(work.Rip, plumbing, plumbingCount))
			break;

		DWORD64 imageBase = 0;
		PRUNTIME_FUNCTION entry =
			RtlLookupFunctionEntry(work.Rip, &imageBase, NULL);

		if (!entry)
			return false;

		PVOID handlerData = NULL;
		DWORD64 establisherFrame = 0;

		RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, work.Rip, entry,
				 &work, &handlerData, &establisherFrame, NULL);

		// Stacks grow down, so a frame we unwound TO must sit above the
		// one we came from. Anything else means the walk is lost.
		if (!work.Rip || work.Rsp <= lastSp)
			return false;

		lastSp = work.Rsp;
	}

	// Never got clear of the listed functions, or nothing moved at all.
	if (IsPlumbingFrame(work.Rip, plumbing, plumbingCount) ||
	    work.Rip == context->Rip)
		return false;

	*context = work;
	return true;
}

bool SEIsCrashPlumbingFrame(DWORD64 instructionPointer,
			    const void *const *plumbing, size_t plumbingCount)
{
	if (!plumbing || !plumbingCount)
		return false;

	return IsPlumbingFrame(instructionPointer, plumbing, plumbingCount);
}

bool SEAdvanceContextPastPlumbing(CONTEXT *context, const void *const *plumbing,
				  size_t plumbingCount)
{
	if (!context || !plumbing || !plumbingCount)
		return false;

	// The stack this walks may be the reason the process is dying.
	__try {
		return AdvanceContextPastPlumbingImpl(context, plumbing,
						      plumbingCount);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

#endif // _M_AMD64
