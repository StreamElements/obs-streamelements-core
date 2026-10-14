#include "SEGlobalStateManager.hpp"
#include "SEApiMessageHandler.hpp"
#include "SEUtils.hpp"
#include "Version.hpp"
#include "SEBrowserDialog.hpp"
#include "SEReportIssueDialog.hpp"

#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include "cef-headers.hpp"

#include <util/threading.h>
#include <util/util.hpp>

#include <QPushButton>
#include <QMessageBox>
#include <QGuiApplication>

#include "updater/updater.hpp"

#ifndef WIN32
#include <errno.h>
#include <string.h>
#endif

#include <future>

/* ========================================================================= */

static QString GetLastErrorMsg()
{
#ifdef WIN32
	LPWSTR bufPtr = NULL;
	DWORD err = GetLastError();
	FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
			       FORMAT_MESSAGE_FROM_SYSTEM |
			       FORMAT_MESSAGE_IGNORE_INSERTS,
		       NULL, err, 0, (LPWSTR)&bufPtr, 0, NULL);
	const QString result =
		(bufPtr) ? QString::fromUtf16((const ushort *)bufPtr).trimmed()
			 : QString("Unknown Error %1").arg(err);
	LocalFree(bufPtr);
	return result;
#else
	QString result = strerror(errno);

	return result;
#endif
}

/* ========================================================================= */

SEGlobalStateManager::ApplicationStateListener::
	ApplicationStateListener()
	: m_timer(this)
{
	uint32_t obsMajorVersion = obs_get_version() >> 24;

	if (obsMajorVersion < 22) {
		m_timer.setSingleShot(false);
		m_timer.setInterval(100);

		QObject::connect(&m_timer, &QTimer::timeout,
				 [this]() { applicationStateChanged(); });

		m_timer.start();
	}
}

SEGlobalStateManager::ApplicationStateListener::
	~ApplicationStateListener()
{
	uint32_t obsMajorVersion = obs_get_version() >> 24;

	if (obsMajorVersion < 22) {
		m_timer.stop();
	}
}

void SEGlobalStateManager::ApplicationStateListener::
	applicationStateChanged()
{
	// This is an unpleasant hack for OBS versions before
	// OBS 22.
	//
	// Older versions of OBS enabled global hotkeys only
	// when the OBS window was not focused to prevent
	// hotkey collision with text boxes and "natural"
	// program hotkeys. This mechanism relies on Qt's
	// QGuiApplication::applicationStateChanged event which
	// does not fire under certain conditions with CEF in
	// focus for the first time you set a hotkey.
	//
	// To mitigate, we re-enable background hotkeys
	// 10 times/sec for older OBS versions.
	//
	obs_hotkey_enable_background_press(true);
}

/* ========================================================================= */

static void SetMainWindowStyle()
{
	if (IsTraceLogLevel()) {
		blog(LOG_INFO, "SetMainWindowStyle");
	}

	QMainWindow* main = (QMainWindow*)obs_frontend_get_main_window();

	//qApp->setStyleSheet("QMainWindow::separator { border: none; padding: 0; margin: 0; }");
	//qApp->setStyleSheet("QDockWindow { margin: 0 4px 4px 4px; }");
//			dock->setStyleSheet("QDockWidget { margin: 0 -4px -4px -4px; }");
}

SEGlobalStateManager::ThemeChangeListener::ThemeChangeListener(
	std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
	std::shared_ptr<SEMessageBus> messageBus)
	: QDockWidget(),
	  m_hostEventDispatcher(hostEventDispatcher),
	  m_messageBus(messageBus)
{
	setVisible(false);
	setFloating(true);

	SetMainWindowStyle();
}

void SEGlobalStateManager::ThemeChangeListener::changeEvent(
	QEvent *event)
{
	if (event->type() == QEvent::StyleChange) {
		//SetMainWindowStyle();

		std::string newTheme = GetCurrentThemeName();

		if (newTheme != m_currentTheme) {
			json11::Json json = json11::Json::object{
				{"name", newTheme},
			};

			if (m_hostEventDispatcher) {
				m_hostEventDispatcher->DispatchJSEventGlobal(
					"hostUIThemeChanged", json.dump());
			}

			if (m_messageBus) {
				m_messageBus->NotifyExternalHostEvent(
					"hostUIThemeChanged", json.dump());
			}

			m_currentTheme = newTheme;
		}
	}
}

