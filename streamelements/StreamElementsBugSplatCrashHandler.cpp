/*
 * This is the global OBS.Live crash handler, BugSplat backend.
 *
 * It operates by setting up a global exception filter (saving
 * the exception filter previously set by obs.dll).
 *
 * Once an exception occurs:
 *
 * 1. Our exception filter gets called. It walks the crashing
 *    thread's stack via StreamElementsCrashContext, which also
 *    decides whether that stack passed through a module of
 *    interest -- crashes that did not are never reported.
 * 2. If the crash is ours to report, BugSplat's unhandled
 *    exception handler is invoked, and composes the minidump.
 * 3. Our BugSplat crash callback is called. It asks
 *    StreamElementsCrashContext to gather the crash-time payload
 *    -- the OBS configuration ZIP, the walked stack, the async
 *    and API call context, system and performance snapshots --
 *    and registers it with BugSplat as attributes, notes and
 *    additional files.
 * 4. Our exception filter then chains to the exception filter
 *    obs.dll installed, so OBS still writes its own crash log.
 * 5. Next the program is terminated.
 */

#include "StreamElementsBugSplatCrashHandler.hpp"

#include "cef-headers.hpp"
#include "StreamElementsCrashContext.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"
#include <util/base.h>
#include <util/platform.h>
#include <obs-frontend-api.h>
#include <time.h>
#include <cstdio>
#include <string>
#include <windows.h>
#include <winuser.h>
#include "bugsplat.h"
#include <condition_variable>

#define HANG_DETECTION_ENABLED 0

#define STATUS_HANG_DETECTED 0xE0000001

/* ================================================================= */

const static DWORD g_dwBugSplatBaseFlags =
	MDSF_CUSTOMEXCEPTIONFILTER | MDSF_USEGUARDMEMORY | MDSF_LOGFILE |
	MDSF_LOG_VERBOSE |
	// MDSF_SUSPENDALLTHREADS |
	/* MDSF_NONINTERACTIVE |*/ // Doing this interactively apparently adds more reliability to actually delivering the crash reports to bugsplat...
	0;

/* ================================================================= */

static MiniDmpSender *s_mdSender = nullptr;
static StreamElementsCrashContext *s_crashContext = nullptr;
static LPTOP_LEVEL_EXCEPTION_FILTER s_prevExceptionFilter = nullptr;
static DWORD s_insideExceptionFilter = 0L;

static HWND hMessageWindow = NULL;

static void CreateMessageWindow(bool hang)
{
	if (hMessageWindow)
		return;

	auto mainWindowHandle = (HWND)obs_frontend_get_main_window_handle();

	if (!mainWindowHandle)
		return;

	RECT r;
	GetWindowRect(mainWindowHandle, &r);

	const int width = 600;
	const int height = 85;
	const int x = (r.left + r.right) / 2 - (width / 2);
	const int y = (r.top + r.bottom) / 2 - (height / 2);

	hMessageWindow = CreateWindowA(
		"Button",
		hang ? "Oops, something is not right. Please wait while a hang report is being prepared..."
		     : "Oops, something is not right. Please wait while a crash report is being prepared...",
		0, x, y, width, height, mainWindowHandle, NULL,
		GetModuleHandle(0), NULL);

	if (!hMessageWindow)
		return;

	SetWindowLong(hMessageWindow, GWL_STYLE, WS_VISIBLE);
	SetWindowLong(hMessageWindow, GWL_EXSTYLE, WS_EX_TOPMOST);
	EnableWindow(hMessageWindow, FALSE);
	ShowWindow(hMessageWindow, SW_SHOWNORMAL);
	UpdateWindow(hMessageWindow);
}

static void DestroyMessageWindow()
{
	if (!hMessageWindow)
		return;

	DestroyWindow(hMessageWindow);

	hMessageWindow = NULL;
}

/* ================================================================= */

static bool s_isNotesSet = false;

#if HANG_DETECTION_ENABLED

static std::condition_variable s_eventHangDetectionStop;
static std::mutex s_hangDetectionMutex;
static std::shared_ptr<std::thread> s_threadHangDetection = nullptr;
static bool s_isQtResponsive = true;

