//
// se-crash-wer.dll -- a WER runtime exception module that gates.
//
// CORE-864. Heap corruption and every __fastfail bypass SEH entirely: our
// top-level exception filter never sees them, the SIGABRT door never sees them,
// and OBS's own handler never sees them either. A WER runtime exception module
// is the only mechanism on Windows that can capture them at all, because it
// runs out of process, inside WerFault.exe, after the crashed process has
// already been suspended.
//
// sentry-native ships one of these (sentry-wer.dll) and it works. What it does
// not do is decide whether the crash was ours. It claims every fast-fail and
// every heap corruption in the host process -- OBS's own, and every other
// plugin's. Registering it directly would quietly undo the module-of-interest
// gate that CORE-860 exists to enforce, for one crash class, without anyone
// deciding to.
//
// So this module is registered instead, and sentry's is not registered at all:
// sentry looks for its module next to the host executable, and we deliberately
// ship it beside the plugin, where sentry will not find it. We do the two
// things sentry's module cannot:
//
//   decline unless the crashed thread's stack passes through one of our
//   modules,
//
// and only then hand the very same call to sentry's module, which owns the
// shared-memory handshake with the sentry-crash daemon that writes and uploads
// the minidump. We deliberately do not reimplement that handshake: it depends
// on sentry's internal sentry_crash_context.h layout, and a second copy of it
// here would be one more thing to keep in step across SDK upgrades.
//
// This file is built as its own DLL and links neither libobs, nor Qt, nor
// sentry. It is C++ only for `::`-qualified calls and static_assert; nothing
// here allocates through the CRT's C++ machinery or throws.
//
// Constraints it works under, all of them consequences of where it runs:
//
//   * We are inside WerFault.exe, a system process. There is no OBS log to
//     write to and nobody to show a message to. Diagnostics go to
//     OutputDebugString, which costs nothing when no debugger is listening.
//   * WER puts a time budget on this callback. The stack walk is bounded and
//     no symbol path is ever set, so nothing here can block on a symbol server.
//   * Claiming ownership suppresses WER's normal handling. We claim only what
//     sentry's module claims, and never on our own account.
//

#include "StreamElementsWerRegistration.h"

#include <werapi.h>
#include <dbghelp.h>
#include <psapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")

#ifndef STATUS_FAIL_FAST_EXCEPTION
#define STATUS_FAIL_FAST_EXCEPTION ((DWORD)0xC0000602)
#endif

#ifndef STATUS_STACK_BUFFER_OVERRUN
#define STATUS_STACK_BUFFER_OVERRUN ((DWORD)0xC0000409)
#endif

#ifndef STATUS_HEAP_CORRUPTION
#define STATUS_HEAP_CORRUPTION ((DWORD)0xC0000374)
#endif

// Deep enough to cross the CRT and OBS frames that sit above plugin code on a
// fast-fail, shallow enough that a corrupt stack cannot spin us. The in-process
// walker is bounded comparably.
#define SE_WER_MAX_FRAMES 128

// The crashed process's module table is read once per callback. OBS with
// browser sources loaded runs to several hundred modules.
#define SE_WER_MAX_MODULES 1024

//
// One OutputDebugStringA per line, deliberately. The debug output channel is
// system-wide and unsynchronised across processes, so a line emitted as
// prefix-then-body-then-newline can interleave with another process's and
// arrive unreadable. Composing first costs a stack buffer and makes each line
// atomic.
//
static void SEWerLog(const char *format, ...)
{
	char message[1024];
	char line[1088];
	va_list args;

	va_start(args, format);
	_vsnprintf_s(message, sizeof(message), _TRUNCATE, format, args);
	va_end(args);

	_snprintf_s(line, sizeof(line), _TRUNCATE, "se-crash-wer: %s\n",
		    message);

	::OutputDebugStringA(line);
}

/* ================================================================= */

//
// Module ranges of the CRASHED process.
//
// Names come from EnumProcessModulesEx rather than from dbghelp on purpose. The
// gate only needs to know which module a return address falls in, and a range
// table answers that with no allocation. Asking dbghelp for a symbol would mean
// touching PDBs -- and potentially a symbol server -- inside a callback WER is
// timing.
//
struct SEWerModuleRange {
	ULONG64 base;
	ULONG64 end;
	char name[SE_WER_MODULE_NAME_MAX];
};

static SEWerModuleRange s_modules[SE_WER_MAX_MODULES];
static int s_moduleCount = 0;

static void SEWerStripExtension(char *name)
{
	char *dot = ::strrchr(name, '.');

	if (dot)
		*dot = '\0';
}