/* ========================================================================= */

static void handle_obs_frontend_event(enum obs_frontend_event event, void *data)
{
	//
	// Registered with the manager as its callback data, so this does not
	// have to look one up. Valid for as long as callbacks can arrive:
	// Shutdown() removes this one before the manager is destroyed, and the
	// Leak() path never destroys it at all.
	//
	auto self = static_cast<SEGlobalStateManager *>(data);

	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	UNUSED_PARAMETER(data);

	std::string name;
	std::string args = "null";

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
		name = "hostStreamingStarting";
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		name = "hostStreamingStarted";
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
		name = "hostStreamingStopping";
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		name = "hostStreamingStopped";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		name = "hostRecordingStarting";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STARTED:
		name = "hostRecordingStarted";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPING:
		name = "hostRecordingStopping";
		break;
	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		name = "hostRecordingStopped";
		break;
	case OBS_FRONTEND_EVENT_SCENE_CHANGED: {
		obs_source_t *source =
			SETRACE_ADDREF(obs_frontend_get_current_scene());

		if (source) {
			const char *sourceName = obs_source_get_name(source);

			if (sourceName) {
				json11::Json json = json11::Json::object{
					{"sceneId", GetIdFromPointer(source)},
					{"videoCompositionId",
					 self->GetVideoCompositionManager()
						 ->GetObsNativeVideoComposition()
						 ->GetId()},
					{"name", sourceName},
					{"width",
					 (int)obs_source_get_width(source)},
					{"height",
					 (int)obs_source_get_height(source)}};

				name = "hostActiveSceneChanged";
				args = json.dump();
			}

			obs_source_release(SETRACE_DECREF(source));
		}
		break;
	}
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		name = "hostSceneListChanged";
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		name = "hostSceneCollectionChanged";
		break;
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_LIST_CHANGED:
		name = "hostSceneCollectionListChanged";
		break;
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
		name = "hostProfileChanged";
		break;
	case OBS_FRONTEND_EVENT_PROFILE_LIST_CHANGED:
		name = "hostProfileListChanged";
		break;
	case OBS_FRONTEND_EVENT_EXIT:
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
		name = "hostExit";
		break;
	//
	// OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED was handled here once, to
	// force studio mode off while a central browser widget was visible:
	// the two compete for the same space, and the central widget is how
	// macOS support constrains us. The handler has been commented out for
	// a long time -- kept as prose rather than as dead code, because the
	// conflict it describes is real but the code it contained reached the
	// singleton for the widget manager.
	//
	default:
		return;
	}

	if (name.size()) {
		//
		// Both audiences, through the two services this object holds.
		// It is handed to the callback as private_data, so there is
		// nothing to ask whether the plugin is alive -- if it were
		// not, self would not be here.
		//
		if (auto dispatcher = self->GetHostEventDispatcher()) {
			dispatcher->DispatchJSEventGlobal(name, args);
		}

		if (auto msgBus = self->GetMessageBus()) {
			msgBus->NotifyExternalHostEvent(name, args);
		}
	}
}

/* ========================================================================= */

std::shared_ptr<SEGlobalStateManager> SEGlobalStateManager::s_instance =
	nullptr;

/* ========================================================================= */

SEGlobalStateManager::SEGlobalStateManager(
	SEGlobalStateManager::Private)
{
}

SEGlobalStateManager::~SEGlobalStateManager()
{
	Shutdown();

	//
	// The container is not emptied here. It used to be, because the
	// references formed a ring -- a service owning widgets, each widget
	// owning an API handler, each handler holding the container -- that
	// refcounting could not unwind, and a teardown step was the only
	// thing breaking it. Every one of those back-edges is a weak_ptr now,
	// so the container goes when this object goes and each service goes
	// when its last real user does.
	//
	// If a shutdown hang comes back, the fix is the weak edge that went
	// missing, not a line here: an owner deciding other objects'
	// lifetimes is what this refactor exists to remove.
	// check_nothing_but_the_manager_owns_the_services_container is the
	// structural guard, and
	// check_api_services_container_is_not_emptied_at_teardown is the
	// guard on this comment.
	//
}

std::shared_ptr<SEGlobalStateManager>
SEGlobalStateManager::GetInstance()
{
	if (!s_instance) {
		s_instance = std::make_shared<
			SEGlobalStateManager>(
			SEGlobalStateManager::Private());
	}

	return s_instance;
}

