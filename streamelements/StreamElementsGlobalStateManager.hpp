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
#include "StreamElementsBrowserDialog.hpp"
#include "StreamElementsVideoCompositionManager.hpp"
#include "StreamElementsAudioCompositionManager.hpp"
#include "StreamElementsOutputManager.hpp"

class StreamElementsGlobalStateManager : public StreamElementsObsAppMonitor {
private:
	struct Private {
		explicit Private() = default;
	};

private:
	QCef* m_cef = nullptr;
	QCefCookieManager* m_cefCookieManager = nullptr;

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

public:
	StreamElementsGlobalStateManager(Private);
	virtual ~StreamElementsGlobalStateManager();

public:
	static std::shared_ptr<StreamElementsGlobalStateManager> GetInstance();
	static void Destroy();
	static bool IsInstanceAvailable();

public:
	enum UiModifier { Default = 0, OnBoarding = 1, Import = 2 };

	void Initialize(QMainWindow *obs_main_window);
	void Shutdown();

	bool IsInitialized() { return m_initialized; }

	void Reset(bool deleteAllCookies = true,
		   UiModifier uiModifier = Default);
	void DeleteCookies();
	void StartOnBoardingUI(UiModifier uiModifier);
	void StopOnBoardingUI();
	void SwitchToOBSStudio();

	void PersistState(bool sendEventToGuest = true);
	void RestoreState();

	QCef *GetCef() { return m_cef; }

	std::shared_ptr<StreamElementsBrowserWidgetManager> GetWidgetManager()
	{
		return m_widgetManager;
	}
	std::shared_ptr<StreamElementsObsSceneManager> GetObsSceneManager()
	{
		return m_obsSceneManager;
	}
	std::shared_ptr<StreamElementsMenuManager> GetMenuManager()
	{
		return m_menuManager;
	}
	std::shared_ptr<StreamElementsBandwidthTestManager>
	GetBandwidthTestManager()
	{
		return m_bwTestManager;
	}
	std::shared_ptr<StreamElementsOutputSettingsManager>
	GetOutputSettingsManager()
	{
		return m_outputSettingsManager;
	}
	std::shared_ptr<StreamElementsWorkerManager> GetWorkerManager()
	{
		return m_workerManager;
	}
	std::shared_ptr<StreamElementsHotkeyManager> GetHotkeyManager()
	{
		return m_hotkeyManager;
	}
	std::shared_ptr<StreamElementsPerformanceHistoryTracker>
	GetPerformanceHistoryTracker()
	{
		return m_performanceHistoryTracker;
	}
	std::shared_ptr<StreamElementsAnalyticsEventsManager>
	GetAnalyticsEventsManager()
	{
		return m_analyticsEventsManager;
	}
	std::shared_ptr<StreamElementsExternalSceneDataProviderManager>
	GetExternalSceneDataProviderManager()
	{
		return m_externalSceneDataProviderManager;
	}
	std::shared_ptr<StreamElementsHttpClient> GetHttpClient()
	{
		return m_httpClient;
	}
	std::shared_ptr<StreamElementsLocalFilesystemHttpServer>
	GetLocalFilesystemHttpServer()
	{
		return m_localFilesystemHttpServer;
	}

