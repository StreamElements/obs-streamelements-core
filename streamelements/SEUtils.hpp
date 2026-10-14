#pragma once

#include "SETrace.hpp"

#include "SEUtils/SEStringUtils.hpp"
#include "SEUtils/SECryptoUtils.hpp"
#include "SEUtils/SEDiagnosticsUtils.hpp"
#include "SEUtils/SECefValueUtils.hpp"
#include "SEUtils/SEEnvConfigUtils.hpp"
#include "SEUtils/SEPathUtils.hpp"
#include "SEUtils/SECancelableTask.hpp"
#include "SEUtils/SEHttpUtils.hpp"
#include "SEUtils/SEObsSceneUtils.hpp"
#include "SEUtils/SEObsDataUtils.hpp"
#include "SEUtils/SEObsEncoderUtils.hpp"
#include "SEUtils/SELazyOBSVideoEncoderProvider.hpp"

#include <cef-headers.hpp>
#include <obs.h>
#include <obs.hpp>
#include <obs-module.h>

#include <memory>
#include <iostream>
#include <mutex>
#include <string>
#include <cstdio>
#include <cstdarg>
#include <vector>
#include <map>

#include <functional>
#include <future>

#include <shared_mutex>

#include <util/threading.h>
#include <util/platform.h>
#include <curl/curl.h>

#include <QMenu>
#include <QDockWidget>
#include <QPointer>
#include <QWidget>

#define SYNC_ACCESS()                                                    \
	static std::recursive_mutex __sync_access_mutex;                 \
	std::lock_guard<std::recursive_mutex> __sync_access_mutex_guard( \
		__sync_access_mutex);

#include <QTimer>
#include <QApplication>
#include <QThread>
#include <QFile>
#include <QTextStream>
#include <QString>
#include <QWidget>

/* ========================================================= */

bool IsTraceLogLevel();

/* ========================================================= */


std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line);
std::future<void> __QtExecSync_Impl(std::function<void()> task,
				    std::string file, int line);

class QtAsyncCallFunctor {
private:
	typedef std::future<void> (*call_t)(std::function<void()> /*task*/,
					    std::string /*file*/, int /*line*/);

	std::string file;
	int line;
	call_t impl;

public:
	QtAsyncCallFunctor(const char *file_, const int line_,
			   const call_t impl_)
		: file(file_), line(line_), impl(impl_)
	{
	}

	std::future<void> operator()(std::function<void()> task) const
	{
		return impl(task, file, line);
	}
};

std::future<void> __QtDelayTask_Impl(std::function<void()> task, int delayMs,
				     const char *file, const int line);

#define QtDelayTask(task, delayMs) \
	__QtDelayTask_Impl(task, delayMs, __FILE__, __LINE__)
#define QtPostTask QtAsyncCallFunctor(__FILE__, __LINE__, &__QtPostTask_Impl)
#define QtExecSync QtAsyncCallFunctor(__FILE__, __LINE__, &__QtExecSync_Impl)

/* ========================================================= */

//
// UI teardown gate (CORE-786). Defined in obs-streamelements-core-plugin.cpp,
// which is where the detection lives; see the long comment there.
//
// False once OBS has been asked to close before our Initialize() completed --
// the update-thread race. In that state our object graph is half-built and
// destroying any of it corrupts the widget tree, so every UI teardown site
// must check this first.
//
bool SEIsUiTeardownSafe();

// Called at the end of Initialize(). After this, an ordinary close is an
// ordinary close and teardown proceeds normally.
void SENoteInitializeCompleted();

// False while Initialize() is on the stack, and after a close has been caught.
// Defined in obs-streamelements-core-plugin.cpp.
bool SEIsEventPumpAllowed();

// Use instead of QApplication::sendPostedEvents() everywhere: one choke point
// for every event pump the plug-in runs.
void SEDrainEventQueue();

