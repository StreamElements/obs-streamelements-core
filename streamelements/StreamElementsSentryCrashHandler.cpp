#include "StreamElementsSentryCrashHandler.hpp"

#include "StreamElementsConfig.hpp"
#include "StreamElementsCrashConsentDialog.hpp"
#include "StreamElementsCrashContext.hpp"
#include "StreamElementsWerRegistration.h"
#include "StreamElementsUtils.hpp"

#include <util/base.h>
#include <obs.h>
#include <obs-frontend-api.h>

#include <sentry.h>

#include <string>
#include <vector>

#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

// The CRT doors into a dying process: signal() for SIGABRT,
// _set_purecall_handler() for pure virtual calls, _set_new_handler() and
// _set_new_mode() for allocation failure. All of it is process-wide state
// shared with OBS, which is the point -- see the CORE-860 and CORE-863 blocks
// further down.
#include <signal.h>
#include <new.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <dwmapi.h>

// WerRegisterRuntimeExceptionModule, the fast-fail door (CORE-864).
#include <werapi.h>

// Needed only by the dark title bar on the progress window below.
#pragma comment(lib, "dwmapi.lib")

// WerRegisterRuntimeExceptionModule, for the fast-fail door (CORE-864).
#pragma comment(lib, "wer.lib")

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

// What killed us, read by HandleFatalException() to set crash.kind and written
// by the CRT handlers far below. Declared here only because the reader comes
// first in the file; the reasoning for each lives with the handler that sets it.
//
// Both are one-way: nothing clears them.
static volatile LONG s_abortIsPurecall = 0L; // the purecall door (CORE-860)
static volatile LONG s_sawOutOfMemory = 0L;  // the allocation door (CORE-863)

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

//
// Turns a possibly-relative path into an absolute one, anchored to the
// directory containing the host executable.
//
// obs_module_config_path() returns an absolute path on a normal install and a
// RELATIVE one on a portable install -- literally
// "../../config/obs-studio/plugin_config/obs-streamelements-core/sentry-db",
// which is relative to obs64.exe. Handed to sentry unchanged, it is resolved
// against the process's CURRENT WORKING DIRECTORY instead, and that is not ours
// to rely on: the host or any other plug-in may call SetCurrentDirectory at any
// time, and nothing announces it when they do.
//
// Today those two happen to agree, so this is hardening rather than a fix for
// an observed failure. It is cheap, and the failure mode it removes is a bad
// one: sentry__path_create_dir_all() fails, sentry_init() aborts, and the
// install has NO CRASH REPORTING AT ALL -- no handler, no gate, no consent
// prompt. See CORE-861.
//
// GetFullPathNameW alone would not do: it resolves against the current
// directory too. The anchor has to be the host executable's directory, because
// that is what OBS built the relative path against.
//
static std::wstring ResolveAgainstHostExecutable(const std::wstring &path)
{
	// Already absolute (a normal install): nothing to do.
	if (path.size() >= 2 &&
	    (path[1] == L':' || (path[0] == L'\\' && path[1] == L'\\')))
		return path;

	wchar_t exePath[MAX_PATH];
	DWORD len = ::GetModuleFileNameW(NULL, exePath, MAX_PATH);

	if (!len || len >= MAX_PATH)
		return path; // no better answer than what we were given

	wchar_t *lastSlash = wcsrchr(exePath, L'\\');

	if (!lastSlash)
		return path;

	*(lastSlash + 1) = L'\0';

	const std::wstring joined = std::wstring(exePath) + path;

	// Collapsing the ".." matters: sentry creates the directory chain one
	// prefix at a time, and a prefix that still contains ".." only works
	// from the right starting point.
	//
	// Sized dynamically because the result may exceed MAX_PATH -- see the
	// warning at the call site. A fixed buffer would silently return the
	// uncollapsed form here, which is the least useful of the three
	// possible answers.
	DWORD needed = ::GetFullPathNameW(joined.c_str(), 0, NULL, NULL);

	if (!needed)
		return joined;

	std::vector<wchar_t> buffer(needed);

	DWORD written =
		::GetFullPathNameW(joined.c_str(), needed, buffer.data(), NULL);

	if (!written || written >= needed)
		return joined;

	return std::wstring(buffer.data(), written);
}

/* ================================================================= */