static void SEWerLoadModuleRanges(HANDLE process)
{
	DWORD needed = 0;
	int count = 0;

	s_moduleCount = 0;

	HMODULE *handles =
		(HMODULE *)::calloc(SE_WER_MAX_MODULES, sizeof(HMODULE));

	if (!handles) {
		SEWerLog("could not allocate the module table");
		return;
	}

	if (!::EnumProcessModulesEx(
		    process, handles,
		    (DWORD)(SE_WER_MAX_MODULES * sizeof(HMODULE)), &needed,
		    LIST_MODULES_ALL)) {
		SEWerLog("EnumProcessModulesEx failed: %lu", ::GetLastError());
		::free(handles);
		return;
	}

	count = (int)(needed / sizeof(HMODULE));

	if (count > SE_WER_MAX_MODULES)
		count = SE_WER_MAX_MODULES;

	for (int index = 0; index < count; ++index) {
		MODULEINFO info;
		char base[MAX_PATH];

		if (!::GetModuleInformation(process, handles[index], &info,
					    sizeof(info)))
			continue;

		if (!::GetModuleBaseNameA(process, handles[index], base,
					  sizeof(base)))
			continue;

		// "obs-streamelements-core.dll" -> "obs-streamelements-core",
		// matching how the in-process gate compares.
		SEWerStripExtension(base);

		s_modules[s_moduleCount].base = (ULONG64)info.lpBaseOfDll;
		s_modules[s_moduleCount].end =
			(ULONG64)info.lpBaseOfDll + info.SizeOfImage;

		::strncpy_s(s_modules[s_moduleCount].name,
			    sizeof(s_modules[s_moduleCount].name), base,
			    _TRUNCATE);

		++s_moduleCount;
	}

	::free(handles);

	SEWerLog("enumerated %d modules in the crashed process", s_moduleCount);
}

static const char *SEWerModuleForAddress(ULONG64 address)
{
	for (int index = 0; index < s_moduleCount; ++index) {
		if (address >= s_modules[index].base &&
		    address < s_modules[index].end)
			return s_modules[index].name;
	}

	return NULL;
}

static BOOL SEWerIsModuleOfInterest(const SEWerRegistration *registration,
				    const char *moduleName)
{
	if (!moduleName)
		return FALSE;

	for (DWORD index = 0;
	     index < registration->seModuleCount && index < SE_WER_MODULES_MAX;
	     ++index) {
		if (::_stricmp(registration->seModules[index], moduleName) == 0)
			return TRUE;
	}

	return FALSE;
}

/* ================================================================= */

//
// The gate. Walks the crashed thread cross-process and reports whether any
// frame lands in a module we own.
//
static BOOL
SEWerStackTouchesModuleOfInterest(HANDLE process, HANDLE thread,
				  const CONTEXT *contextIn,
				  const SEWerRegistration *registration)
{
	// StackWalk64 writes through the context, so give it a copy. The one
	// WER handed us describes a process we do not own and must not modify.
	CONTEXT context = *contextIn;
	STACKFRAME64 frame;
	DWORD machine = 0;
	BOOL found = FALSE;

	::SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_NO_PROMPTS);

	// fInvadeProcess = TRUE so dbghelp knows the module list, which is what
	// the x64 unwinder needs in order to find function tables. No symbol
	// path is set and no symbol is ever requested, so this cannot reach out
	// to a symbol server.
	if (!::SymInitialize(process, NULL, TRUE)) {
		SEWerLog("SymInitialize failed: %lu (walking anyway)",
			 ::GetLastError());
	}

	::memset(&frame, 0, sizeof(frame));

#if defined(_M_AMD64)
	machine = IMAGE_FILE_MACHINE_AMD64;
	frame.AddrPC.Offset = context.Rip;
	frame.AddrFrame.Offset = context.Rbp;
	frame.AddrStack.Offset = context.Rsp;
#elif defined(_M_ARM64)
	machine = IMAGE_FILE_MACHINE_ARM64;
	frame.AddrPC.Offset = context.Pc;
	frame.AddrFrame.Offset = context.Fp;
	frame.AddrStack.Offset = context.Sp;
#else
	machine = IMAGE_FILE_MACHINE_I386;
	frame.AddrPC.Offset = context.Eip;
	frame.AddrFrame.Offset = context.Ebp;
	frame.AddrStack.Offset = context.Esp;
