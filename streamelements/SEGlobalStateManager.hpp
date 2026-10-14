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
#include "SEApiServices.hpp"
#include "SEStatePersistence.hpp"
#include "SEStatusBarMessageManager.hpp"
#include "SEPopupWindowManager.hpp"
#include "SEBrowserDialogManager.hpp"
#include "SESessionManager.hpp"
#include "SEAuxiliaryControlsBuilder.hpp"
#include "SEHostEventDispatcher.hpp"
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
	void Initialize(QMainWindow *obs_main_window);
	void Shutdown();

private:
	//
	// Publishes the constructed services into m_apiServices. Called from
	// Initialize() once they all exist.
	//
	void PublishApiServices();

private:
	//
	// Private: asking the root whether it is alive is what a holder does
	// instead of holding what it uses, and every external caller has been
	// given the thing it actually wanted. The one caller left is
	// IsInstanceAvailable(), which answers the same question for the
	// static reaches that remain -- the SEUtils, composition and
	// crash-path ones, which later stages inject.
	//
	bool IsInitialized() { return m_initialized; }

public:


	QCef *GetCef() { return m_cef; }

	//
	// For handle_obs_frontend_event, which is a file static in the
	// implementation and is handed this object as its private_data.
	//
	std::shared_ptr<SEHostEventDispatcher> GetHostEventDispatcher()
	{
		return m_hostEventDispatcher;
	}
	std::shared_ptr<SEMessageBus> GetMessageBus() { return m_messageBus; }

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
	void UninstallPlugin();


protected:
	virtual void OnObsExit() override;

private:
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
	std::shared_ptr<SEApiServices> m_apiServices = nullptr;
	std::shared_ptr<SEStatePersistence> m_statePersistence = nullptr;
	std::shared_ptr<SEStatusBarMessageManager>
		m_statusBarMessageManager = nullptr;
	std::shared_ptr<SEPopupWindowManager> m_popupWindowManager =
		nullptr;
	std::shared_ptr<SEBrowserDialogManager> m_browserDialogManager =
		nullptr;
	std::shared_ptr<SESessionManager> m_sessionManager = nullptr;
	std::shared_ptr<SEAuxiliaryControlsBuilder>
		m_auxiliaryControlsBuilder = nullptr;
	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher = nullptr;
	std::shared_ptr<SEMessageBus> m_messageBus = nullptr;

private:
	static std::shared_ptr<
		SEGlobalStateManager> s_instance;

private:
	class ThemeChangeListener : public QDockWidget {
	public:
		ThemeChangeListener(
			std::shared_ptr<SEHostEventDispatcher>
				hostEventDispatcher,
			std::shared_ptr<SEMessageBus> messageBus);

	protected:
		virtual void changeEvent(QEvent *event) override;

		std::string m_currentTheme;

		//
		// A nested QDockWidget: Qt owns it once it is added to the
		// main window, so it holds what it announces with rather
		// than reaching for it when the theme changes.
		//
		std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;
		std::shared_ptr<SEMessageBus> m_messageBus;
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