// The only sanctioned way to destroy a QDockWidget. Deletes it when the gate
// above is open; otherwise detaches it from OBS's widget tree and leaks it,
// loudly. Destroying a dock while OBSInit() is still on the stack corrupts
// the tree, and by then obs_frontend_get_main_window() may already be null.
void SEDeleteDockWidgetWhenSafe(QPointer<QDockWidget> dock, const char *id,
				bool useDeleteLater);


/* ========================================================= */

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area);
std::string GetCommandLineOptionValue(const std::string key);
std::string LoadResourceString(std::string path);

/* ========================================================= */

void SerializeSystemTimes(CefRefPtr<CefValue> &output);
void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output);
void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output);

/* ========================================================= */

std::string GetCurrentThemeName();
std::string SerializeAppStyleSheet();
std::string GetAppStyleSheetSelectorContent(std::string selector);

/* ========================================================= */

std::string GetCefVersionString();
std::string GetCefPlatformApiHash();
std::string GetCefUniversalApiHash();
std::string GetSEPluginVersionString();
std::string GetSEApiVersionString();

/* ========================================================= */

std::string GetComputerSystemUniqueId();

/* ========================================================= */

std::string CreateSessionSignedAbsolutePathURL(std::wstring path);
bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path);

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window);
void SetAlwaysOnTop(QWidget *window, bool enable);

/* ========================================================= */

class RecursiveNestingLevelCounter {
public:
	RecursiveNestingLevelCounter(long *counter) : m_counter(counter)
	{
		m_level = os_atomic_inc_long(m_counter);
	}

	~RecursiveNestingLevelCounter() { os_atomic_dec_long(m_counter); }

	long level() { return m_level; }

private:
	long *m_counter;
	long m_level;
};

#define PREVENT_RECURSIVE_REENTRY()                                   \
	static long __recursive_nesting_level = 0L;                   \
	RecursiveNestingLevelCounter __recursive_nesting_level_guard( \
		&__recursive_nesting_level);                          \
	if (__recursive_nesting_level_guard.level() > 1)              \
		return;

double GetObsGlobalFramesPerSecond();

/* ========================================================= */

void AdviseHostUserInterfaceStateChanged();
void AdviseHostHotkeyBindingsChanged();

/* ========================================================= */

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId);

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId);

/* ========================================================= */

#if ENABLE_DECRYPT_COOKIES
void SEDecryptCefCookiesFile(const char *path_utf8);
void SEDecryptCefCookiesStoragePath(const char *path_utf8);
#endif /* ENABLE_DECRYPT_COOKIES */

/* ========================================================= */

bool DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				std::function<void()> defaultAction,
				std::function<void()> defaultContextMenu);

bool DeserializeDocksMenu(QMenu &menu);

bool DeserializeMenu(
	CefRefPtr<CefValue> input, QMenu &menu,
	std::function<void()> defaultAction = []() {},
	std::function<void()> defaultContextMenu = []() {});

QWidget *DeserializeAuxiliaryControlWidget(
	CefRefPtr<CefValue> input,
	std::function<void()> defaultAction = []() {},
	std::function<void()> defaultContextMenu = []() {});

QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
				     QPixmap *defaultPixmap = nullptr);

/* ========================================================= */

std::string CreateTimedObsApiTransaction(int timeoutMilliseconds = 60000);
void CompleteTimedObsApiTransaction(std::string id);

/* ========================================================= */

void RemoveObsDirtyShutdownMarker();

void RestartCurrentApplication();


/* ========================================================= */

void DispatchClientMessage(std::string target,
			   CefRefPtr<CefProcessMessage> msg);

void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson);

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson);

//
// True once teardown has begun and the Qt main thread has left its event loop
// for good.
//
// From that moment a cross-thread QtExecSync can never be served: the thread it
// is waiting for is inside our own shutdown and will not process another
// posted task. Anything still blocking on one deadlocks the shutdown it is
// blocking (CORE-1131).
//
bool IsShuttingDown();
void SetShuttingDown();

/* ========================================================= */

void DeserializeRevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

/* ========================================================= */