#endif

	frame.AddrPC.Mode = AddrModeFlat;
	frame.AddrFrame.Mode = AddrModeFlat;
	frame.AddrStack.Mode = AddrModeFlat;

	for (int index = 0; index < SE_WER_MAX_FRAMES; ++index) {
		if (!::StackWalk64(machine, process, thread, &frame, &context,
				   NULL, ::SymFunctionTableAccess64,
				   ::SymGetModuleBase64, NULL))
			break;

		if (!frame.AddrPC.Offset)
			break;

		const char *moduleName =
			SEWerModuleForAddress(frame.AddrPC.Offset);

		if (SEWerIsModuleOfInterest(registration, moduleName)) {
			SEWerLog("frame %d is in %s -- ours", index,
				 moduleName);

			found = TRUE;
			break;
		}
	}

	::SymCleanup(process);

	return found;
}

/* ================================================================= */

//
// bIsFatal is absent from older SDK definitions of
// WER_RUNTIME_EXCEPTION_INFORMATION, so its presence is established from dwSize
// before the field is touched. Same approach, and the same reason, as
// sentry_wer.c.
//
struct SEWerExceptionInformation19041 {
	DWORD dwSize;
	HANDLE hProcess;
	HANDLE hThread;
	EXCEPTION_RECORD exceptionRecord;
	CONTEXT context;
	PCWSTR pwszReportId;
	BOOL bIsFatal;
	DWORD dwReserved;
};

static BOOL SEWerIsFatal(const WER_RUNTIME_EXCEPTION_INFORMATION *info)
{
	if (!info ||
	    info->dwSize <= offsetof(SEWerExceptionInformation19041, bIsFatal))
		return FALSE;

	return ((const SEWerExceptionInformation19041 *)info)->bIsFatal;
}

//
// The codes that reach a WER module and nothing else. Kept identical to
// sentry_wer.c's is_native_wer_exception: claiming anything sentry would not
// claim would suppress WER's normal handling for a crash we then fail to
// report.
//
static BOOL SEWerIsNativeException(DWORD code)
{
	return code == STATUS_FAIL_FAST_EXCEPTION ||
	       code == STATUS_STACK_BUFFER_OVERRUN ||
	       code == STATUS_HEAP_CORRUPTION;
}

static BOOL SEWerReadRegistration(HANDLE process, PVOID address,
				  SEWerRegistration *registration)
{
	SIZE_T read = 0;

	if (!process || !address || !registration)
		return FALSE;

	if (!::ReadProcessMemory(process, address, registration,
				 sizeof(*registration), &read) ||
	    read != sizeof(*registration)) {
		SEWerLog("could not read the registration block: %lu",
			 ::GetLastError());
		return FALSE;
	}

	if (registration->seMagic != SE_WER_MAGIC ||
	    registration->seVersion != SE_WER_VERSION) {
		SEWerLog(
			"registration block is not ours (magic 0x%08lX, version %lu)",
			(unsigned long)registration->seMagic,
			(unsigned long)registration->seVersion);
		return FALSE;
	}

	return TRUE;
}

//
// Hands the call to sentry's own module, unchanged.
//
// The context pointer is passed through untouched: sentry's read_registration()
// does a fixed-size read of its own struct from that address, and our block
// begins with exactly that struct. Its verdict on ownership becomes ours -- we
// never claim on our own account.
//
static HRESULT SEWerForwardToSentry(const wchar_t *sentryWerPath, PVOID context,
				    PWER_RUNTIME_EXCEPTION_INFORMATION info,
				    BOOL *ownershipClaimed, PWSTR eventName,
				    PDWORD eventNameSize, PDWORD signatureCount)
{
	typedef HRESULT(WINAPI * SentryCallback)(
		PVOID, PWER_RUNTIME_EXCEPTION_INFORMATION, BOOL *, PWSTR,
		PDWORD, PDWORD);

	if (!sentryWerPath || !sentryWerPath[0]) {
		SEWerLog("no sentry-wer.dll path in the registration block");
		return S_OK;
	}

	HMODULE sentryModule = ::LoadLibraryW(sentryWerPath);

	if (!sentryModule) {
		SEWerLog("could not load sentry-wer.dll: %lu",
			 ::GetLastError());
		return S_OK;
	}

	SentryCallback callback = (SentryCallback)::GetProcAddress(
		sentryModule, "OutOfProcessExceptionEventCallback");

	if (!callback) {
		SEWerLog(
			"sentry-wer.dll has no OutOfProcessExceptionEventCallback");
		::FreeLibrary(sentryModule);
		return S_OK;
	}

	HRESULT result = callback(context, info, ownershipClaimed, eventName,
				  eventNameSize, signatureCount);

	SEWerLog("sentry-wer.dll returned 0x%08lX, claimed=%d",
		 (unsigned long)result, *ownershipClaimed ? 1 : 0);

	// Deliberately not freed. sentry's module has just handed the crash to
	// the sentry-crash daemon and WER may still call back into it; unloading
	// here would be a use-after-free for the sake of tidiness in a process
	// that is about to exit anyway.
	return result;
}