//
// Routes sentry-native's own diagnostics into the OBS log.
//
// Every way sentry_init() can fail reports itself through SENTRY_WARN first --
// which database directory it could not create, which run folder it could not
// lock, which backend refused to start. All of it is compiled in, and all of it
// is discarded unless options->debug is set.
//
// Without this, a failed init produced exactly one line -- "sentry_init()
// failed" -- and no way to find out why short of attaching a debugger to a
// user's machine. That is how CORE-861 stayed invisible: crash reporting was
// entirely absent on an install and the log said nothing about the cause.
//
// So the logger is always installed, and the level rather than the switch is
// what varies: WARNING and above normally, so a failure always explains itself,
// and the full DEBUG stream under --setrace when someone is actually looking.
// The chatty part is DEBUG/INFO, and that stays off by default.
//
static void SentryLogger(sentry_level_t level, const char *message,
			 va_list args, void *userdata)
{
	UNUSED_PARAMETER(userdata);

	char buffer[2048];

	// vsnprintf always terminates, and a truncated diagnostic is still worth
	// more than none.
	vsnprintf(buffer, sizeof(buffer), message, args);

	int obsLevel;

	switch (level) {
	case SENTRY_LEVEL_FATAL:
	case SENTRY_LEVEL_ERROR:
		obsLevel = LOG_ERROR;
		break;
	case SENTRY_LEVEL_WARNING:
		obsLevel = LOG_WARNING;
		break;
	default:
		obsLevel = LOG_INFO;
		break;
	}

	blog(obsLevel,
	     "obs-streamelements-core: StreamElements: Crash Handler: sentry: %s",
	     buffer);
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

//
// Dark, unconditionally, matching StreamElementsCrashConsentDialog.cpp. This
// window is shown immediately before that dialog and again immediately after
// it, so a light panel beside a dark dialog reads as a glitch rather than a
// theme.
//
// The system colours this replaced (COLOR_WINDOW, COLOR_WINDOWTEXT,
// COLOR_BTNFACE, COLOR_HIGHLIGHT, COLOR_GRAYTEXT) follow the OS setting, which
// on a default Windows install means a white panel over a dark OBS.
//
static const COLORREF kProgressBackground = RGB(32, 32, 32);
static const COLORREF kProgressText = RGB(255, 255, 255);
static const COLORREF kProgressDimText = RGB(160, 160, 160);
static const COLORREF kProgressTrack = RGB(60, 60, 60);
static const COLORREF kProgressChunk = RGB(0, 120, 212);

//
// Leaked deliberately, like the dialog's brushes: they live as long as the
// window, the process is terminating behind it, and a brush destroyed mid-paint
// is a worse outcome than one never freed.
//
static HBRUSH GetProgressBrush(COLORREF color)
{
	static HBRUSH background = CreateSolidBrush(kProgressBackground);
	static HBRUSH track = CreateSolidBrush(kProgressTrack);
	static HBRUSH chunk = CreateSolidBrush(kProgressChunk);

	if (color == kProgressTrack)
		return track;

	if (color == kProgressChunk)
		return chunk;

	return background;
}

static void PaintCrashProgress(HDC hdc, const RECT &rc)
{
	const int width = rc.right - rc.left;
	const int margin = 18;

	FillRect(hdc, &rc, GetProgressBrush(kProgressBackground));
	SetBkMode(hdc, TRANSPARENT);

	RECT line = {margin, margin, width - margin, margin + 24};

	SelectObject(hdc, s_progressTitleFont);
	SetTextColor(hdc, kProgressText);
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
	FillRect(hdc, &track, GetProgressBrush(kProgressTrack));

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
		FillRect(hdc, &chunk, GetProgressBrush(kProgressChunk));

	line.top = margin + 82;
	line.bottom = rc.bottom - margin;

	SetTextColor(hdc, kProgressDimText);
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
	wc.hbrBackground = GetProgressBrush(kProgressBackground);
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

	// WS_CAPTION means there is a title bar, and the title bar is drawn by
	// the compositor rather than by PaintCrashProgress -- so without this it
	// stays light above a dark panel. 20 on Windows 10 2004 and later, 19
	// before it; try the current one and fall back rather than sniffing the
	// build number. Both simply fail on older systems.
	{
		const DWORD kUseImmersiveDarkMode = 20;
		const DWORD kUseImmersiveDarkModeBefore20H1 = 19;

		BOOL enabled = TRUE;

		if (FAILED(DwmSetWindowAttribute(hwnd, kUseImmersiveDarkMode,
						 &enabled, sizeof(enabled)))) {
			DwmSetWindowAttribute(hwnd,
					      kUseImmersiveDarkModeBefore20H1,
					      &enabled, sizeof(enabled));
		}
	}

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
// The handful of tags that identify the build, armed at startup rather than on
// the crash path.
//
// Everything else is collected when the process is dying and set in
// ArmSentryScope(). That is fine while the crash path is the only way an event
// is produced -- but CORE-860 was exactly the case where it was not, and those
// events arrived with no product, no version and no platform, which made them
// nearly impossible to place. These three cost nothing at startup and mean any
// future bypass still yields an attributable event.
//
// Kept to three: with SENTRY_INTEGRATION_WER, tags map onto
// WerRegisterCustomMetadata, capped at 8. These plus the two set on the crash
// path plus crash.kind leaves headroom.
//
static void ArmStableSentryTags()
{
	sentry_set_tag("product", "SE.Live");
	sentry_set_tag("obs_version", obs_get_version_string());

#if defined(_M_AMD64)
	sentry_set_tag("arch", "x86_64");
#elif defined(_M_ARM64)
	sentry_set_tag("arch", "arm64");
#else
	sentry_set_tag("arch", "x86");
#endif
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

//
//
// Deadline on the consent prompt (CORE-862).
//
// The prompt is a native Win32 modal dialog, and every Win32 modal loop
// dispatches the private message Qt's event dispatcher uses to drain its
// posted-event queue. So a queued call can run inside the prompt, and if it
// opens a QDialog::exec() its nested loop sits on top of ours and
// DialogBoxIndirectParamW cannot return until that unrelated dialog is
// answered. Observed with a cdb attach: the prompt had already been answered
// and hidden while USER32!DialogBox2 was still on the stack.
//
// The choke point in __QtPostTask_Impl stops OUR queued work from doing that.
// It cannot stop OBS's own posted calls or another plug-in's, and nothing can:
// a nested modal loop is above us on the stack, and no other thread can unwind
// it. EndDialog only marks the dialog for termination; it does not return.
//
// So the only bounded outcome available from outside is to stop waiting. The
// process has already crashed; leaving a zombie OBS holding the user's machine
// is worse than losing one report.
//
// Scoped to the prompt alone, and disarmed the moment it returns. Everything
// after it is already bounded -- the daemon wait by shutdown_timeout, the
// progress window by its own 2s join, and the host filter exits. Arming a
// single watchdog over the whole path would have to outlast a 60s upload, which
// makes it useless as a backstop.
//
// Generous on purpose: this must never fire for someone typing a description.
// Five minutes is far longer than that and far shorter than forever.
//
static const DWORD CRASH_PROMPT_DEADLINE_MS = 5 * 60 * 1000;

static HANDLE s_promptWatchdogThread = NULL;
static HANDLE s_promptWatchdogAnswered = NULL;

static DWORD WINAPI PromptWatchdogThreadProc(LPVOID)
{
	if (::WaitForSingleObject(s_promptWatchdogAnswered,
				  CRASH_PROMPT_DEADLINE_MS) == WAIT_OBJECT_0)
		return 0; // answered in time; nothing to do

	blog(LOG_ERROR,
	     "obs-streamelements-core: StreamElements: Crash Handler: the crash consent prompt has not returned after %lu seconds -- most likely blocked behind another modal dialog. Terminating rather than leaving the process hung; this report is lost. See CORE-862.",
	     (unsigned long)(CRASH_PROMPT_DEADLINE_MS / 1000));

	// Not exit() and not abort(): both run code on the way out, and abort()
	// would re-enter our own SIGABRT handler.
	::TerminateProcess(::GetCurrentProcess(), 3);

	return 0;
}

static void ArmPromptWatchdog()
{
	s_promptWatchdogAnswered = ::CreateEventW(NULL, TRUE, FALSE, NULL);

	if (!s_promptWatchdogAnswered)
		return; // no watchdog is better than a watchdog that fires early

	s_promptWatchdogThread = ::CreateThread(
		NULL, 0, PromptWatchdogThreadProc, NULL, 0, NULL);
}

static void DisarmPromptWatchdog()
{
	if (s_promptWatchdogAnswered)
		::SetEvent(s_promptWatchdogAnswered);

	// Deliberately not waiting on the thread and not closing the handles:
	// the wait above releases it, and a crash path must not block on its own
	// bookkeeping. The process is about to end regardless.
	s_promptWatchdogThread = NULL;
}

/* ================================================================= */

//
// The fast-fail door (CORE-864).
//
// Heap corruption (STATUS_HEAP_CORRUPTION) and every __fastfail
// (STATUS_STACK_BUFFER_OVERRUN -- /GS cookie failures, and the CRT's default
// invalid-parameter handler) bypass SEH entirely. The filter above never runs,
// the SIGABRT door never runs, and OBS's own handler never runs either. The
// process is simply gone.
//
// A WER runtime exception module is the only mechanism on Windows that sees
// them, because it runs out of process inside WerFault.exe once we are already
// dead. sentry-native ships one, but it has never registered here: its
// wer_default_path() looks for sentry-wer.dll beside the host executable, and
// we ship it beside the plug-in. Hence "Native WER module not found" in every
// log since the Sentry backend shipped.
//
// Shipping it where sentry looks would have registered it -- and it claims
// every fast-fail in the process, OBS's own and every other plug-in's, with no
// gate at all. That is exactly what CORE-860 exists to stop. So se-crash-wer.dll
// is registered instead: it applies the gate and the consent check, then
// forwards to sentry's module. See StreamElementsWerModule.cpp.
//
// What the plug-in side owes that arrangement is a registration struct that
// outlives everything, because WER hands its address to the module and the
// module reads it back out of our memory with ReadProcessMemory().
//

static SEWerRegistration s_werRegistration = {};
static bool s_werRegistered = false;

//
// Standing consent, as last answered at a crash prompt.
//
// The WER path cannot ask -- there is nobody left to ask -- so it reports on
// the strength of the previous answer, or not at all. Mirrored into the
// registration block so the module reads the current value rather than whatever
// happened to be true at startup.
//
static bool s_standingConsent = false;

static void SetStandingConsent(bool consented)
{
	// The in-memory half is free and always kept current.
	const bool changed = s_standingConsent != consented;

	s_standingConsent = consented;
	s_werRegistration.seConsent = consented ? 1U : 0U;

	if (!changed)
		return;

	// The ini write is not free: this runs on the crashing thread, on a
	// process that is already dying. Only when the answer actually changed,
	// for the same reason PersistContactDetails() guards its writes.
	auto config = StreamElementsConfig::GetInstance();

	if (config)
		config->SetCrashReportStandingConsent(consented);
}

//
// HKCU, deliberately: WER accepts a per-user allow-list, so this needs no
// elevation. Same key and same approach as sentry-native's own registration.
// Without the value, WerRegisterRuntimeExceptionModule still succeeds but
// WerFault declines to load the DLL.
//
static bool SetWerRegistryValue(const std::wstring &modulePath)
{
	const DWORD one = 1;

	const LSTATUS status = ::RegSetKeyValueW(
		HKEY_CURRENT_USER,
		L"Software\\Microsoft\\Windows\\Windows Error Reporting\\RuntimeExceptionHelperModules",
		modulePath.c_str(), REG_DWORD, &one, sizeof(one));

	return status == ERROR_SUCCESS;
}

//
// Fills in the registration block and registers the module.
//
// Must run after sentry_init(), because app_tid has to be the thread that
// called it: sentry's module derives the shared-memory names it uses to reach
// the sentry-crash daemon from (pid, that tid), and opening them under any
// other name finds nothing. And after the crash context exists, because the
// gate list comes from it.
//
static void RegisterWerModule(uint64_t sentryInitThreadId,
			      const std::vector<std::string> &modulesOfInterest)
{
	std::wstring moduleDirectory;

	if (!GetOwnModuleDirectory(moduleDirectory)) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not resolve own module directory; the WER module cannot be registered and fast-fail crashes will go unreported");
		return;
	}

	const std::wstring modulePath = moduleDirectory + L"se-crash-wer.dll";
	const std::wstring sentryWerPath = moduleDirectory + L"sentry-wer.dll";

	// Both DLLs have to be on disk, and saying which one is missing matters:
	// they are produced by two different CMake rules and packaged by two
	// different lines in main.nsi, so they can go missing independently.
	if (::GetFileAttributesW(modulePath.c_str()) ==
	    INVALID_FILE_ATTRIBUTES) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: se-crash-wer.dll is not next to the plug-in; fast-fail and heap-corruption crashes will go unreported");
		return;
	}

	if (::GetFileAttributesW(sentryWerPath.c_str()) ==
	    INVALID_FILE_ATTRIBUTES) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: sentry-wer.dll is not next to the plug-in; the gating module would have nothing to forward to, so it is not registered");
		return;
	}

	// --- sentry's half of the block. Its layout is not ours to choose. ---
	s_werRegistration.version = 1;
	s_werRegistration.app_pid = ::GetCurrentProcessId();
	s_werRegistration.app_tid = sentryInitThreadId;

	// --- ours ------------------------------------------------------------
	s_werRegistration.seMagic = SE_WER_MAGIC;
	s_werRegistration.seVersion = SE_WER_VERSION;
	s_werRegistration.seConsent = s_standingConsent ? 1U : 0U;

	::wcsncpy_s(s_werRegistration.seSentryWerPath,
		    _countof(s_werRegistration.seSentryWerPath),
		    sentryWerPath.c_str(), _TRUNCATE);

	s_werRegistration.seModuleCount = 0;

	for (const auto &name : modulesOfInterest) {
		if (s_werRegistration.seModuleCount >= SE_WER_MODULES_MAX) {
			blog(LOG_WARNING,
			     "obs-streamelements-core: StreamElements: Crash Handler: more than %d modules of interest; the WER gate will use the first %d and may decline a crash the in-process gate would have accepted",
			     SE_WER_MODULES_MAX, SE_WER_MODULES_MAX);
			break;
		}

		::strncpy_s(s_werRegistration
				    .seModules[s_werRegistration.seModuleCount],
			    SE_WER_MODULE_NAME_MAX, name.c_str(), _TRUNCATE);

		++s_werRegistration.seModuleCount;
	}

	if (!SetWerRegistryValue(modulePath)) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not add the WER module to the per-user allow-list; fast-fail and heap-corruption crashes will go unreported");
		return;
	}

	//
	// Below Windows 10 build 19041 this call succeeds but the out-of-process
	// callback is never invoked -- sentry refuses to register below the same
	// build for that reason. We register anyway and say so: the call is
	// harmless, and a version test here would have to duplicate sentry's,
	// which reads the real build number rather than the shimmed one that
	// GetVersionEx reports to an unmanifested host.
	//
	const HRESULT hr = ::WerRegisterRuntimeExceptionModule(
		modulePath.c_str(), &s_werRegistration);

	if (FAILED(hr)) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: WerRegisterRuntimeExceptionModule failed (0x%08lX); fast-fail and heap-corruption crashes will go unreported",
		     (unsigned long)hr);
		return;
	}

	s_werRegistered = true;

	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: WER module registered (requires Windows 10 build 19041 or later to fire); %d module(s) of interest, standing consent = %s",
	     (int)s_werRegistration.seModuleCount,
	     s_standingConsent ? "yes" : "no");
}