static void HangDetectionThread()
{
	bool isHanged = false;

	int consecutiveHangDetectionsCount = 0;

	const int MIN_CONSECUTIVE_HANG_DETECTIONS = 60;
	const int CHECK_INTERVAL_MS = 1000;

	std::time_t startTimestamp = std::time(nullptr);

	while (true) {
		{
			std::unique_lock lock(s_hangDetectionMutex);
			if (s_eventHangDetectionStop.wait_for(
				    lock, std::chrono::milliseconds(
						  CHECK_INTERVAL_MS)) ==
			    std::cv_status::no_timeout) {
				// We've been signalled to stop
				break;
			}
		}

		HWND hWnd = (HWND)obs_frontend_get_main_window_handle();

		if (!hWnd)
			continue;

		if (!IsWindow(hWnd))
			continue;

		if (IsHungAppWindow(hWnd)) {
			if (s_isQtResponsive) {
				consecutiveHangDetectionsCount = 0;

				s_isQtResponsive = false;

				QtDelayTask([&]() { s_isQtResponsive = true; },
					    CHECK_INTERVAL_MS);
			} else {
				++consecutiveHangDetectionsCount;
			}
		} else {
			consecutiveHangDetectionsCount = 0;
		}

		if (consecutiveHangDetectionsCount >=
		    MIN_CONSECUTIVE_HANG_DETECTIONS) {
			if (isHanged)
				continue;

			isHanged = true;

			blog(LOG_ERROR,
			     "[obs-streamelements-core]: main app window hang detected");

			if (IDYES != ::MessageBoxA(
				0,
				"Would you like to interrupt OBS Studio and send a hang report to StreamElements for analysis?\n\nClicking \"Yes\" will close OBS Studio, clicking \"No\" will continue waiting for the program to respond.",
				"Oops, OBS Studio appears to have stopped responding.",
				MB_YESNO | MB_ICONSTOP | MB_DEFBUTTON1 |
					    MB_SETFOREGROUND | MB_TOPMOST)) {
				// Stop hang detection thread completely, and don't show this message again.
				return;
			}

			CreateMessageWindow(true);

			std::time_t hangTimestamp = std::time(nullptr);
			auto secondsSinceStartup = unsigned long(std::chrono::seconds(hangTimestamp - startTimestamp).count());
			wchar_t secondsSinceStartupString[64];
			wsprintf(secondsSinceStartupString, L"%lu",
				 secondsSinceStartup);

			std::wstring notes =
				L"HANG on main app window detected " +
				std::wstring(secondsSinceStartupString) + L" seconds since startup";

			GetAsyncCallContextStack(
				[&](const StreamElementsAsyncCallContextStack_t
					    *asyncCallContextStack) {
					if (!asyncCallContextStack->size())
						return;

					notes +=
						L" ---- Asynchronous call context:";

					for (auto item :
					     *asyncCallContextStack) {
						char buf[32];
						notes += std::wstring(
								 L" ---- ") +
							 utf8_to_wstring(
								 item->file)
								 .c_str() +
							 utf8_to_wstring(
								 " (line: ") +
							 utf8_to_wstring(itoa(
								 item->line,
								 buf, 10)) +
							 L")\r\n";
					}
				});

			// Set report parameters
			s_mdSender->setDefaultUserDescription(
				L"OBS main window appears to have stopped responding. Please provide any additional information you might have here. What were you doing just before this happened?");
			s_mdSender->setNotes(
				notes.c_str());

			s_isNotesSet = true;

			DWORD tid = GetWindowThreadProcessId(hWnd, NULL);
			HANDLE hThread =
				OpenThread(THREAD_ALL_ACCESS, TRUE, tid);
			if (hThread != INVALID_HANDLE_VALUE) {
				SuspendThread(hThread);

				CONTEXT context;
				context.ContextFlags = CONTEXT_FULL;

				if (GetThreadContext(hThread,
							&context)) {
					EXCEPTION_RECORD
					exceptionRecord = {};
					exceptionRecord.ExceptionCode =
						STATUS_HANG_DETECTED;
					exceptionRecord.ExceptionFlags =
						0;
					exceptionRecord
						.ExceptionAddress =
						reinterpret_cast<void *>(
							context.Rip);
					exceptionRecord
						.NumberParameters = 0;

					EXCEPTION_POINTERS
						exceptionPointers = {};
					exceptionPointers
						.ExceptionRecord =
						&exceptionRecord;
					exceptionPointers.ContextRecord =
						&context;

					s_mdSender->setMiniDumpType(
						MiniDmpSender::BS_MINIDUMP_TYPE::
							MiniDumpWithFullMemory);

					s_mdSender->createReport(
						&exceptionPointers);

					DestroyMessageWindow();
				} else {
					s_mdSender->createReportAndExit();
				}

				CloseHandle(hThread);
			}

			abort();
		} else {
			isHanged = false;
		}
	}
}

static void StopHangDetection()
{
	{
		std::lock_guard<decltype(s_hangDetectionMutex)> lock(
			s_hangDetectionMutex);

		if (!s_threadHangDetection.get())
			return;
	}

	// Ask thread to stop
	s_eventHangDetectionStop.notify_one();

	s_threadHangDetection->join();
	s_threadHangDetection = nullptr;
}

static void StartHangDetection()
{
	StopHangDetection();

	std::lock_guard<decltype(s_hangDetectionMutex)> lock(
		s_hangDetectionMutex);

	s_threadHangDetection =
		std::make_shared<std::thread>(HangDetectionThread);
}

#endif

/* ================================================================= */

