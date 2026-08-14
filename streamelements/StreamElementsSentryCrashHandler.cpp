#include "StreamElementsSentryCrashHandler.hpp"

#include "StreamElementsConfig.hpp"
#include "StreamElementsCrashConsentDialog.hpp"
#include "StreamElementsCrashContext.hpp"
#include "StreamElementsUtils.hpp"

#include <util/base.h>
#include <obs.h>
#include <obs-frontend-api.h>

#include <sentry.h>

#include <string>
#include <vector>

#include <stdio.h>
#include <wchar.h>

#include <windows.h>

// Empty means crash reporting is inert: sentry_init() is skipped entirely rather
// than run against a bad DSN, so a build without the DSN behaves like
// STREAMELEMENTS_CRASH_HANDLER=none instead of failing at runtime.
#ifndef SE_SENTRY_DSN
#define SE_SENTRY_DSN ""
#endif

/* ================================================================= */

static bool s_initialized = false;
static StreamElementsCrashContext *s_crashContext = nullptr;

// The filter sentry-native installed during sentry_init(). Running it is what
// emits the event and has the daemon write the minidump.
static LPTOP_LEVEL_EXCEPTION_FILTER s_sentryExceptionFilter = nullptr;

// The filter that was in place before sentry_init() -- OBS's own, which writes
// %appdata%\obs-studio\crashes\ and then exits the process. Captured separately
// because sentry's filter stores it but never calls it.
static LPTOP_LEVEL_EXCEPTION_FILTER s_hostExceptionFilter = nullptr;

static LONG s_insideExceptionFilter = 0L;

// Contact details from the last report the user filled in. Read once at
// startup, so the crash path never has to touch the config to prefill the
// prompt.
static std::string s_userName;
static std::string s_userEmail;
static std::string s_userDiscord;

/* ================================================================= */

//
// Reads the currently installed top-level exception filter without permanently
// disturbing it. Win32 offers no getter, so the only way to read it is to
// install something and put back what came out.
//
// There is a window here in which no filter is installed. This runs once, on the
// UI thread, while the plugin is loading, so a crash landing inside it would
// have to be exquisitely timed; the alternative -- not knowing OBS's filter --
// means losing OBS's crash log on every crash, which is strictly worse.
//
static LPTOP_LEVEL_EXCEPTION_FILTER PeekUnhandledExceptionFilter()
{
	auto current = SetUnhandledExceptionFilter(nullptr);

	SetUnhandledExceptionFilter(current);

	return current;
}

//
// Directory containing this DLL.
//
// The daemon path matters: left unset, sentry-native looks for sentry-crash.exe
// next to the *host executable* (obs64.exe), which is not a directory this
// plugin installs into. We ship the daemon beside our own binary and point the
// SDK at it.
//
static bool GetOwnModuleDirectory(std::wstring &result)
{
	HMODULE module = NULL;

	if (!::GetModuleHandleExW(
		    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    (LPCWSTR)&GetOwnModuleDirectory, &module)) {
		return false;
	}

	wchar_t path[MAX_PATH];
	DWORD len = ::GetModuleFileNameW(module, path, MAX_PATH);

	if (!len || len >= MAX_PATH)
		return false;

	wchar_t *lastSlash = wcsrchr(path, L'\\');

	if (!lastSlash)
		return false;

	*(lastSlash + 1) = L'\0';

	result = path;

	return true;
}

/* ================================================================= */

// Progress window.
//
// This runs on its own thread, and it has to. The crashing thread goes from
// here straight into Collect() and then blocks inside sentry's filter while
// the daemon captures the dump and the transport uploads the event, so it
// never pumps messages again. A window owned by that thread gets painted once
// by UpdateWindow() and never repaints; worse, Windows ghosts a window whose
// thread has not pumped for roughly five seconds and retitles it "(Not
// Responding)". That is exactly what "everything is stuck" looks like, and
// raising the shutdown timeout to 60s made that dead period longer.
//
// The sentry-crash daemon cannot help here: it is headless by design (it
// launches itself with SW_HIDE) and reports nothing back but its own log. So
// the phases below are the ones we can honestly observe ourselves -- the
// crashing thread publishes one and never waits on the UI.

enum {
	CRASH_PROGRESS_COLLECTING = 0,
	CRASH_PROGRESS_UPLOADING = 1,
};

static const wchar_t *const s_progressStatus[] = {
	L"Collecting diagnostic information…",
	L"Writing and uploading the crash report…",
};