/* ================================================================= */

extern "C" {

HRESULT WINAPI OutOfProcessExceptionEventCallback(
	PVOID context, PWER_RUNTIME_EXCEPTION_INFORMATION exceptionInfo,
	BOOL *ownershipClaimed, PWSTR eventName, PDWORD eventNameSize,
	PDWORD signatureCount)
{
	SEWerRegistration registration;

	if (ownershipClaimed)
		*ownershipClaimed = FALSE;

	if (!exceptionInfo || !ownershipClaimed)
		return S_OK;

	const DWORD code = exceptionInfo->exceptionRecord.ExceptionCode;

	// Cheap tests first. Everything below this point costs a cross-process
	// module enumeration and a stack walk, and WER is timing us.
	if (!SEWerIsFatal(exceptionInfo) || !SEWerIsNativeException(code))
		return S_OK;

	if (!SEWerReadRegistration(exceptionInfo->hProcess, context,
				   &registration))
		return S_OK;

	SEWerLog("fatal 0x%08lX in pid %lu", (unsigned long)code,
		 (unsigned long)::GetProcessId(exceptionInfo->hProcess));

	//
	// Note what is deliberately NOT tested here: consent.
	//
	// This crash cannot ask -- the process is already gone -- and gating on
	// an answer to some earlier prompt would mean the first fast-fail a user
	// ever hits is always the one that goes unreported, which is the case
	// most worth having. So this class is reported on implicit consent, and
	// the crash-time prompt says so.
	//
	// It is a narrower disclosure than it sounds. A WER report is a minidump
	// plus the scope armed at startup; there is nobody left to collect the
	// configuration archive, the screenshot of the user's screen, or their
	// description of what they were doing. Those are what the prompt is
	// really asking about, and they are absent here by construction.
	//
	SEWerLoadModuleRanges(exceptionInfo->hProcess);

	//
	// The gate. A stack that never passed through our code is not ours to
	// report, exactly as in the in-process filter. Declining costs nothing
	// but not claiming ownership: WER carries on as though we were not
	// installed.
	//
	if (!SEWerStackTouchesModuleOfInterest(
		    exceptionInfo->hProcess, exceptionInfo->hThread,
		    &exceptionInfo->context, &registration)) {
		SEWerLog("no frame in a module of interest -- declining");
		return S_OK;
	}

	return SEWerForwardToSentry(registration.seSentryWerPath, context,
				    exceptionInfo, ownershipClaimed, eventName,
				    eventNameSize, signatureCount);
}

//
// WER only asks for signatures on a report we claimed, and sentry's module
// declines to supply any. Matching it keeps a report's bucketing identical to
// what sentry produces when its module is registered directly.
//
HRESULT WINAPI OutOfProcessExceptionEventSignatureCallback(
	PVOID context, PWER_RUNTIME_EXCEPTION_INFORMATION exceptionInfo,
	DWORD index, PWSTR name, PDWORD nameSize, PWSTR value, PDWORD valueSize)
{
	(void)context;
	(void)exceptionInfo;
	(void)index;
	(void)name;
	(void)nameSize;
	(void)value;
	(void)valueSize;

	return E_FAIL;
}

//
// We never want a debugger launched on a user's machine.
//
HRESULT WINAPI OutOfProcessExceptionEventDebuggerLaunchCallback(
	PVOID context, PWER_RUNTIME_EXCEPTION_INFORMATION exceptionInfo,
	PBOOL isCustomDebugger, PWSTR debuggerLaunch, PDWORD debuggerLaunchSize,
	PBOOL isDebuggerAutolaunch)
{
	(void)context;
	(void)exceptionInfo;
	(void)isCustomDebugger;
	(void)debuggerLaunch;
	(void)debuggerLaunchSize;
	(void)isDebuggerAutolaunch;

	return E_FAIL;
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved)
{
	(void)reserved;

	// Nothing to do, and nothing may safely be done: this loads inside
	// WerFault.exe while it is handling a crash.
	if (reason == DLL_PROCESS_ATTACH)
		::DisableThreadLibraryCalls(instance);

	return TRUE;
}