void SEGlobalStateManager::Destroy()
{
	s_instance = nullptr;
}

void SEGlobalStateManager::Leak()
{
	// Heap-allocated and never freed, so the refcount never reaches zero
	// and no destructor runs -- not now, and not during static destruction
	// at process exit either.
	if (s_instance.get())
		new std::shared_ptr<SEGlobalStateManager>(
			s_instance);

	s_instance = nullptr;
}

bool SEGlobalStateManager::IsInstanceAvailable()
{
	if (!s_instance.get())
		return false;

	return s_instance->IsInitialized();
}

#include <QWindow>
#include <QObjectList>

void SEGlobalStateManager::PublishApiServices()
{
	if (!m_apiServices)
		return;

	m_apiServices->cef = m_cef;
	m_apiServices->widgetManager = m_widgetManager;
	m_apiServices->obsSceneManager = m_obsSceneManager;
	m_apiServices->menuManager = m_menuManager;
	m_apiServices->bandwidthTestManager = m_bwTestManager;
	m_apiServices->outputSettingsManager = m_outputSettingsManager;
	m_apiServices->workerManager = m_workerManager;
	m_apiServices->hotkeyManager = m_hotkeyManager;
	m_apiServices->performanceHistoryTracker = m_performanceHistoryTracker;
	m_apiServices->analyticsEventsManager = m_analyticsEventsManager;
	m_apiServices->externalSceneDataProviderManager =
		m_externalSceneDataProviderManager;
	m_apiServices->httpClient = m_httpClient;
	m_apiServices->localFilesystemHttpServer = m_localFilesystemHttpServer;
	m_apiServices->nativeObsControlsManager = m_nativeObsControlsManager;
	m_apiServices->profilesManager = m_profilesManager;
	m_apiServices->backupManager = m_backupManager;
#ifdef SE_ENABLE_WYVRN
	m_apiServices->razerWyvrnManager = m_razerWyvrnManager;
#endif
	m_apiServices->cleanupManager = m_cleanupManager;
	m_apiServices->previewManager = m_previewManager;
	m_apiServices->websocketApiServer = m_websocketApiServer;
	m_apiServices->hostEventDispatcher = m_hostEventDispatcher;
	m_apiServices->messageBus = m_messageBus;
	m_apiServices->sharedVideoCompositionManager =
		m_sharedVideoCompositionManager;
	m_apiServices->videoCompositionManager = m_videoCompositionManager;
	m_apiServices->audioCompositionManager = m_audioCompositionManager;
	m_apiServices->outputManager = m_outputManager;
	m_apiServices->obsActiveSceneTracker = m_obsActiveSceneTracker;
	m_apiServices->statePersistence = m_statePersistence;
	m_apiServices->statusBarMessageManager = m_statusBarMessageManager;
	m_apiServices->popupWindowManager = m_popupWindowManager;
	m_apiServices->browserDialogManager = m_browserDialogManager;
	m_apiServices->sessionManager = m_sessionManager;
	m_apiServices->auxiliaryControlsBuilder = m_auxiliaryControlsBuilder;
}