#define WM_SE_CRASH_PROGRESS_DONE (WM_APP + 1)

static const UINT_PTR CRASH_PROGRESS_TIMER_ID = 1;
static const UINT CRASH_PROGRESS_TIMER_MS = 60;
static const int CRASH_PROGRESS_WIDTH = 460;
static const int CRASH_PROGRESS_HEIGHT = 156;

static HANDLE s_progressThread = NULL;
static HWND s_progressWindow = NULL;
static LONG s_progressPhase = CRASH_PROGRESS_COLLECTING;
static LONG s_progressPhaseTick = 0;
static LONG s_progressQuit = 0;

// Owned by the progress thread alone.
static HFONT s_progressTitleFont = NULL;
static HFONT s_progressTextFont = NULL;

static HFONT CreateProgressFont(HDC hdc, int pointSize, int weight)
{
	LOGFONTW lf;
	memset(&lf, 0, sizeof(lf));

	lf.lfHeight = -MulDiv(pointSize, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	lf.lfWeight = weight;
	lf.lfCharSet = DEFAULT_CHARSET;
	lf.lfQuality = CLEARTYPE_QUALITY;
	wcscpy_s(lf.lfFaceName, L"Segoe UI");

	return CreateFontIndirectW(&lf);
}

static void PaintCrashProgress(HDC hdc, const RECT &rc)
{
	const int width = rc.right - rc.left;
	const int margin = 18;

	FillRect(hdc, &rc, GetSysColorBrush(COLOR_WINDOW));
	SetBkMode(hdc, TRANSPARENT);

	RECT line = {margin, margin, width - margin, margin + 24};

	SelectObject(hdc, s_progressTitleFont);
	SetTextColor(hdc, GetSysColor(COLOR_WINDOWTEXT));
	DrawTextW(hdc, L"Sending your crash report", -1, &line,
		  DT_LEFT | DT_SINGLELINE | DT_NOPREFIX);

	// Status, with elapsed seconds once there is something to wait for.
	// A number that keeps moving is the whole point: it is the difference
	// between "working" and "hung".
	const LONG phase = InterlockedCompareExchange(&s_progressPhase, 0, 0);
	const LONG start =
		InterlockedCompareExchange(&s_progressPhaseTick, 0, 0);
	const DWORD elapsed = (GetTickCount() - (DWORD)start) / 1000;

	wchar_t status[256];
	if (elapsed >= 2)
		swprintf_s(status, L"%s  (%us)", s_progressStatus[phase],
			   (unsigned)elapsed);
	else
		wcscpy_s(status, s_progressStatus[phase]);

	line.top = margin + 30;
	line.bottom = line.top + 20;

	SelectObject(hdc, s_progressTextFont);
	DrawTextW(hdc, status, -1, &line,
		  DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_END_ELLIPSIS);

	// Indeterminate bar: there is no progress figure to report, so animate
	// rather than invent a percentage.
	RECT track = {margin, margin + 60, width - margin, margin + 68};
	FillRect(hdc, &track, GetSysColorBrush(COLOR_BTNFACE));

	const int trackWidth = track.right - track.left;
	const int chunkWidth = trackWidth / 4;
	const int travel = trackWidth + chunkWidth;
	const int period = 1600;
	const int offset =
		(int)((__int64)travel * (GetTickCount() % period) / period);

	RECT chunk = track;
	chunk.left = track.left + offset - chunkWidth;
	chunk.right = chunk.left + chunkWidth;

	if (chunk.left < track.left)
		chunk.left = track.left;
	if (chunk.right > track.right)
		chunk.right = track.right;

	if (chunk.right > chunk.left)
		FillRect(hdc, &chunk, GetSysColorBrush(COLOR_HIGHLIGHT));

	line.top = margin + 82;
	line.bottom = rc.bottom - margin;

	SetTextColor(hdc, GetSysColor(COLOR_GRAYTEXT));
	DrawTextW(hdc,
		  L"This can take up to a minute. OBS will close by itself "
		  L"once the report has been sent.",
		  -1, &line, DT_LEFT | DT_WORDBREAK | DT_NOPREFIX);
}

static LRESULT CALLBACK CrashProgressWndProc(HWND hwnd, UINT msg, WPARAM wp,
					     LPARAM lp)
{
	switch (msg) {
	case WM_CREATE: {
		HDC hdc = GetDC(hwnd);
		if (hdc) {
			s_progressTitleFont =
				CreateProgressFont(hdc, 11, FW_SEMIBOLD);
			s_progressTextFont =
				CreateProgressFont(hdc, 9, FW_NORMAL);
			ReleaseDC(hwnd, hdc);
		}
		SetTimer(hwnd, CRASH_PROGRESS_TIMER_ID, CRASH_PROGRESS_TIMER_MS,
			 NULL);
		return 0;
	}

	case WM_TIMER:
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;

	case WM_ERASEBKGND:
		return 1; // WM_PAINT covers the whole client area

	case WM_PAINT: {
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(hwnd, &ps);

		RECT rc;
		GetClientRect(hwnd, &rc);

		// Double buffered: the bar animates at ~16fps and flicker on a
		// crash dialog reads as instability.
		HDC memDC = CreateCompatibleDC(hdc);
		HBITMAP bmp =
			memDC ? CreateCompatibleBitmap(hdc, rc.right, rc.bottom)
			      : NULL;

		if (memDC && bmp) {
			HGDIOBJ old = SelectObject(memDC, bmp);
			PaintCrashProgress(memDC, rc);
			BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0,
			       SRCCOPY);
			SelectObject(memDC, old);
		} else {
			PaintCrashProgress(hdc, rc);
		}

		if (bmp)
			DeleteObject(bmp);
		if (memDC)
			DeleteDC(memDC);

		EndPaint(hwnd, &ps);
		return 0;
	}

	case WM_CLOSE:
		// Not the user's to dismiss -- the report is still in flight.
		return 0;

	case WM_SE_CRASH_PROGRESS_DONE:
		DestroyWindow(hwnd);
		return 0;

	case WM_DESTROY:
		KillTimer(hwnd, CRASH_PROGRESS_TIMER_ID);
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(hwnd, msg, wp, lp);
}

static DWORD WINAPI CrashProgressThreadProc(LPVOID param)
{
	HWND mainWindow = (HWND)param;

	WNDCLASSEXW wc;
	memset(&wc, 0, sizeof(wc));
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = CrashProgressWndProc;
	wc.hInstance = GetModuleHandleW(NULL);
	wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
	wc.hbrBackground = GetSysColorBrush(COLOR_WINDOW);
	wc.lpszClassName = L"SELiveCrashProgress";

	RegisterClassExW(
		&wc); // may already exist; CreateWindow reports failure

	int x, y;
	RECT r;

	// mainWindow was resolved on the crashing thread. GetWindowRect only
	// reads shared state, so a hung owner thread cannot block us here.
	if (mainWindow && GetWindowRect(mainWindow, &r)) {
		x = (r.left + r.right) / 2 - CRASH_PROGRESS_WIDTH / 2;
		y = (r.top + r.bottom) / 2 - CRASH_PROGRESS_HEIGHT / 2;
	} else {
		x = GetSystemMetrics(SM_CXSCREEN) / 2 -
		    CRASH_PROGRESS_WIDTH / 2;
		y = GetSystemMetrics(SM_CYSCREEN) / 2 -
		    CRASH_PROGRESS_HEIGHT / 2;
	}

	// No owner window on purpose: the owner's thread is the one that
	// crashed, and tying activation to it is how this ends up ghosted.
	HWND hwnd = CreateWindowExW(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
				    wc.lpszClassName, L"SE.Live",
				    WS_POPUP | WS_CAPTION, x, y,
				    CRASH_PROGRESS_WIDTH, CRASH_PROGRESS_HEIGHT,
				    NULL, NULL, wc.hInstance, NULL);

	if (!hwnd)
		return 0;

	InterlockedExchangePointer((PVOID volatile *)&s_progressWindow, hwnd);

	// Stop() may have run before the window existed.
	if (InterlockedCompareExchange(&s_progressQuit, 0, 0)) {
		DestroyWindow(hwnd);
		return 0;
	}

	SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
	UpdateWindow(hwnd);

	MSG msg;
	while (GetMessageW(&msg, NULL, 0, 0) > 0) {
		TranslateMessage(&msg);
		DispatchMessageW(&msg);
	}

	InterlockedExchangePointer((PVOID volatile *)&s_progressWindow, NULL);

	if (s_progressTitleFont)
		DeleteObject(s_progressTitleFont);
	if (s_progressTextFont)
		DeleteObject(s_progressTextFont);

	return 0;
}

static void SetCrashProgressPhase(LONG phase)
{
	// Tick first: the UI must never pair a new phase with a stale elapsed.
	InterlockedExchange(&s_progressPhaseTick, (LONG)GetTickCount());
	InterlockedExchange(&s_progressPhase, phase);
}

static void StartCrashProgress()
{
	if (s_progressThread)
		return;

	// Resolved here rather than on the progress thread: this is a libobs
	// call, and the crash path already makes it today.
	HWND mainWindow = (HWND)obs_frontend_get_main_window_handle();

	SetCrashProgressPhase(CRASH_PROGRESS_COLLECTING);

	s_progressThread = CreateThread(NULL, 0, CrashProgressThreadProc,
					(LPVOID)mainWindow, 0, NULL);
}

static void StopCrashProgress()
{
	if (!s_progressThread)
		return;

	InterlockedExchange(&s_progressQuit, 1);

	HWND hwnd = (HWND)InterlockedCompareExchangePointer(
		(PVOID volatile *)&s_progressWindow, NULL, NULL);

	if (hwnd)
		PostMessageW(hwnd, WM_SE_CRASH_PROGRESS_DONE, 0, 0);

	// Bounded wait: nothing on the crash path may hang on the UI, and the
	// process is about to be terminated regardless.
	WaitForSingleObject(s_progressThread, 2000);
	CloseHandle(s_progressThread);
	s_progressThread = NULL;
}

/* ================================================================= */

//
// Attributes that are worth having as searchable tags.
//
// Everything collected also goes into a structured context below, so this list
// is about query-ability, not completeness. It is kept short on purpose: with
// SENTRY_INTEGRATION_WER enabled, sentry_set_tag maps onto
// WerRegisterCustomMetadata, which is capped at WER_MAX_REGISTERED_METADATA (8)
// entries. The open-ended selive.api.<n>.* attributes must never become tags.
//
static bool IsTagWorthyAttribute(const std::string &name)
{
	return name == "product" || name == "selive.api.calls";
}

//
// Identifies the reporter on every event.
//
// The machine id is always present; name and email only once the user has
// supplied them on some earlier crash report. Called both at startup -- so a
// crash that never reaches the prompt still says who it came from -- and again
// after the prompt, with whatever was just typed.
//
static void SetSentryUser(const std::string &name, const std::string &email,
			  const std::string &discord)
{
	sentry_value_t user = sentry_value_new_object();

	sentry_value_set_by_key(
		user, "id",
		sentry_value_new_string(GetComputerSystemUniqueId().c_str()));

	if (name.size()) {
		sentry_value_set_by_key(user, "username",
					sentry_value_new_string(name.c_str()));
	}

	if (email.size()) {
		sentry_value_set_by_key(user, "email",
					sentry_value_new_string(email.c_str()));
	}

	// Not one of Sentry's recognised user fields, but the user object
	// preserves and displays additional keys -- and this belongs with the
	// rest of the reporter's identity rather than off in a context.
	if (discord.size()) {
		sentry_value_set_by_key(
			user, "discord",
			sentry_value_new_string(discord.c_str()));
	}

	sentry_set_user(user);
}

//
// Writes back what the user typed, so the next prompt starts prefilled.
//
// This runs on the crashing thread and writes the plugin's ini. It is one small
// file write, guarded so it only happens when there is something to store, and
// it sits on a path that is already about to zip the entire configuration tree
// -- but it is still file I/O on a dying process, which is why it is not done
// unconditionally.
//
static void PersistContactDetails(const std::string &name,
				  const std::string &email,
				  const std::string &discord)
{
	auto config = StreamElementsConfig::GetInstance();

	if (!config)
		return;

	if (name.size() && name != s_userName) {
		config->SetCrashReportUserName(name);

		s_userName = name;
	}

	if (email.size() && email != s_userEmail) {
		config->SetCrashReportUserEmail(email);

		s_userEmail = email;
	}

	if (discord.size() && discord != s_userDiscord) {
		config->SetCrashReportUserDiscord(discord);

		s_userDiscord = discord;
	}
}

//
// Puts everything StreamElementsCrashContext gathered onto the Sentry scope, so
// that the event sentry's filter builds a moment later carries it.
//
static void
ArmSentryScope(const StreamElementsCrashContext::Result &context,
	       const StreamElementsCrashConsentDialog::Result &consent)
{
	// Complete record, unabridged: a context is JSON in the event body, so
	// it has neither the WER entry cap nor BugSplat's hostility to long
	// attribute values.
	sentry_value_t attributes = sentry_value_new_object();

	for (auto &attribute : context.attributes) {
		sentry_value_set_by_key(
			attributes, attribute.name.c_str(),
			sentry_value_new_string(attribute.value.c_str()));

		if (IsTagWorthyAttribute(attribute.name)) {
			sentry_set_tag(attribute.name.c_str(),
				       attribute.value.c_str());
		}
	}

	sentry_set_context("selive", attributes);

	if (context.notes.size()) {
		sentry_value_t async = sentry_value_new_object();

		sentry_value_set_by_key(
			async, "callStack",
			sentry_value_new_string(context.notes.c_str()));

		sentry_set_context("async_call_context", async);
	}

	for (auto &attachment : context.attachments) {
		// The wide-char entry point, so paths with non-ANSI characters
		// in the user's temp directory survive.
		sentry_attach_filew(utf8_to_wstring(attachment.path).c_str());
	}

	//
	// What the user told us.
	//
	// Not sentry_capture_feedback(): that needs an event_id to attach to,
	// and the event is created downstream by sentry's own filter, so there
	// is no id to hand it at this point. On the scope it travels with the
	// event that filter builds.
	//
	SetSentryUser(consent.name, consent.email, consent.discord);

	if (consent.description.size()) {
		sentry_value_t report = sentry_value_new_object();

		sentry_value_set_by_key(
			report, "description",
			sentry_value_new_string(consent.description.c_str()));

		sentry_set_context("user_report", report);
	}
}

/* ================================================================= */

static LONG CALLBACK SentryExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
	if (pExceptionInfo->ExceptionRecord->ExceptionCode ==
	    EXCEPTION_STACK_OVERFLOW) {
		static ULONG stack_size = 0L;
		if (SetThreadStackGuarantee(&stack_size)) {
			stack_size += 1024 * 32; // add another 32KB

			SetThreadStackGuarantee(&stack_size);
		}
	}

	if (s_crashContext)
		s_crashContext->WalkStack(pExceptionInfo->ContextRecord);

	if (InterlockedIncrement(&s_insideExceptionFilter) == 1L) {
		// The gate. A stack that never passed through our code is not
		// ours to report: we skip sentry's filter entirely, so no event
		// and no minidump are produced, and go straight to the host
		// filter below. The user is not asked about a crash we were
		// never going to send.
		if (s_initialized && s_crashContext &&
		    s_crashContext->ShouldReport()) {
			// Ask first. Sentry uploads silently by default, and
			// going to Sentry without this would start sending
			// crash data from users who never agreed to it -- and
			// would lose the descriptions, which are frequently the
			// only account of what the user was actually doing.
			const auto consent =
				StreamElementsCrashConsentDialog::Prompt(
					s_userName, s_userEmail, s_userDiscord);

			if (consent.consented) {
				PersistContactDetails(consent.name,
						      consent.email,
						      consent.discord);

				// Only now: collecting the payload and waiting
				// on the daemon takes a moment, and there is
				// nothing to wait for if the user declined.
				StartCrashProgress();

				ArmSentryScope(s_crashContext->Collect(),
					       consent);

				if (s_sentryExceptionFilter) {
					SetCrashProgressPhase(
						CRASH_PROGRESS_UPLOADING);

					// Emits the event and signals the
					// sentry-crash daemon, which writes and
					// uploads the minidump out of process.
					// Returns once the daemon has captured
					// it, or on timeout.
					s_sentryExceptionFilter(pExceptionInfo);
				}

				// Before the host filter: OBS puts its own
				// crash dialog up, and a topmost window of
				// ours must not sit on top of it.
				StopCrashProgress();
			}
		}

		atexit([](void) {
			TerminateProcess(GetCurrentProcess(), 2);
			abort();
		});

		if (s_hostExceptionFilter) {
			// OBS's own handler: writes its crash log, then exits.
			// This does not return.
			s_hostExceptionFilter(pExceptionInfo);
		}

		// exit(-1); // <-- calling exit() may cause normal shutdown code to run, and we definitely do not want that in an exception handler
		TerminateProcess(GetCurrentProcess(), 1);
		abort();
	}

	InterlockedDecrement(&s_insideExceptionFilter);

	return EXCEPTION_CONTINUE_SEARCH;
}