//
// Hands the crash-time payload to BugSplat in BugSplat's own vocabulary.
//
// Everything actually gathered here is backend-agnostic and now lives in
// StreamElementsCrashContext; this function is only the translation layer --
// UTF-8 in, wide strings out.
//
static inline void SubmitCrashContextToBugSplat()
{
	if (!s_crashContext || !s_mdSender)
		return;

	auto context = s_crashContext->Collect();

	for (auto &attribute : context.attributes) {
		s_mdSender->setAttribute(
			utf8_to_wstring(attribute.name).c_str(),
			utf8_to_wstring(attribute.value).c_str());
	}

	// Notes are set at most once per process, preserving the guard this code
	// has always carried.
	if (context.notes.size() && !s_isNotesSet) {
		s_mdSender->setNotes(utf8_to_wstring(context.notes).c_str());

		s_isNotesSet = true;
	}

	for (auto &attachment : context.attachments) {
		s_mdSender->sendAdditionalFile(
			utf8_to_wstring(attachment.path).c_str());
	}
}

/* ================================================================= */

static bool BugSplatExceptionCallback(UINT nCode, LPVOID lpVal1, LPVOID lpVal2)
{
	UNUSED_PARAMETER(lpVal1);
	UNUSED_PARAMETER(lpVal2);

	switch (nCode) {
	case MDSCB_EXCEPTIONCODE:
		//EXCEPTION_RECORD *p = (EXCEPTION_RECORD *)lpVal1;
		//DWORD code = p ? p->ExceptionCode : 0;

		SubmitCrashContextToBugSplat();
		break;
	}

	return false;
}

/* ================================================================= */

static LONG CALLBACK CustomExceptionFilter(PEXCEPTION_POINTERS pExceptionInfo)
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
#if HANG_DETECTION_ENABLED
		// Turn off hang detection so it doesn't pop up during minidump collection
		// s_mdSender->setFlags(g_dwBugSplatBaseFlags);
		StopHangDetection();
#endif

		CreateMessageWindow(false);

		if (s_mdSender && s_crashContext &&
		    s_crashContext->ShouldReport()) {
			s_mdSender->unhandledExceptionHandler(pExceptionInfo);
		}

		atexit([](void) {
			TerminateProcess(GetCurrentProcess(), 2);
			abort();
		});

		if (s_prevExceptionFilter) {
			// This will exit(0)
			s_prevExceptionFilter(pExceptionInfo);
		}

		// exit(-1); // <-- calling exit() may cause normal shutdown code to run, and we definitely do not want that in an exception handler
		TerminateProcess(GetCurrentProcess(), 1);
		abort();
	}

	InterlockedDecrement(&s_insideExceptionFilter);

	return EXCEPTION_CONTINUE_SEARCH;
}

/* ================================================================= */

StreamElementsBugSplatCrashHandler::StreamElementsBugSplatCrashHandler()
{
	if (IsDebuggerPresent()) {
		return;
	}

	std::wstring plugin_version =
		utf8_to_wstring(GetStreamElementsPluginVersionString());
	std::wstring obs_version = utf8_to_wstring(obs_get_version_string());

	std::wstring app_id = std::wstring(L"OBS ") + obs_version;
#ifdef _WIN64
	app_id += L" (64bit)";
#else
	app_id += L" (32bit)";
#endif

	// Constructed here rather than on the crash path: building it fetches
	// the remote module-of-interest list over HTTP.
	s_crashContext = new StreamElementsCrashContext();

	s_mdSender = new MiniDmpSender(L"OBS_Live", L"obs-streamelements-core",
				       plugin_version.c_str(), app_id.c_str(), g_dwBugSplatBaseFlags /* | MDSF_DETECTHANGS*/);

	// Enable full memory dumps
	s_mdSender->setMiniDumpType(
		MiniDmpSender::BS_MINIDUMP_TYPE::MiniDumpWithFullMemory); 

	// The following calls add support for collecting crashes for abort(), vectored exceptions, out of memory,
	// pure virtual function calls, and for invalid parameters for OS functions.
	// These calls should be used for each module that links with a separate copy of the CRT.
	SetGlobalCRTExceptionBehavior();
	SetPerThreadCRTExceptionBehavior(); // This call needed in each thread of your app

	// Set optional default values for user, email, and user description of the crash.
	//s_mdSender->setDefaultUserName(L"Anonymous");
	//s_mdSender->setDefaultUserEmail(L"");
	s_mdSender->setDefaultUserDescription(L"OBS crashed. Please provide any additional information you might have here. What were you doing just before this happened?");
	s_mdSender->setGuardByteBufferSize(1024 * 1024); // Allocate 1MB of guard buffer
	//s_mdSender->setHangDetectionTimeout(30000);

	s_mdSender->setCallback(BugSplatExceptionCallback);

	s_prevExceptionFilter =
		SetUnhandledExceptionFilter(CustomExceptionFilter);

#if HANG_DETECTION_ENABLED
	StartHangDetection();
#endif
}

void StreamElementsBugSplatCrashHandler::StopAsyncHangDetection()
{
#if HANG_DETECTION_ENABLED
	StopHangDetection();
#endif
}

StreamElementsBugSplatCrashHandler::~StreamElementsBugSplatCrashHandler()
{
	StopAsyncHangDetection();

	if (s_prevExceptionFilter) {
		// Unbind our exception filter
		SetUnhandledExceptionFilter(s_prevExceptionFilter);

		s_prevExceptionFilter = nullptr;
	}
}