void SEGlobalStateManager::Initialize(QMainWindow *obs_main_window)
{
	m_mainWindow = obs_main_window;

	// Initialize on the main thread.
	//
	// Which backend this is -- BugSplat, Sentry, or none at all -- is decided
	// at build time by the STREAMELEMENTS_CRASH_HANDLER CMake variable. Create()
	// returns nullptr when crash reporting is compiled out.
	m_crashHandler = SECrashHandler::Create();

	/*
	struct local_context {
		SEGlobalStateManager *self;
		QMainWindow *obs_main_window;
	};
	*/

	// http://doc.qt.io/qt-5/qmainwindow.html#DockOption-enum
	obs_main_window->setDockOptions(
		QMainWindow::AnimatedDocks |
		QMainWindow::AllowNestedDocks |
		QMainWindow::AllowTabbedDocks);

#ifdef _WIN32
	std::string storagePath = std::string("../obs-browser/") + GetCefVersionString();
#else
	std::string storagePath = GetCefVersionString();
#endif
	std::string fullStoragePath;
	{
		auto rpath = obs_module_config_path(
			storagePath.c_str());

#ifdef _WIN32
		auto path = os_get_abs_path_ptr(rpath);

		fullStoragePath = path;

		bfree(path);
#else
		fullStoragePath = rpath;
#endif

		bfree(rpath);
	}
	int os_mkdirs_ret = os_mkdirs(fullStoragePath.c_str());

	m_cef = obs_browser_init_panel();
	if (!m_cef) {
		blog(LOG_ERROR,
			"obs-streamelements-core: obs_browser_init_panel() failed");
		return;
	}
	if (!m_cef->initialized()) {
		m_cef->init_browser();
		m_cef->wait_for_browser_init();
	}

	m_cefCookieManager = m_cef->create_cookie_manager(storagePath, true);

	//
	// Created before any service, and filled in by PublishApiServices()
	// once they all exist: the widget manager and the worker manager are
	// constructed here and hand the bundle on to every API handler they
	// create, which happens later, when a page loads.
	//
	m_apiServices = std::make_shared<SEApiServices>();

	//
	// Needs nothing but the bundle, and three things below take it: the
	// menu manager, and the two scene-list monitors the scene manager
	// builds. It keeps the bundle weakly and locks it per call, so being
	// built before any of its contents exist is fine.
	//
	m_auxiliaryControlsBuilder =
		std::make_shared<SEAuxiliaryControlsBuilder>(m_apiServices);

	//
	// Before the menu manager, which takes it: two of its menu items
	// are the non-API way into a session reset. Nothing else has to
	// exist yet -- the session manager locks the bundle per call, and
	// by the time a menu item can be clicked it is filled in.
	//
	m_sessionManager = std::make_shared<SESessionManager>(
		mainWindow(), m_cefCookieManager, m_apiServices);

	m_httpClient =
		std::make_shared<SEHttpClient>();
	m_localFilesystemHttpServer = std::make_shared<
		SELocalFilesystemHttpServer>();
	//
	// Before the menu manager, which takes it: the same rule the cleanup
	// manager and the backup manager follow two lines down -- a
	// dependency exists before the thing that holds it.
	//
	m_performanceHistoryTracker = std::make_shared<
		SEPerformanceHistoryTracker>();
	m_analyticsEventsManager = std::make_shared<
		SEAnalyticsEventsManager>();
	m_widgetManager = std::make_shared<SEBrowserWidgetManager>(
		obs_main_window, m_apiServices);

	//
	// Constructed ahead of everything that takes it. Its constructor binds
	// port 27952 and starts accepting, so this opens the socket a few
	// microseconds earlier in startup than it used to -- the managers
	// between here and its old position are synchronous and immediate, and
	// the panels that connect to it take seconds to come up.
	//
	m_websocketApiServer = std::make_shared<SEWebsocketApiServer>();

	//
	// Right after the server it forwards to, and before everything that
	// announces a host event, so each of those can be handed it.
	//
	m_hostEventDispatcher =
		std::make_shared<SEHostEventDispatcher>(m_websocketApiServer);

	//
	// The instance GetInstance() owns, not a second one: its remaining
	// consumers would otherwise register their listeners on a different
	// bus from the one events are published to.
	//
	m_messageBus = SEMessageBus::GetInstance();
	m_messageBus->SetHostEventDispatcher(m_hostEventDispatcher);

	//
	// The config publishes a system-state change when the startup flags
	// are written, which is the only thing it needs the bus for.
	//
	SEConfig::GetInstance()->SetMessageBus(m_messageBus);

	m_videoCompositionManager = std::make_shared<SEVideoCompositionManager>(
		m_websocketApiServer, m_hostEventDispatcher, m_messageBus);
	m_audioCompositionManager = std::make_shared<SEAudioCompositionManager>(
		m_websocketApiServer, m_hostEventDispatcher, m_messageBus);

	//
	// The composition manager is injected rather than fetched inside the
	// constructor, which is where CORE-735's shape lived: it is built
	// above, so the ordering is now the compiler's business.
	//
	m_obsSceneManager = std::make_shared<SEObsSceneManager>(
		obs_main_window, m_videoCompositionManager,
		m_auxiliaryControlsBuilder, m_hostEventDispatcher);
	m_menuManager = std::make_shared<SEMenuManager>(
		obs_main_window, m_sessionManager, m_performanceHistoryTracker,
		m_analyticsEventsManager, m_auxiliaryControlsBuilder);
	m_bwTestManager = std::make_shared<SEBandwidthTestManager>(
		m_hostEventDispatcher);
	m_outputSettingsManager = std::make_shared<
		SEOutputSettingsManager>();
	m_workerManager = std::make_shared<SEWorkerManager>(m_apiServices);
	m_hotkeyManager =
		std::make_shared<SEHotkeyManager>(m_hostEventDispatcher);
	m_externalSceneDataProviderManager = std::make_shared<
		SEExternalSceneDataProviderManager>(
		m_localFilesystemHttpServer);
	m_nativeObsControlsManager =
		SENativeOBSControlsManager::
			GetInstance();
	m_nativeObsControlsManager->SetApiServices(m_apiServices);
	m_profilesManager =
		std::make_shared<SEProfilesManager>();
	// Constructed before the backup manager, which now takes it: a
	// dependency has to exist before the thing that holds it.
	m_cleanupManager =
		std::make_shared<SECleanupManager>();
	m_backupManager = std::make_shared<SEBackupManager>(
		m_cleanupManager, m_localFilesystemHttpServer);
	m_previewManager =
		std::make_shared<SEPreviewManager>(
			mainWindow(), m_hostEventDispatcher);

	m_windowStateEventFilter =
		std::make_shared<WindowStateChangeEventFilter>(
			mainWindow(), m_hostEventDispatcher);

#ifdef SE_ENABLE_WYVRN
	// After the websocket API server, because reaching Ok or a failure
	// status dispatches hostRazerWyvrnStatusChanged through it.
	//
	// Start() returns immediately. CoreInitSDK costs a flat ~3.3 s, so it
	// runs on the manager's own SDK thread and OBS start is not delayed;
	// WYVRN becomes available a few seconds into the session.
	m_razerWyvrnManager =
		std::make_shared<SERazerWyvrnManager>(
			m_hostEventDispatcher, m_localFilesystemHttpServer);
	m_razerWyvrnManager->Start();
#endif

	m_outputManager =
		std::make_shared<SEOutputManager>(
			m_videoCompositionManager, m_audioCompositionManager,
			m_hostEventDispatcher);

	m_sharedVideoCompositionManager =
		std::make_shared<SESharedVideoCompositionManager>(
			m_websocketApiServer, m_videoCompositionManager,
			m_messageBus);

	m_obsActiveSceneTracker = std::make_shared<SEObsActiveSceneTracker>(
		m_messageBus, m_websocketApiServer, m_videoCompositionManager);

	//
	// .get(), because the persistence object borrows these four rather
	// than owning them -- see the comment on its constructor.
	//
	m_statePersistence = std::make_shared<SEStatePersistence>(
		mainWindow(), m_widgetManager.get(), m_workerManager.get(),
		m_hotkeyManager.get(), m_nativeObsControlsManager.get(),
		m_hostEventDispatcher);

	//
	// Handed over after construction, and held weakly: the deferred saves
	// in SEWidgetManager used to reach it through the services bundle,
	// which is an edge from a widget-owning manager back to the bundle.
	//
	m_widgetManager->SetStatePersistence(m_statePersistence);

	m_statusBarMessageManager =
		std::make_shared<SEStatusBarMessageManager>(mainWindow());

	m_popupWindowManager =
		std::make_shared<SEPopupWindowManager>(m_apiServices);

	m_browserDialogManager = std::make_shared<SEBrowserDialogManager>(
		mainWindow(), m_apiServices);

	PublishApiServices();

	m_appStateListener = new ApplicationStateListener();
	m_themeChangeListener = new ThemeChangeListener(m_hostEventDispatcher,
							m_messageBus);

	// objectName, because QMainWindow::saveState() and restoreState()
	// identify docks by it and Qt's documentation requires it to be set for
	// every dock in the window. This one had none, so it was saved and
	// matched under an empty key (CORE-967).
	m_themeChangeListener->setObjectName("streamelements_theme_listener");

	// Bottom on both platforms now. The Windows branch passed
	// Qt::NoDockWidgetArea, which addDockWidget rejects -- the dock was
	// never added at all, which is one of the four "invalid 'area'
	// argument" warnings in every OBS log. It is invisible and floating
	// either way; see SEWidgetManager::AddDockWidget.
	mainWindow()->addDockWidget(Qt::BottomDockWidgetArea,
				    m_themeChangeListener);

	{
		// Set up "Live Support" button
		/*QPushButton* liveSupport = new QPushButton(
		QIcon(QPixmap(QString(":/images/icon.png"))),
		obs_module_text("StreamElements.Action.LiveSupport"));*/

		QPushButton *liveSupport =
			new QPushButton(obs_module_text(
				"StreamElements.Action.LiveSupport"));

		liveSupport->setStyleSheet(QString(
			"QPushButton { background-color: #d9dded; border: 1px solid #546ac8; color: #032ee1; padding: 2px; border-radius: 0px; } "
			"QPushButton:hover { background-color: #99aaec; border: 1px solid #546ac8; color: #ffffff; } "
			"QPushButton:pressed { background-color: #808dc0; border: 1px solid #546ac8; color: #ffffff; } "));

		QDockWidget *controlsDock =
			(QDockWidget *)obs_main_window
				->findChild<QDockWidget *>(
					"controlsDock");
		QVBoxLayout *buttonsVLayout =
			(QVBoxLayout *)controlsDock
				->findChild<QVBoxLayout *>(
					"buttonsVLayout");
		buttonsVLayout->addWidget(liveSupport);

		QObject::connect(
			liveSupport, &QPushButton::clicked,
			[analytics = m_analyticsEventsManager]() {
				analytics->trackEvent(
					"live_support_click",
					json11::Json::object{
						{"type", "button_click"},
						{"placement",
						 "live_support_button"}});

				QUrl navigate_url = QUrl(
					obs_module_text(
						"StreamElements.Action.LiveSupport.URL"),
					QUrl::TolerantMode);
				QDesktopServices::openUrl(
					navigate_url);
			});
	}

	{
		// Set up status bar
		QWidget *container = new QWidget();
		QHBoxLayout *layout = new QHBoxLayout();

		layout->setContentsMargins(5, 0, 5, 0);

		container->setLayout(layout);

		char version_buf[512];
		sprintf(version_buf,
#ifdef __APPLE__
        "SE.Live %s",
#else
			"SE.Live %s",
#endif
			GetSEPluginVersionString()
				.c_str());

		container->layout()->addWidget(
			new QLabel(version_buf, container));

		obs_main_window->statusBar()
			->addPermanentWidget(container);
	}

	std::string onBoardingReason = "";

	bool isOnBoarding = false;

	if (MKDIR_ERROR == os_mkdirs_ret) {
		blog(LOG_WARNING,
			"obs-streamelements-core: init: set on-boarding mode due to error creating new cookie storage path: %s",
			fullStoragePath.c_str());

		isOnBoarding = true;
		onBoardingReason =
			"Failed creating new cookie storage folder";
	} else if (MKDIR_SUCCESS == os_mkdirs_ret) {
		blog(LOG_INFO,
			"obs-streamelements-core: init: set on-boarding mode due to new cookie storage path: %s",
			fullStoragePath.c_str());

		isOnBoarding = true;
		onBoardingReason = "New cookie storage folder";
	} else if (SEConfig::GetInstance()
				->GetSEPluginVersion() !=
			STREAMELEMENTS_PLUGIN_VERSION) {
		blog(LOG_INFO,
			"obs-streamelements-core: init: set on-boarding mode due to configuration version mismatch");

		isOnBoarding = true;
		onBoardingReason =
			"State saved by other version of the plug-in";
	} else if (SEConfig::GetInstance()
				->GetStartupFlags() &
			SEConfig::
				STARTUP_FLAGS_ONBOARDING_MODE) {
		blog(LOG_INFO,
			"obs-streamelements-core: init: set on-boarding mode due to start-up flags");

		isOnBoarding = true;
		onBoardingReason =
			"Start-up flags indicate on-boarding mode";
	}

	if (isOnBoarding) {
		// On-boarding

		// Reset state but don't erase all cookies
		m_sessionManager->Reset(false);
	} else {
		// Regular

		m_statePersistence->RestoreState();
	}

	// Draining the posted-event queue runs arbitrary code while this
	// Initialize() is still on the stack and m_initialized is still false:
	// deferred deletes, frontend callbacks, and any modal dialog's own event
	// loop. Nothing constructed above may be assumed live below this point.
	SEDrainEventQueue();

	// Guarded because of the pump above, not out of caution. Observed: OBS
	// held OBSInit open in its modal update dialog for ~5 minutes, and by the
	// time this line ran m_menuManager was null. The fault then landed inside
	// UpdateInternal's own `if (!m_menu)` guard -- SYNC_ACCESS() locks a
	// static mutex, so it touches no member, which makes that guard the first
	// read through `this` and therefore the crash site rather than the
	// protection it looks like.
	if (m_menuManager)
		m_menuManager->Update();

	{
		json11::Json::object eventProps;
		json11::Json::array fields =
			json11::Json::array{};
		fields.push_back(json11::Json::array{
			"is_onboarding",
			isOnBoarding ? "true" : "false"});
		if (isOnBoarding) {
			fields.push_back(json11::Json::array{
				"onboarding_reason",
				onBoardingReason});
		}

		GetAnalyticsEventsManager()
			->trackEvent("se_live_initialized", eventProps, fields);
	}

	// Update visible state.
	//
	// Through m_menuManager, not GetInstance()->GetMenuManager(): this is a
	// member function and the member is right here. Going through the singleton
	// is what crashed. GetInstance() lazily constructs when s_instance is not
	// this instance, and a freshly constructed manager has every shared_ptr
	// member null -- so ->Update() ran on a null SEMenuManager.
	//
	// Confirmed from two minidumps of the same fault: UpdateInternal entered
	// with this == nullptr, while this->m_menuManager on the instance running
	// Initialize() was non-null. Two different objects; the singleton returned
	// the empty one.
	if (m_menuManager)
		m_menuManager->Update();

	streamelements_updater_init();

	m_statePersistence->SetEnabled(true);
	m_initialized = true;

	// From here on an OBS close is an ordinary close, and teardown runs
	// normally (CORE-786).
	SENoteInitializeCompleted();

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);
}