/* ================================================================= */

StreamElementsSentryCrashHandler::StreamElementsSentryCrashHandler()
{
	if (s_initialized)
		return;

	if (IsDebuggerPresent())
		return;

	const std::string dsn = SE_SENTRY_DSN;

	if (dsn.empty()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: no Sentry DSN was compiled in; crash reporting is disabled");
		return;
	}

	// Must be read before sentry_init(), which replaces it and never calls
	// it again.
	s_hostExceptionFilter = PeekUnhandledExceptionFilter();

	sentry_options_t *options = sentry_options_new();

	sentry_options_set_dsn(options, dsn.c_str());

	// Matches the tag CI creates for the same build, so a Sentry release lines
	// up with the GitHub release and the CDN manifest version.
	const std::string release = "obs-streamelements-core@" +
				    GetStreamElementsPluginVersionString();
	sentry_options_set_release(options, release.c_str());

	std::wstring moduleDirectory;

	if (GetOwnModuleDirectory(moduleDirectory)) {
		const std::wstring daemonPath =
			moduleDirectory + L"sentry-crash.exe";

		sentry_options_set_handler_pathw(options, daemonPath.c_str());
	} else {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not resolve own module directory; falling back to the SDK's default sentry-crash.exe lookup, which searches next to obs64.exe");
	}

	// Stack plus heap around the crash site, roughly 5-10MB. BugSplat was
	// configured for full memory dumps, which on an OBS process carrying
	// browser sources is large enough to risk the upload cap; this is the
	// SDK's recommended middle setting and the one to revisit if crashes
	// turn out to need more.
	//
	// How long the SDK waits for in-flight uploads before forcing shutdown.
	//
	// Not a tuning knob: with the default, an observed crash produced a
	// complete 1MB minidump on disk and then lost it. The daemon log read
	//
	//   WinHttpSendRequest failed with code 12017
	//   background thread failed to shut down cleanly within timeout
	//
	// -- 12017 being ERROR_WINHTTP_OPERATION_CANCELLED, the request aborted
	// mid-flight because teardown began underneath it. The crash envelope was
	// not persisted for retry either, so the report was simply gone.
	//
	// 60s is deliberately generous. A minidump of this size on a slow or
	// congested uplink takes seconds at best, and the cost of waiting too
	// long is a delay in a process that is already terminating, against
	// losing the report entirely.
	//
	sentry_options_set_shutdown_timeout(options, 60000);

	sentry_options_set_minidump_mode(options, SENTRY_MINIDUMP_MODE_SMART);

	// Client-side stackwalk as the primary event, with the minidump attached
	// for deep debugging. Keeps the server-side symbolication path available
	// without giving up a readable stack when symbols are missing.
	sentry_options_set_crash_reporting_mode(
		options, SENTRY_CRASH_REPORTING_MODE_NATIVE_WITH_MINIDUMP);

	if (sentry_init(options) != 0) {
		blog(LOG_ERROR,
		     "obs-streamelements-core: StreamElements: Crash Handler: sentry_init() failed");
		return;
	}

	// Contact details the user gave on some previous crash report, if any.
	// Read here rather than on the crash path, both to prefill the prompt
	// and so that a crash which never reaches the prompt -- a fast-fail
	// caught by WER, say -- still says who it came from.
	auto config = StreamElementsConfig::GetInstance();

	if (config) {
		s_userName = config->GetCrashReportUserName();
		s_userEmail = config->GetCrashReportUserEmail();
		s_userDiscord = config->GetCrashReportUserDiscord();
	}

	SetSentryUser(s_userName, s_userEmail, s_userDiscord);

	// Whatever sentry_init() installed. Ours goes on top, so ours runs
	// first and this one is invoked by us, deliberately, only when the
	// module-of-interest gate passes.
	s_sentryExceptionFilter =
		SetUnhandledExceptionFilter(SentryExceptionFilter);

	s_crashContext = new StreamElementsCrashContext();

	s_initialized = true;

	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: Sentry initialized (%s)",
	     release.c_str());
}

void StreamElementsSentryCrashHandler::StopAsyncHangDetection()
{
	// No hang detection yet. sentry-native has its own app-hang tracking
	// which is worth evaluating against the currently disabled
	// HANG_DETECTION_ENABLED path in the BugSplat handler.
}

StreamElementsSentryCrashHandler::~StreamElementsSentryCrashHandler()
{
	// Deliberately not calling sentry_close(): the BugSplat handler likewise
	// never tears down, on the grounds that shutting the reporter down early
	// loses exceptions thrown during shutdown. See
	// StreamElementsGlobalStateManager::Shutdown().
	//
	// The exception filter is left installed for the same reason, and
	// s_crashContext is left alive because a filter running on another
	// thread may still be inside it.
}