	std::shared_ptr<StreamElementsNativeOBSControlsManager>
	GetNativeOBSControlsManager()
	{
		return m_nativeObsControlsManager;
	}
	QCefCookieManager *GetCookieManager()
	{
		return m_cefCookieManager;
	}
	std::shared_ptr<StreamElementsProfilesManager> GetProfilesManager()
	{
		return m_profilesManager;
	}
	std::shared_ptr<StreamElementsBackupManager> GetBackupManager()
	{
		return m_backupManager;
	}
	std::shared_ptr<StreamElementsCleanupManager> GetCleanupManager()
	{
		return m_cleanupManager;
	}
	std::shared_ptr<StreamElementsPreviewManager> GetPreviewManager()
	{
		return m_previewManager;
	}
	std::shared_ptr<StreamElementsWebsocketApiServer>
	GetWebsocketApiServer()
	{
		return m_websocketApiServer;
	}
	std::shared_ptr<StreamElementsVideoCompositionManager> GetVideoCompositionManager()
	{
		return m_videoCompositionManager;
	}
	std::shared_ptr<StreamElementsAudioCompositionManager>
	GetAudioCompositionManager()
	{
		return m_audioCompositionManager;
	}
	std::shared_ptr<StreamElementsOutputManager> GetOutputManager()
	{
		return m_outputManager;
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

public:
	std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
	DeserializeNonModalDialog(CefRefPtr<CefValue> input);

	bool HasNonModalDialog(const char *id);
	std::string GetNonModalDialogUrl(const char *id);
	void SerializeAllNonModalDialogs(CefRefPtr<CefValue> &output);
	bool DeserializeCloseNonModalDialogsByIds(CefRefPtr<CefValue> input);
	bool DeserializeFocusNonModalDialogById(CefRefPtr<CefValue> input);
	bool DeserializeNonModalDialogDimensionsById(CefRefPtr<CefValue> dialogId, CefRefPtr<CefValue> dimensions);

private:
	std::map<std::string, StreamElementsBrowserDialog*>
		m_nonModalDialogs;

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
	std::shared_ptr<StreamElementsBrowserWidgetManager> m_widgetManager =
		nullptr;
	std::shared_ptr<StreamElementsObsSceneManager> m_obsSceneManager =
		nullptr;
	std::shared_ptr<StreamElementsMenuManager> m_menuManager = nullptr;
	std::shared_ptr<StreamElementsBandwidthTestManager> m_bwTestManager =
		nullptr;
	std::shared_ptr<StreamElementsOutputSettingsManager>
		m_outputSettingsManager = nullptr;
	std::shared_ptr<StreamElementsWorkerManager> m_workerManager = nullptr;
	std::shared_ptr<StreamElementsHotkeyManager> m_hotkeyManager = nullptr;
	std::shared_ptr<StreamElementsPerformanceHistoryTracker>
		m_performanceHistoryTracker = nullptr;
	std::shared_ptr<StreamElementsAnalyticsEventsManager>
		m_analyticsEventsManager = nullptr;
	StreamElementsCrashHandler *m_crashHandler = nullptr;
	std::shared_ptr<StreamElementsExternalSceneDataProviderManager>
		m_externalSceneDataProviderManager = nullptr;
	std::shared_ptr<StreamElementsHttpClient> m_httpClient = nullptr;
	std::shared_ptr<StreamElementsLocalFilesystemHttpServer>
		m_localFilesystemHttpServer;
	std::shared_ptr<StreamElementsNativeOBSControlsManager>
		m_nativeObsControlsManager = nullptr;
	std::shared_ptr<StreamElementsProfilesManager> m_profilesManager =
		nullptr;
	std::shared_ptr<StreamElementsBackupManager> m_backupManager = nullptr;
	std::shared_ptr<StreamElementsCleanupManager> m_cleanupManager =
		nullptr;
	std::shared_ptr<StreamElementsPreviewManager> m_previewManager =
		nullptr;
	std::shared_ptr<StreamElementsWebsocketApiServer> m_websocketApiServer =
		nullptr;
	std::shared_ptr<WindowStateChangeEventFilter> m_windowStateEventFilter =
		nullptr;
	std::shared_ptr<StreamElementsVideoCompositionManager>
		m_videoCompositionManager = nullptr;
	std::shared_ptr<StreamElementsAudioCompositionManager>
		m_audioCompositionManager = nullptr;
	std::shared_ptr<StreamElementsOutputManager> m_outputManager = nullptr;

private:
	static std::shared_ptr<
		StreamElementsGlobalStateManager> s_instance;

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

	QDockWidget *m_themeChangeListener = nullptr;
	ApplicationStateListener *m_appStateListener = nullptr;
};