void SEGlobalStateManager::Shutdown()
{
	PREVENT_RECURSIVE_REENTRY();

	if (!m_initialized) {
		return;
	}

	// Before anything below blocks. From here the Qt main thread is ours and
	// will not return to its event loop, so a worker waiting on QtExecSync
	// must be released rather than left to deadlock us (CORE-1131).
	SetShuttingDown();

	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	m_statePersistence->PersistState(false);

	m_statePersistence->SetEnabled(false);

	streamelements_updater_shutdown();

	// Early, and explicitly rather than by releasing the pointer: this
	// joins the SDK thread, which unwinds through CoreUnInit() on its way
	// out. Two things must still be alive while that happens -- the
	// websocket API server, so the ShuttingDown status reaches the page,
	// and the crash handler below, so a fault inside Razer's DLL is still
	// reported.
#ifdef SE_ENABLE_WYVRN
	if (m_razerWyvrnManager) {
		m_razerWyvrnManager->Shutdown();
	}
#endif

	// Shutdown on the main thread. No platform guard: StopAsyncHangDetection()
	// is a no-op on backends that have no hang detection.
	if (m_crashHandler) {
		m_crashHandler->StopAsyncHangDetection();

		//delete m_crashHandler; // TODO: Shutting down the crash handler might be contributing to us missing some exceptions during shutdown
		//m_crashHandler = nullptr;
	}
	
	//mainWindow()->removeDockWidget(m_themeChangeListener);
	if (m_themeChangeListener) {
		m_themeChangeListener->deleteLater();
		m_themeChangeListener = nullptr;
	}
	if (m_appStateListener) {
		m_appStateListener->deleteLater();
		m_appStateListener = nullptr;
	}

	m_obsActiveSceneTracker = nullptr;
	m_analyticsEventsManager = nullptr;
	m_performanceHistoryTracker = nullptr;
	m_outputSettingsManager = nullptr;
	m_bwTestManager = nullptr;
	m_widgetManager = nullptr;
	m_workerManager = nullptr;

	m_obsSceneManager = nullptr;

	m_sharedVideoCompositionManager = nullptr;
	m_outputManager = nullptr;
	m_videoCompositionManager = nullptr;
	m_audioCompositionManager = nullptr;

	m_menuManager = nullptr;
	m_auxiliaryControlsBuilder = nullptr;
	m_hotkeyManager = nullptr;
	m_externalSceneDataProviderManager = nullptr;
	m_httpClient = nullptr;
	m_localFilesystemHttpServer = nullptr;

	m_nativeObsControlsManager = nullptr; // Singleton
	SENativeOBSControlsManager::Destroy();

	m_profilesManager = nullptr;
	m_backupManager = nullptr;
#ifdef SE_ENABLE_WYVRN
	m_razerWyvrnManager = nullptr;
#endif
	m_cleanupManager = nullptr;
	m_previewManager = nullptr;
	m_messageBus = nullptr;
	m_hostEventDispatcher = nullptr;
	m_websocketApiServer = nullptr;
	m_windowStateEventFilter = nullptr;
	m_statePersistence = nullptr;
	m_statusBarMessageManager = nullptr;
	m_popupWindowManager = nullptr;
	m_browserDialogManager = nullptr;

	//
	// The session manager borrows this pointer and can outlive
	// Shutdown() -- the container holds it until this object is
	// destroyed -- so take the pointer away before deleting what it
	// points at.
	//
	if (m_sessionManager) {
		m_sessionManager->ForgetCookieManager();
	}

	m_sessionManager = nullptr;

	if (m_cefCookieManager) {
		delete m_cefCookieManager;
		m_cefCookieManager = nullptr;
	}

	if (m_cef) {
		delete m_cef;
		m_cef = nullptr;
	}

	SEMessageBus::Destroy();

	SEConfig::Destroy();

	SEDrainEventQueue();

	m_initialized = false;
}

