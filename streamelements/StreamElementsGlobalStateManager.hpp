#pragma once

#include "StreamElementsLocalFilesystemHttpServer.hpp"

#include "../../obs-browser/panel/browser-panel.hpp"

struct QCef;
struct QCefCookieManager;

#include "StreamElementsBrowserWidgetManager.hpp"
#include "StreamElementsMenuManager.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsBandwidthTestManager.hpp"
#include "StreamElementsOutputSettingsManager.hpp"
#include "StreamElementsWorkerManager.hpp"
#include "StreamElementsHotkeyManager.hpp"
#include "StreamElementsPerformanceHistoryTracker.hpp"
#include "StreamElementsAnalyticsEventsManager.hpp"
#include "StreamElementsCrashHandler.hpp"
#include "StreamElementsObsSceneManager.hpp"
#include "StreamElementsExternalSceneDataProviderManager.hpp"
#include "StreamElementsHttpClient.hpp"
#include "StreamElementsNativeOBSControlsManager.hpp"
#include "StreamElementsProfilesManager.hpp"
#include "StreamElementsBackupManager.hpp"
#include "StreamElementsCleanupManager.hpp"
#include "StreamElementsPreviewManager.hpp"
#include "StreamElementsWebsocketApiServer.hpp"

class StreamElementsGlobalStateManager : public StreamElementsObsAppMonitor {
private:
	QCef* m_cef;
	QCefCookieManager* m_cefCookieManager;

private:
	class WindowStateChangeEventFilter : public QObject {
	private:
		QObject *m_target;

	public:
		WindowStateChangeEventFilter(QObject *target) : m_target(target)
		{
			QCoreApplication::instance()->installEventFilter(this);
		}

		virtual ~WindowStateChangeEventFilter()
		{
			QCoreApplication::instance()->removeEventFilter(this);
		}

		virtual bool eventFilter(QObject *o, QEvent *e) override
		{
			if (o == m_target) {
				switch (e->type()) {
				case QEvent::Resize:
				case QEvent::Move:
				case QEvent::WindowStateChange:
					AdviseHostUserInterfaceStateChanged();
					break;
				}
			}

			return QObject::eventFilter(o, e);
		}
	};

private:
	StreamElementsGlobalStateManager();
	virtual ~StreamElementsGlobalStateManager();

public:
	static StreamElementsGlobalStateManager *GetInstance();

public:
	enum UiModifier { Default = 0, OnBoarding = 1, Import = 2 };

	void Initialize(QMainWindow *obs_main_window);
	void Shutdown();

	void Reset(bool deleteAllCookies = true,
		   UiModifier uiModifier = Default);
	void DeleteCookies();
	void StartOnBoardingUI(UiModifier uiModifier);
	void StopOnBoardingUI();
	void SwitchToOBSStudio();

	void PersistState(bool sendEventToGuest = true);
	void RestoreState();

	QCef *GetCef() { return m_cef; }

	StreamElementsBrowserWidgetManager *GetWidgetManager()
	{
		return m_widgetManager;
	}
	StreamElementsObsSceneManager *GetObsSceneManager()
	{
		return m_obsSceneManager;
	}
	StreamElementsMenuManager *GetMenuManager() { return m_menuManager; }
	StreamElementsBandwidthTestManager *GetBandwidthTestManager()
	{
		return m_bwTestManager;
	}
	StreamElementsOutputSettingsManager *GetOutputSettingsManager()
	{
		return m_outputSettingsManager;
	}
	StreamElementsWorkerManager *GetWorkerManager()
	{
		return m_workerManager;
	}
	StreamElementsHotkeyManager *GetHotkeyManager()
	{
		return m_hotkeyManager;
	}
	StreamElementsPerformanceHistoryTracker *GetPerformanceHistoryTracker()
	{
		return m_performanceHistoryTracker;
	}
	StreamElementsAnalyticsEventsManager *GetAnalyticsEventsManager()
	{
		return m_analyticsEventsManager;
	}
	StreamElementsExternalSceneDataProviderManager *
	GetExternalSceneDataProviderManager()
	{
		return m_externalSceneDataProviderManager;
	}
	StreamElementsHttpClient *GetHttpClient() { return m_httpClient; }
	StreamElementsLocalFilesystemHttpServer *GetLocalFilesystemHttpServer()
	{
		return m_localFilesystemHttpServer;
	}

