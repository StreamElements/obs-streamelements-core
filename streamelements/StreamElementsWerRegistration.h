//
// The registration block a WER runtime exception module reads back out of the
// crashed process. Shared verbatim between the plugin, which fills it in and
// hands its address to WerRegisterRuntimeExceptionModule(), and
// StreamElementsWerModule.cpp, which runs inside WerFault.exe and reads it
// back with ReadProcessMemory(). No dependencies either side: the module
// links neither libobs, nor Qt, nor sentry.
//
// See CORE-864.
//
// The layout is not free. Its first sixteen bytes MUST stay byte-for-byte
// identical to sentry-native's own sentry_wer_registration_t:
//
//     typedef struct {
//         DWORD version;      // must be 1
//         DWORD app_pid;
//         uint64_t app_tid;
//     } sentry_wer_registration_t;
//
// because our module forwards to sentry's sentry-wer.dll with the same context
// pointer, and sentry's read_registration() does a fixed-size read of exactly
// that struct. Everything after those sixteen bytes is ours and is invisible to
// sentry -- it never reads that far.
//
// Carrying our own data here, rather than in a sidecar file, is deliberate. The
// module runs in another process, after we are dead, with no way to call back
// into the plugin and nowhere to look for configuration; and the values it
// needs are exactly the values as of the moment of the crash. A snapshot in the
// crashed process's own memory is precisely that.
//

#pragma once

#include <windows.h>
#include <stdint.h>
#include <stddef.h>

// Ours, so a stale or foreign block cannot be mistaken for one of ours. "SEWR".
#define SE_WER_MAGIC 0x53455752U

// Bumped when the layout below changes. The module refuses anything it does not
// recognise rather than reading a struct it does not understand.
#define SE_WER_VERSION 1U

// The gate list. Thirty-two names is well past what the remote settings.json
// has ever carried, and the block is read cross-process in one shot, so the
// fixed sizing costs a few kilobytes of our own address space and buys a read
// with no allocation and no pointer chasing in a crashing process.
#define SE_WER_MODULES_MAX 32
#define SE_WER_MODULE_NAME_MAX 64

#pragma pack(push, 8)
typedef struct {
	//
	// --- sentry_wer_registration_t: do not reorder, do not resize --------
	//
	DWORD version;    // SENTRY's version field. Must be 1.
	DWORD app_pid;    // GetCurrentProcessId()
	uint64_t app_tid; // the thread that called sentry_init(), see below

	//
	// --- ours ------------------------------------------------------------
	//
	DWORD seMagic;   // SE_WER_MAGIC
	DWORD seVersion; // SE_WER_VERSION

	// Non-zero while our own crash handler is on the stack (CORE-968).
	//
	// Our frames being present is not evidence the crash is ours: when the
	// fault happens inside the handler they are there by construction. The
	// module needs to tell that case apart from a fault that genuinely
	// started in our code, and frame position alone cannot -- both have our
	// code innermost.
	//
	// So the handler publishes whether it was running. Read out of the
	// crashed process at the moment of the fault, it is exactly the fact the
	// gate needs, and it is the same thing the in-process walker relies on
	// when it arms SetSkipLeadingOwnFrames for the abort door and only
	// there.
	DWORD seHandlerActive;

	// Modules of interest, lower-cased base names with no extension,
	// mirroring the in-process gate in StreamElementsCrashContext. Sent
	// along because the remote settings.json can replace the built-in list
	// at runtime, and a gate that disagreed with the in-process one would
	// be worse than no gate at all.
	DWORD seModuleCount;
	char seModules[SE_WER_MODULES_MAX][SE_WER_MODULE_NAME_MAX];

	// Full path to sentry's own sentry-wer.dll, which does the actual work
	// once we have decided the crash is ours. The plugin resolves this at
	// init from its own module path; the module must not have to guess,
	// because sentry's wer_default_path() looks next to the host
	// executable and we deliberately do not ship it there.
	wchar_t seSentryWerPath[MAX_PATH];
} SEWerRegistration;
#pragma pack(pop)

//
// A compile-time restatement of the constraint in the comment above. If a
// future sentry-native reshapes sentry_wer_registration_t, or someone inserts a
// field into the prefix, this is what fails -- at build time, in both the
// plugin and the module, rather than as a WER module that silently stops
// matching the shared-memory name and reports nothing.
//
// offsetof(SEWerRegistration, seMagic) is the size of sentry's struct: 4 + 4
// padded to 8 for the uint64_t, plus 8 = 16.
//
#if defined(__cplusplus)
static_assert(offsetof(SEWerRegistration, version) == 0,
	      "sentry_wer_registration_t prefix: version must be first");
static_assert(offsetof(SEWerRegistration, app_pid) == 4,
	      "sentry_wer_registration_t prefix: app_pid must be at 4");
static_assert(offsetof(SEWerRegistration, app_tid) == 8,
	      "sentry_wer_registration_t prefix: app_tid must be at 8");
static_assert(offsetof(SEWerRegistration, seMagic) == 16,
	      "our fields must begin exactly where sentry's struct ends");
#else
_STATIC_ASSERT(offsetof(SEWerRegistration, version) == 0);
_STATIC_ASSERT(offsetof(SEWerRegistration, app_pid) == 4);
_STATIC_ASSERT(offsetof(SEWerRegistration, app_tid) == 8);
_STATIC_ASSERT(offsetof(SEWerRegistration, seMagic) == 16);
#endif