void SEGlobalStateManager::OnObsExit()
{
	if (!m_statePersistence)
		return;

	m_statePersistence->PersistState(false);

	m_statePersistence->SetEnabled(false);
}

#ifndef WIN32
void SEGlobalStateManager::UninstallPlugin()
{
	QMessageBox::information(mainWindow(), "Unsupported",
				 "This function is not currently supported");
}
#else
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
void SEGlobalStateManager::UninstallPlugin()
{
	bool opt_out = false;
	bool exec_success = false;

	DWORD buflen = 32768;
	wchar_t *buffer = new wchar_t[buflen];

	LSTATUS lResult = RegGetValueW(
		HKEY_LOCAL_MACHINE,
		L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\StreamElements OBS.Live",
		L"UninstallString", RRF_RT_REG_SZ, NULL, buffer, &buflen);

	if (lResult != ERROR_SUCCESS) {
		lResult = RegGetValueW(
			HKEY_LOCAL_MACHINE,
			L"Software\\WOW6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\StreamElements OBS.Live",
			L"UninstallString", RRF_RT_REG_SZ, NULL, buffer,
			&buflen);
	}

	if (lResult == ERROR_SUCCESS) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			mainWindow(),
			obs_module_text(
				"StreamElements.Action.Uninstall.Confirmation.Title"),
			obs_module_text(
				"StreamElements.Action.Uninstall.Confirmation.Text"),
			QMessageBox::Yes | QMessageBox::No);

		if (reply == QMessageBox::Yes) {
			STARTUPINFOW startInf;
			memset(&startInf, 0, sizeof startInf);
			startInf.cb = sizeof(startInf);

			PROCESS_INFORMATION procInf;
			memset(&procInf, 0, sizeof procInf);

			wchar_t *args = PathGetArgsW(buffer);
			PathRemoveArgsW(buffer);

			HINSTANCE hInst = ShellExecuteW(NULL, L"runas", buffer,
							args, NULL, SW_SHOW);

			BOOL bResult = hInst > (HINSTANCE)32;

			if (bResult) {
				exec_success = true;
			} else {
				QMessageBox::information(
					mainWindow(),
					obs_module_text(
						"StreamElements.Action.Uninstall.Error.Title"),
					GetLastErrorMsg());
			}
		} else {
			opt_out = true;
		}
	} else {
		QMessageBox::information(
			mainWindow(),
			obs_module_text(
				"StreamElements.Action.Uninstall.Error.Title"),
			obs_module_text(
				"StreamElements.Action.Uninstall.Error.Text.CommandNotFound"));
	}

	delete[] buffer;

	if (!opt_out) {
		if (exec_success) {
			// Close main window = quit OBS
			QMetaObject::invokeMethod(mainWindow(), "close",
						  Qt::QueuedConnection);
		} else {
			QUrl navigate_url = QUrl(
				obs_module_text(
					"StreamElements.Action.Uninstall.URL"),
				QUrl::TolerantMode);
			QDesktopServices::openUrl(navigate_url);
		}
	}
}
#endif