	StreamElementsNativeOBSControlsManager *GetNativeOBSControlsManager()
	{
		return m_nativeObsControlsManager;
	}
	QCefCookieManager *GetCookieManager()
	{
		return m_cefCookieManager;
	}
	StreamElementsProfilesManager *GetProfilesManager()
	{
		return m_profilesManager;
	}
	StreamElementsBackupManager *GetBackupManager()
	{
		return m_backupManager;
	}
	StreamElementsCleanupManager *GetCleanupManager()
	{
		return m_cleanupManager;
	}
	StreamElementsPreviewManager* GetPreviewManager()
	{
		return m_previewManager;
	}
	StreamElementsWebsocketApiServer* GetWebsocketApiServer() {
		return m_websocketApiServer;
	}
	QMainWindow *mainWindow() { return m_mainWindow; }

public:
	bool DeserializeStatusBarTemporaryMessage(CefRefPtr<CefValue> input);
	bool DeserializePopupWindow(CefRefPtr<CefValue> input);
	bool DeserializeModalDialog(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);

	void ReportIssue();
	void UninstallPlugin();

	void SerializeUserInterfaceState(CefRefPtr<CefValue> &output);
	bool DeserializeUserInterfaceState(CefRefPtr<CefValue> input);

private:
	std::recursive_mutex m_mutex;
	long m_apiTransactionLevel = 0;

protected:
	virtual void OnObsExit() override;

private:
	bool m_persistStateEnabled = false;
	bool m_initialized = false;
	QMainWindow *m_mainWindow = nullptr;
	QWidget *m_nativeCentralWidget = nullptr;
	StreamElementsBrowserWidgetManager *m_widgetManager = nullptr;
	StreamElementsObsSceneManager *m_obsSceneManager = nullptr;
	StreamElementsMenuManager *m_menuManager = nullptr;
	StreamElementsBandwidthTestManager *m_bwTestManager = nullptr;
	StreamElementsOutputSettingsManager *m_outputSettingsManager = nullptr;
	StreamElementsWorkerManager *m_workerManager = nullptr;
	StreamElementsHotkeyManager *m_hotkeyManager = nullptr;
	StreamElementsPerformanceHistoryTracker *m_performanceHistoryTracker =
		nullptr;
	StreamElementsAnalyticsEventsManager *m_analyticsEventsManager =
		nullptr;
	StreamElementsCrashHandler *m_crashHandler = nullptr;
	StreamElementsExternalSceneDataProviderManager
		*m_externalSceneDataProviderManager = nullptr;
	StreamElementsHttpClient *m_httpClient = nullptr;
	StreamElementsLocalFilesystemHttpServer *m_localFilesystemHttpServer;
	StreamElementsNativeOBSControlsManager *m_nativeObsControlsManager =
		nullptr;
	StreamElementsProfilesManager *m_profilesManager = nullptr;
	StreamElementsBackupManager *m_backupManager = nullptr;
	StreamElementsCleanupManager *m_cleanupManager = nullptr;
	StreamElementsPreviewManager *m_previewManager = nullptr;
	StreamElementsWebsocketApiServer *m_websocketApiServer = nullptr;
	WindowStateChangeEventFilter *m_windowStateEventFilter = nullptr;

private:
	static StreamElementsGlobalStateManager *s_instance;

private:
	class ThemeChangeListener : public QDockWidget {
	public:
		ThemeChangeListener();

	protected:
		virtual void changeEvent(QEvent *event) override;

		std::string m_currentTheme;
	};

	class ApplicationStateListener : public QObject {
	public:
		ApplicationStateListener();
		~ApplicationStateListener();

	protected:
		void applicationStateChanged();

	private:
		QTimer m_timer;
	};

	QDockWidget *m_themeChangeListener;
	ApplicationStateListener *m_appStateListener;
};
