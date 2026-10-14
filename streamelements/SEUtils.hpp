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
#include "SEUtils/SEReentrancyUtils.hpp"
#include "SEUtils/SEAsyncTaskUtils.hpp"
#include "SEUtils/SELifecycleUtils.hpp"

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

void DispatchClientMessage(std::string target,
			   CefRefPtr<CefProcessMessage> msg);

void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson);

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson);


/* ========================================================= */

void DeserializeRevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

/* ========================================================= */
