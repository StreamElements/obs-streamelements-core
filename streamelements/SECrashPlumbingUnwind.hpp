//
// Advancing a captured CONTEXT past a leading run of crash-plumbing frames.
//
// Split out of SESentryCrashHandler.cpp for one reason: this is the half that
// cannot be reasoned about, only run. RtlLookupFunctionEntry and
// RtlVirtualUnwind against a real x64 stack are what decide whether the abort
// door's correction works, and a model of them says nothing about them -- the
// test that models the decision says so itself. Here, depending on nothing but
// windows.h, the real code compiles into tests/ and is driven by a real stack.
//
// WHICH frames are plumbing is NOT here. That belongs with the door that
// builds them, in SESentryCrashHandler.cpp, where a reviewer looks for it and
// where a new leg of the door would be added. This file knows only how to
// walk, which is also what makes it testable: a test supplies its own
// functions as the plumbing list and gets a real stack made of them.
//
// See CORE-2267.
//
#pragma once

#if defined(_M_AMD64)

#include <windows.h>
#include <stddef.h>

//
// Walks `context` forward while its instruction pointer lies inside one of the
// `plumbingCount` functions `plumbing` points at, and leaves it on the first
// frame that does not. Returns true only if the context moved.
//
// Matching is deliberately exact, not heuristic: a frame counts as plumbing
// only when its function entry is byte-identical to one of those listed, never
// because of the module it lives in -- which would also match the real fault
// whenever the fault is in our own code.
//
// Fails safe in every direction. No function entry, an unwind that faults, a
// null instruction pointer, a stack pointer that does not strictly increase,
// more than a handful of frames, or never leaving the listed functions at all
// -- any of those returns false and leaves `*context` untouched, so the worst
// case is exactly the uncorrected behaviour and never a worse one.
//
bool SEAdvanceContextPastPlumbing(CONTEXT *context, const void *const *plumbing,
				  size_t plumbingCount);

//
// True when `instructionPointer` lies inside one of the listed functions,
// by the same exact function-entry match the walk above uses.
//
// Exposed because the in-process module-of-interest gate needs the same
// question answered: on the abort door the innermost frames are the CRT's
// (`raise`, `abort`), so a gate that skips only frames belonging to this
// plug-in skips nothing and then latches on our own handler two frames
// down. Matching the DOOR rather than the MODULE is what separates the
// handler from the fault, and it is exact -- a module-name rule would also
// skip a genuine fault in our own code, which is silent under-reporting.
//
bool SEIsCrashPlumbingFrame(DWORD64 instructionPointer,
			    const void *const *plumbing, size_t plumbingCount);

#endif // _M_AMD64
