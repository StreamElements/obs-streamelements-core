#pragma once

#include "SELocalFilesystemHttpServer.hpp"

#include "../../obs-browser/panel/browser-panel.hpp"

struct QCef;
struct QCefCookieManager;

#include "SEBrowserWidgetManager.hpp"
#include "SEMenuManager.hpp"
#include "SEConfig.hpp"
#include "SEObsAppMonitor.hpp"
#include "SEBandwidthTestManager.hpp"
#include "SEOutputSettingsManager.hpp"
#include "SEWorkerManager.hpp"
#include "SEHotkeyManager.hpp"
#include "SEPerformanceHistoryTracker.hpp"
#include "SEAnalyticsEventsManager.hpp"
#include "SECrashHandler.hpp"
#include "SEObsSceneManager.hpp"
#include "SEExternalSceneDataProviderManager.hpp"
#include "SEHttpClient.hpp"
#include "SENativeOBSControlsManager.hpp"
#include "SEProfilesManager.hpp"
#include "SEBackupManager.hpp"
#ifdef SE_ENABLE_WYVRN
#include "SERazerWyvrnManager.hpp"
#endif
#include "SECleanupManager.hpp"
#include "SEPreviewManager.hpp"
#include "SEWebsocketApiServer.hpp"
#include "SEBrowserDialog.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEAudioCompositionManager.hpp"
#include "SESharedVideoCompositionManager.hpp"
#include "SEOutputManager.hpp"
#include "SEObsActiveSceneTracker.hpp"

class SEGlobalStateManager : public SEObsAppMonitor {
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
				default:
					break;
				}
			}

			return QObject::eventFilter(o, e);
		}
	};

public:
	SEGlobalStateManager(Private);
	virtual ~SEGlobalStateManager();

public:
	static std::shared_ptr<SEGlobalStateManager> GetInstance();
	static void Destroy();

	// Releases the singleton WITHOUT running any destructor, by parking a
	// reference that is never freed. Used when OBS asked to close before
	// initialization completed: the object graph is half-built and running
	// its destructors corrupts the widget tree (CORE-786).
	static void Leak();
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

	std::shared_ptr<SEBrowserWidgetManager> GetWidgetManager()
	{
		return m_widgetManager;
	}
	std::shared_ptr<SEObsSceneManager> GetObsSceneManager()
	{
		return m_obsSceneManager;
	}
	std::shared_ptr<SEMenuManager> GetMenuManager()
	{
		return m_menuManager;
	}
	std::shared_ptr<SEBandwidthTestManager>
	GetBandwidthTestManager()
	{
		return m_bwTestManager;
	}
	std::shared_ptr<SEOutputSettingsManager>
	GetOutputSettingsManager()
	{
		return m_outputSettingsManager;
	}
	std::shared_ptr<SEWorkerManager> GetWorkerManager()
	{
		return m_workerManager;
	}
	std::shared_ptr<SEHotkeyManager> GetHotkeyManager()
	{
		return m_hotkeyManager;
	}
	std::shared_ptr<SEPerformanceHistoryTracker>
	GetPerformanceHistoryTracker()
	{
		return m_performanceHistoryTracker;
	}
	std::shared_ptr<SEAnalyticsEventsManager>
	GetAnalyticsEventsManager()
	{
		return m_analyticsEventsManager;
	}
	std::shared_ptr<SEExternalSceneDataProviderManager>
	GetExternalSceneDataProviderManager()
	{
		return m_externalSceneDataProviderManager;
	}
	std::shared_ptr<SEHttpClient> GetHttpClient()
	{
		return m_httpClient;
	}
	std::shared_ptr<SELocalFilesystemHttpServer>
	GetLocalFilesystemHttpServer()
	{
		return m_localFilesystemHttpServer;
	}

	std::shared_ptr<SENativeOBSControlsManager>
	GetNativeOBSControlsManager()
	{
		return m_nativeObsControlsManager;
	}
	QCefCookieManager *GetCookieManager()
	{
		return m_cefCookieManager;
	}
	std::shared_ptr<SEProfilesManager> GetProfilesManager()
	{
		return m_profilesManager;
	}
	std::shared_ptr<SEBackupManager> GetBackupManager()
	{
		return m_backupManager;
	}
#ifdef SE_ENABLE_WYVRN
	std::shared_ptr<SERazerWyvrnManager> GetRazerWyvrnManager()
	{
		return m_razerWyvrnManager;
	}