// The whole crash path, shared by the two ways a fatal condition reaches us:
// the top-level SEH filter and the SIGABRT handler below.
//
// `fromAbortDoor` says which. One flag rather than two, because the two things
// it decides follow from the same fact and must never disagree:
//
//   - the stack walk drops our own leading frames, because the abort door is
//     the one where we captured the CONTEXT ourselves and our handler is
//     therefore the innermost frame. SEH takes its CONTEXT from the OS at the
//     fault point, where we are not on the stack at all. See
//     StreamElementsCrashContext::WalkStack.
//
//   - crash.kind falls back to "abort" rather than "exception".
//
static LONG HandleFatalException(PEXCEPTION_POINTERS pExceptionInfo,
				 bool fromAbortDoor)
{
	//
	// First statement in the whole crash path, and it has to be (CORE-863).
	//
	// StreamElementsCrashContext reserves 1MB at construction so that
	// collection has headroom on a process that died of exhaustion. That
	// reserve used to be handed back on the first line of Collect() -- which
	// runs after the stack walk and after the consent prompt, both of which
	// allocate. On the one crash class the reserve exists for, it was not
	// available to the two things most likely to fail first.
	//
	// Safe to call more than once; the second call does nothing. The new
	// handler calls it too, earlier still.
	//
	StreamElementsCrashContext::ReleaseGuardBuffer();

	//
	// What killed us, on whichever door we came through.
	//
	// This lives here rather than in the SIGABRT handler, and that placement
	// was bought with a wasted test run: an uncaught std::bad_alloc does NOT
	// reach abort() on MSVC. `throw` becomes a real SEH exception
	// (0xE06D7363, RaiseException), so an unhandled one lands in the filter
	// above, not in the abort door. Tagging only in the abort handler meant
	// the OOM this was written for arrived untagged. Both doors funnel
	// through here, so this is the only correct place for it.
	//
	// Precedence, and it matters which way round:
	//
	//   purecall  proximate and certain -- the CRT told us moments ago that
	//             the call which failed was a pure virtual one.
	//
	//   oom       inferred and STICKY. It says an allocation failed at some
	//             point, not that it is why we are dying; a failure that was
	//             caught and handled leaves it set for the life of the
	//             process. So it must not outrank purecall, or it would
	//             relabel a later, unrelated crash. It does outrank the two
	//             below, which carry no cause at all.
	//
	//   abort     came through the SIGABRT door with no better explanation.
	//
	//   exception came through the SEH filter -- an access violation, or a
	//             C++ exception nobody caught.
	//
	if (s_initialized) {
		const char *kind = s_abortIsPurecall  ? "purecall"
				   : s_sawOutOfMemory ? "oom"
				   : fromAbortDoor    ? "abort"
						      : "exception";

		sentry_set_tag("crash.kind", kind);
	}

	if (pExceptionInfo->ExceptionRecord->ExceptionCode ==
	    EXCEPTION_STACK_OVERFLOW) {
		static ULONG stack_size = 0L;
		if (SetThreadStackGuarantee(&stack_size)) {
			stack_size += 1024 * 32; // add another 32KB

			SetThreadStackGuarantee(&stack_size);
		}
	}

	if (s_crashContext)
		s_crashContext->WalkStack(
			pExceptionInfo->ContextRecord,
			/*skipOwnLeadingFrames=*/fromAbortDoor);

	// Stops host API calls from running once we are on the crash path. The
	// consent prompt below is modal, and a modal message loop keeps
	// dispatching to this process -- including Qt's posted events, and so
	// the API calls queued before the fault. See
	// IsCrashReportingInProgress().
	SetCrashReportingInProgress();

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
			// Bounded, because the prompt can be blocked behind a
			// modal dialog we do not own. See ArmPromptWatchdog.
			ArmPromptWatchdog();

			const auto consent =
				StreamElementsCrashConsentDialog::Prompt(
					s_userName, s_userEmail, s_userDiscord);

			DisarmPromptWatchdog();

			// Whatever the user just decided also settles what
			// happens to the crashes that can never ask -- heap
			// corruption and fast-fail, which kill the process
			// before any handler of ours runs and are reported out
			// of process by the WER module. "Send report" grants
			// that; "Don't send" withdraws it. The prompt says so;
			// see StreamElementsCrashConsentDialog.cpp.
			SetStandingConsent(consent.consented);

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

static LONG CALLBACK SentryExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
{
	return HandleFatalException(pExceptionInfo, false);
}

/* ================================================================= */

//
// The second door (CORE-860).
//
// sentry_init() installs TWO crash entry points on Windows, not one --
// sentry-native/src/backends/native/sentry_crash_handler.c:
//
//   g_previous_filter = SetUnhandledExceptionFilter(crash_exception_filter);
//   sentry__win32_install_sigabrt_handler(crash_sigabrt_handler);
//
// The second is a plain signal(SIGABRT, ...). Displacing only the first left
// every abort() going straight to sentry: uploaded with no consent prompt, no
// module-of-interest gate and none of the payload, because all of that lives in
// the filter above. That is not a corner case -- abort() is how EVERY _purecall
// arrives, which is the entire double-destruction family behind CORE-777 and
// CORE-786. The one crash class we most need data on was the one class that
// arrived empty. Observed as SELIVE-E and SELIVE-K.
//
// So we take that door too, and route it into the same path.
//
// macOS never had this problem: the .mm handler installs its own handlers for
// all six signals, SIGABRT included, BEFORE sentry_init.
//

// Sentry's handle_sigabrt, displaced by ours. Only ever used if the shared path
// above returns, which it should not -- a bug there degrades to sentry's old
// behaviour rather than to silence.
static void(__cdecl *s_sentryAbortHandler)(int) = nullptr;

//
// Out of memory (CORE-863).
//
// The BugSplat integration installed five process-global CRT hooks -- purecall,
// SIGABRT, terminate, invalid-parameter and this one -- all routed to a
// deliberate null write so the exception filter would see them. The Sentry
// migration dropped all five; CORE-860 recovered the first two. This is the
// allocation one.
//
// Deliberately NOT what BugSplat did. Its memory_depleted() force-crashes the
// process on any allocation failure, and _set_new_mode(1) extends that to plain
// malloc -- which would preempt libobs's own bmalloc -> bcrash handling. A
// plug-in overriding the host's allocation-failure policy is not a call we get
// to make.
//
// So this observes and chains. Standard semantics are preserved exactly:
// returning 0 means operator new throws std::bad_alloc and malloc returns NULL,
// as they do today. If that goes unhandled it reaches abort() and the SIGABRT
// handler above, which can now say the crash was an OOM rather than a generic
// abort. If it IS handled, nothing is broken and nothing is reported -- a
// handled allocation failure is not a crash.
//
static _PNH s_previousNewHandler = nullptr;
static int s_previousNewMode = 0;

static int __cdecl SentryNewHandler(size_t size)
{
	InterlockedExchange(&s_sawOutOfMemory, 1L);

	// Now, not later. This is the moment the process has no memory, and the
	// crash path is about to want some -- the stack walk and the consent
	// prompt both allocate before Collect() runs. Safe to call more than
	// once; the second call does nothing.
	StreamElementsCrashContext::ReleaseGuardBuffer();

	// Nothing is logged here on purpose: blog() formats, and formatting
	// allocates, on the one code path where allocation is what failed.

	// Chain. If whoever held this handler before us can free something and
	// ask for a retry, that is a better outcome than a crash and it is
	// theirs to decide, not ours.
	if (s_previousNewHandler)
		return s_previousNewHandler(size);

	return 0;
}

static void __cdecl SentryAbortHandler(int signum)
{
	// abort() carries no exception context, so build the same synthetic
	// record sentry's own handler builds. RtlCaptureContext captures the
	// CALLER's context, so the innermost frame is this function -- hence
	// skipOwnLeadingFrames below.
	CONTEXT context;
	RtlCaptureContext(&context);

	EXCEPTION_RECORD record;
	memset(&record, 0, sizeof(record));
	record.ExceptionCode = STATUS_FATAL_APP_EXIT;
	record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
#if defined(_M_AMD64)
	record.ExceptionAddress = (PVOID)context.Rip;
#elif defined(_M_IX86)
	record.ExceptionAddress = (PVOID)context.Eip;
#elif defined(_M_ARM64)
	record.ExceptionAddress = (PVOID)context.Pc;
#endif

	EXCEPTION_POINTERS pointers;
	pointers.ContextRecord = &context;
	pointers.ExceptionRecord = &record;

	HandleFatalException(&pointers, true);

	// Only reachable on re-entry -- HandleFatalException terminates the
	// process on the first pass through. Hand back to sentry so a crash is
	// still reported, then make sure abort() cannot return to its caller.
	if (s_sentryAbortHandler)
		s_sentryAbortHandler(signum);

	TerminateProcess(GetCurrentProcess(), 3);
}

//
// Without this a pure virtual call arrives as an anonymous abort: the CRT's
// default _purecall handler just calls abort(), and the resulting event is
// titled after whatever the innermost resolvable symbol happens to be. Sentry
// grouped two unrelated ones together as "handle_sigabrt".
//
// Deliberately routed through abort() rather than duplicating the handler, so
// there is exactly one abort path to reason about. The _purecall/abort/raise
// frames stay in the walked stack, which is useful: they are what identifies
// the fault.
//
static void __cdecl SentryPurecallHandler(void)
{
	InterlockedExchange(&s_abortIsPurecall, 1L);

	abort();
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

	// Before anything that can fail, so that whatever fails says why.
	// See SentryLogger.
	sentry_options_set_debug(options, 1);
	sentry_options_set_logger(options, SentryLogger, nullptr);
	sentry_options_set_logger_level(
		options,
		IsTraceLogLevel() ? SENTRY_LEVEL_DEBUG : SENTRY_LEVEL_WARNING);

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

	// Where the SDK queues envelopes and minidumps until they are sent.
	//
	// Left unset, sentry-native uses `.sentry-native` relative to the
	// current working directory, which for an installed OBS is
	// `Program Files\obs-studio\bin\64bit`. A non-elevated user cannot
	// write there, so reports are lost before they are ever queued. This
	// was observed on a machine where that directory happened to be
	// writable: a complete minidump and its envelope sat in Program Files,
	// never sent.
	//
	// The plugin config directory is per-user, writable without elevation,
	// and already holds the rest of our state. The wide variant is
	// deliberate -- the narrow one reads the path in the ANSI code page,
	// which mangles a profile directory containing non-Latin characters.
	char *databasePath = obs_module_config_path("sentry-db");

	if (databasePath) {
		const std::wstring resolved = ResolveAgainstHostExecutable(
			utf8_to_wstring(databasePath));

		sentry_options_set_database_pathw(options, resolved.c_str());

		// Logged because a report that never arrives is diagnosed by
		// looking for a stranded envelope, and that is only possible if
		// the log says where to look.
		blog(LOG_INFO,
		     "obs-streamelements-core: StreamElements: Crash Handler: database path is %s",
		     wstring_to_utf8(resolved).c_str());

		// sentry uses plain CreateDirectoryW/CreateFileW with no \\?\
		// prefix, so once a path it needs reaches MAX_PATH it simply
		// cannot create it, sentry_init() fails, and the install has no
		// crash reporting at all.
		//
		// Prefixing \\?\ ourselves does not help: sentry builds the
		// directory chain one separator at a time, and the first prefix
		// of "\\?\C:\..." is "\", which CreateDirectoryW rejects. So
		// this is a real limit rather than something to work around
		// here, and the useful thing is to say so plainly instead of
		// leaving a silent absence of crash reports.
		//
		// The limit applies to what sentry creates INSIDE the database
		// directory, not to the directory itself -- observed failing at
		// a 244-character database path, on the run lock. The longest
		// child is "<uuid>.run\__sentry-breadcrumb1", 63 characters,
		// plus its separator.
		const size_t kLongestSentryChildPath = 64;

		if (resolved.size() + kLongestSentryChildPath >= MAX_PATH) {
			blog(LOG_WARNING,
			     "obs-streamelements-core: StreamElements: Crash Handler: the crash database path is %zu characters, leaving less than %zu before the Windows MAX_PATH limit of %d; sentry_init() is likely to fail and crash reporting to be unavailable. Install OBS to a shorter path.",
			     resolved.size(), kLongestSentryChildPath,
			     MAX_PATH);
		}

		bfree(databasePath);
	} else {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not resolve the plugin config directory; falling back to the SDK's default database path, which is relative to the working directory");
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

	//
	// The thread that calls sentry_init() is part of the WER module's
	// address book, not an incidental detail: sentry-native names the shared
	// memory it talks to the sentry-crash daemon over
	// "Local\\SentryCrash-<pid>-<tid>", with tid fixed at this moment. The
	// module opens it by that name and finds nothing if we hand it any
	// other thread. See RegisterWerModule().
	//
	const uint64_t sentryInitThreadId = (uint64_t)::GetCurrentThreadId();

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

		// The answer the WER path reports on, because it cannot ask.
		// False until the user has said yes to some earlier prompt.
		s_standingConsent = config->GetCrashReportStandingConsent();
	}

	SetSentryUser(s_userName, s_userEmail, s_userDiscord);

	ArmStableSentryTags();

	// Whatever sentry_init() installed. Ours goes on top, so ours runs
	// first and this one is invoked by us, deliberately, only when the
	// module-of-interest gate passes.
	s_sentryExceptionFilter =
		SetUnhandledExceptionFilter(SentryExceptionFilter);

	// The other door sentry_init() installed. Same reasoning, same order:
	// ours on top, sentry's kept as the fallback. Without this, abort() --
	// and therefore every _purecall -- goes straight to sentry, ungated and
	// unconsented. See the CORE-860 block above.
	s_sentryAbortHandler = signal(SIGABRT, SentryAbortHandler);

	if (s_sentryAbortHandler == SIG_ERR) {
		s_sentryAbortHandler = nullptr;

		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: could not install the SIGABRT handler; abort() crashes will bypass the consent prompt and the module-of-interest gate");
	}

	// Process-wide, and the displaced handler is deliberately dropped: the
	// CRT's default is "call abort()", which is where ours ends up anyway,
	// so there is nothing to chain to.
	_set_purecall_handler(SentryPurecallHandler);

	// The allocation door (CORE-863). Unlike purecall, this one CAN be
	// chained -- _set_new_handler hands back whoever held it -- and is,
	// because an upstream handler that can free memory and ask for a retry
	// should still win.
	s_previousNewHandler = _set_new_handler(SentryNewHandler);

	// Routes plain malloc failures through the handler above as well, not
	// just operator new. libobs allocates through bmalloc -> malloc, so
	// without this the largest allocator in the process is invisible to it.
	//
	// Not a behaviour change on its own: the handler returns 0 unless
	// something upstream asked for a retry, and malloc then returns NULL
	// exactly as it does today.
	s_previousNewMode = _set_new_mode(1);

	s_crashContext = new StreamElementsCrashContext();

	// Last, and in this order deliberately: the gate list is only final once
	// the context has been constructed -- its settings.json fetch is
	// synchronous and happens in there -- and app_tid is only meaningful
	// once sentry_init() has run. Copying a half-built list into the
	// registration block would give the two gates different answers.
	RegisterWerModule(sentryInitThreadId,
			  s_crashContext->GetModulesOfInterest());

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