#endif
	std::shared_ptr<SECleanupManager> GetCleanupManager()
	{
		return m_cleanupManager;
	}
	std::shared_ptr<SEPreviewManager> GetPreviewManager()
	{
		return m_previewManager;
	}
	std::shared_ptr<SEWebsocketApiServer>
	GetWebsocketApiServer()
	{
		return m_websocketApiServer;
	}
	std::shared_ptr <SESharedVideoCompositionManager>
		GetSharedVideoCompositionManager()
	{
		return m_sharedVideoCompositionManager;
	}
	std::shared_ptr<SEVideoCompositionManager> GetVideoCompositionManager()
	{
		return m_videoCompositionManager;
	}
	std::shared_ptr<SEAudioCompositionManager>
	GetAudioCompositionManager()
	{
		return m_audioCompositionManager;
	}
	std::shared_ptr<SEOutputManager> GetOutputManager()
	{
		return m_outputManager;
	}
	std::shared_ptr<SEObsActiveSceneTracker>
		GetObsActiveSceneTracker()
	{
		return m_obsActiveSceneTracker;
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
	std::map<std::string, SEBrowserDialog*>
		m_nonModalDialogs;

private:
	std::recursive_mutex m_mutex;
	long m_apiTransactionLevel = 0;

protected:
	virtual void OnObsExit() override;

private:
	bool m_persistStateEnabled = false;
	bool m_initialized = false;
	QPointer<QMainWindow> m_mainWindow = nullptr;
	QPointer<QWidget> m_nativeCentralWidget = nullptr;
	std::shared_ptr<SEBrowserWidgetManager> m_widgetManager =
		nullptr;
	std::shared_ptr<SEObsSceneManager> m_obsSceneManager =
		nullptr;
	std::shared_ptr<SEMenuManager> m_menuManager = nullptr;
	std::shared_ptr<SEBandwidthTestManager> m_bwTestManager =
		nullptr;
	std::shared_ptr<SEOutputSettingsManager>
		m_outputSettingsManager = nullptr;
	std::shared_ptr<SEWorkerManager> m_workerManager = nullptr;
	std::shared_ptr<SEHotkeyManager> m_hotkeyManager = nullptr;
	std::shared_ptr<SEPerformanceHistoryTracker>
		m_performanceHistoryTracker = nullptr;
	std::shared_ptr<SEAnalyticsEventsManager>
		m_analyticsEventsManager = nullptr;
	SECrashHandler *m_crashHandler = nullptr;
	std::shared_ptr<SEExternalSceneDataProviderManager>
		m_externalSceneDataProviderManager = nullptr;
	std::shared_ptr<SEHttpClient> m_httpClient = nullptr;
	std::shared_ptr<SELocalFilesystemHttpServer>
		m_localFilesystemHttpServer;
	std::shared_ptr<SENativeOBSControlsManager>
		m_nativeObsControlsManager = nullptr;
	std::shared_ptr<SEProfilesManager> m_profilesManager =
		nullptr;
	std::shared_ptr<SEBackupManager> m_backupManager = nullptr;
#ifdef SE_ENABLE_WYVRN
	std::shared_ptr<SERazerWyvrnManager> m_razerWyvrnManager =
		nullptr;
#endif
	std::shared_ptr<SECleanupManager> m_cleanupManager =
		nullptr;
	std::shared_ptr<SEPreviewManager> m_previewManager =
		nullptr;
	std::shared_ptr<SEWebsocketApiServer> m_websocketApiServer =
		nullptr;
	std::shared_ptr<WindowStateChangeEventFilter> m_windowStateEventFilter =
		nullptr;
	std::shared_ptr<SESharedVideoCompositionManager>
		m_sharedVideoCompositionManager = nullptr;
	std::shared_ptr<SEVideoCompositionManager>
		m_videoCompositionManager = nullptr;
	std::shared_ptr<SEAudioCompositionManager>
		m_audioCompositionManager = nullptr;
	std::shared_ptr<SEOutputManager> m_outputManager = nullptr;
	std::shared_ptr<SEObsActiveSceneTracker>
		m_obsActiveSceneTracker = nullptr;

private:
	static std::shared_ptr<
		SEGlobalStateManager> s_instance;

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

	QPointer<QDockWidget> m_themeChangeListener = nullptr;
	ApplicationStateListener *m_appStateListener = nullptr;
};
