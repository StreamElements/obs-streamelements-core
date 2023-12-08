#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "Version.hpp"
#include "StreamElementsBrowserDialog.hpp"
#include "StreamElementsReportIssueDialog.hpp"

#include "base64/base64.hpp"
#include "json11/json11.hpp"
#include "cef-headers.hpp"

#include <util/threading.h>
#include <util/util.hpp>

#include <QPushButton>
#include <QMessageBox>
#include <QGuiApplication>

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

StreamElementsGlobalStateManager::ApplicationStateListener::
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

StreamElementsGlobalStateManager::ApplicationStateListener::
	~ApplicationStateListener()
{
	uint32_t obsMajorVersion = obs_get_version() >> 24;

	if (obsMajorVersion < 22) {
		m_timer.stop();
	}
}

void StreamElementsGlobalStateManager::ApplicationStateListener::
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

StreamElementsGlobalStateManager::ThemeChangeListener::ThemeChangeListener()
	: QDockWidget()
{
	setVisible(false);
	setFloating(true);

	SetMainWindowStyle();
}

void StreamElementsGlobalStateManager::ThemeChangeListener::changeEvent(
	QEvent *event)
{
	if (event->type() == QEvent::StyleChange) {
		//SetMainWindowStyle();

		std::string newTheme = GetCurrentThemeName();

		if (newTheme != m_currentTheme) {
			json11::Json json = json11::Json::object{
				{"name", newTheme},
			};

			StreamElementsGlobalStateManager::GetInstance()
				->GetWebsocketApiServer()
				->DispatchJSEvent("system",
						  "hostUIThemeChanged",
						  json.dump());

			StreamElementsMessageBus::GetInstance()
				->NotifyAllExternalEventListeners(
					StreamElementsMessageBus::
						DEST_ALL_EXTERNAL,
					StreamElementsMessageBus::
						SOURCE_APPLICATION,
					"OBS", "UIThemeChanged",
					CefParseJSON(
						json.dump(),
						JSON_PARSER_ALLOW_TRAILING_COMMAS));

			m_currentTheme = newTheme;
		}
	}
}

/* ========================================================================= */

static void handle_obs_frontend_event(enum obs_frontend_event event, void *data)
{
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
		obs_source_t *source = obs_frontend_get_current_scene();

		if (source) {
			const char *sourceName = obs_source_get_name(source);

			if (sourceName) {
				json11::Json json = json11::Json::object{
					{"name", sourceName},
					{"width",
					 (int)obs_source_get_width(source)},
					{"height",
					 (int)obs_source_get_height(source)}};

				name = "hostActiveSceneChanged";
				args = json.dump();
			}

			obs_source_release(source);
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
		name = "hostExit";
		break;
	/*case OBS_FRONTEND_EVENT_STUDIO_MODE_ENABLED:
		if (StreamElementsGlobalStateManager::GetInstance()
			    ->GetWidgetManager()
			    ->HasCentralBrowserWidget()) {
			// Due to the way we are now managing the central widget
			// (constrained by MacOS support), we must make sure that
			// the Studio Mode is disabled while the central widget
			// is visible, otherwise it will take up its space.
			obs_frontend_set_preview_program_mode(false);
		}
		return;
	*/
	default:
		return;
	}

	if (name.size()) {
		StreamElementsGlobalStateManager::GetInstance()
			->GetWebsocketApiServer()
			->DispatchJSEvent("system", name, args);

		std::string externalEventName =
			name.c_str() + 4; /* remove 'host' prefix */
		externalEventName[0] = tolower(
			externalEventName[0]); /* lower case first letter */

		StreamElementsMessageBus::GetInstance()
			->NotifyAllExternalEventListeners(
				StreamElementsMessageBus::DEST_ALL_EXTERNAL,
				StreamElementsMessageBus::SOURCE_APPLICATION,
				"OBS", externalEventName,
				CefParseJSON(args,
					     JSON_PARSER_ALLOW_TRAILING_COMMAS));
	}
}

/* ========================================================================= */

StreamElementsGlobalStateManager *StreamElementsGlobalStateManager::s_instance =
	nullptr;

StreamElementsGlobalStateManager::StreamElementsGlobalStateManager() {}

StreamElementsGlobalStateManager::~StreamElementsGlobalStateManager()
{
	Shutdown();
}

StreamElementsGlobalStateManager *
StreamElementsGlobalStateManager::GetInstance()
{
	if (!s_instance) {
		s_instance = new StreamElementsGlobalStateManager();
	}

	return s_instance;
}

#include <QWindow>
#include <QObjectList>

void StreamElementsGlobalStateManager::Initialize(QMainWindow *obs_main_window)
{
	m_mainWindow = obs_main_window;

	// Initialize on the main thread
	m_crashHandler = new StreamElementsCrashHandler();

	struct local_context {
		StreamElementsGlobalStateManager *self;
		QMainWindow *obs_main_window;
	};

	QtExecSync(
		[this, obs_main_window]() -> void {
			// http://doc.qt.io/qt-5/qmainwindow.html#DockOption-enum
			obs_main_window->setDockOptions(
				QMainWindow::AnimatedDocks |
				QMainWindow::AllowNestedDocks |
				QMainWindow::AllowTabbedDocks);

			std::string storagePath = std::string("../obs-browser/") + GetCefVersionString();
			std::string fullStoragePath;
			{
				auto rpath = obs_module_config_path(
					storagePath.c_str());

				auto path = os_get_abs_path_ptr(rpath);

				fullStoragePath = path;

				bfree(path);
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

			m_httpClient =
				new StreamElementsHttpClient();
			m_localFilesystemHttpServer =
				new StreamElementsLocalFilesystemHttpServer();
			m_analyticsEventsManager =
				new StreamElementsAnalyticsEventsManager();
			m_widgetManager =
				new StreamElementsBrowserWidgetManager(
					obs_main_window);
			m_obsSceneManager =
				new StreamElementsObsSceneManager(
					obs_main_window);
			m_menuManager =
				new StreamElementsMenuManager(
					obs_main_window);
			m_bwTestManager =
				new StreamElementsBandwidthTestManager();
			m_outputSettingsManager =
				new StreamElementsOutputSettingsManager();
			m_workerManager =
				new StreamElementsWorkerManager();
			m_hotkeyManager =
				new StreamElementsHotkeyManager();
			m_performanceHistoryTracker =
				new StreamElementsPerformanceHistoryTracker();
			m_externalSceneDataProviderManager =
				new StreamElementsExternalSceneDataProviderManager();
			m_nativeObsControlsManager =
				StreamElementsNativeOBSControlsManager::
					GetInstance();
			m_profilesManager =
				new StreamElementsProfilesManager();
			m_backupManager =
				new StreamElementsBackupManager();
			m_cleanupManager =
				new StreamElementsCleanupManager();
			m_previewManager =
				new StreamElementsPreviewManager(
					mainWindow());
			m_websocketApiServer =
				new StreamElementsWebsocketApiServer();
			m_windowStateEventFilter =
				new WindowStateChangeEventFilter(
					mainWindow());

						m_appStateListener = new ApplicationStateListener();
			m_themeChangeListener = new ThemeChangeListener();
#ifdef WIN32
			mainWindow()->addDockWidget(Qt::NoDockWidgetArea,
						    m_themeChangeListener);
#else
			mainWindow()->addDockWidget(Qt::BottomDockWidgetArea,
						    m_themeChangeListener);
#endif

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
					[]() {
						StreamElementsGlobalStateManager::GetInstance()
							->GetAnalyticsEventsManager()
							->trackEvent(
								"live_support_click",
								json11::Json::object{
									{"type",
									 "button_click"},
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
                    "SE.Live for Mac version %s powered by StreamElements",
#else
					"SE.Live Core ver. %s powered by StreamElements",
#endif
					GetStreamElementsPluginVersionString()
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
			} else if (StreamElementsConfig::GetInstance()
					   ->GetStreamElementsPluginVersion() !=
				   STREAMELEMENTS_PLUGIN_VERSION) {
				blog(LOG_INFO,
				     "obs-streamelements-core: init: set on-boarding mode due to configuration version mismatch");

				isOnBoarding = true;
				onBoardingReason =
					"State saved by other version of the plug-in";
			} else if (StreamElementsConfig::GetInstance()
					   ->GetStartupFlags() &
				   StreamElementsConfig::
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
				Reset(false);
			} else {
				// Regular

				RestoreState();
			}

			QApplication::sendPostedEvents();

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
		});

	QtPostTask([]() {
		// Update visible state
		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->Update();
	});

	m_initialized = true;
	m_persistStateEnabled = true;

	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);
}

void StreamElementsGlobalStateManager::Shutdown()
{
	PREVENT_RECURSIVE_REENTRY();

	if (!m_initialized) {
		return;
	}

	obs_frontend_remove_event_callback(handle_obs_frontend_event, nullptr);

#ifdef WIN32
    // Shutdown on the main thread
    delete m_crashHandler;

    QtExecSync(
		[this]() -> void {
			//mainWindow()->removeDockWidget(m_themeChangeListener);
			m_themeChangeListener->deleteLater();
			m_appStateListener->deleteLater();

			delete m_analyticsEventsManager;
			delete m_performanceHistoryTracker;
			delete m_outputSettingsManager;
			delete m_bwTestManager;
			delete m_widgetManager;
			delete m_menuManager;
			delete m_hotkeyManager;
			delete m_obsSceneManager;
			delete m_externalSceneDataProviderManager;
			delete m_httpClient;
			delete m_localFilesystemHttpServer;
			// delete m_nativeObsControlsManager; // Singleton
			delete m_profilesManager;
			delete m_backupManager;
			delete m_cleanupManager;
			delete m_previewManager;
			delete m_websocketApiServer;
			delete m_windowStateEventFilter;
			delete m_cefCookieManager;
		});
#endif

	m_initialized = false;
}

void StreamElementsGlobalStateManager::StartOnBoardingUI(UiModifier uiModifier)
{
	std::string onBoardingURL =
		GetCommandLineOptionValue("streamelements-onboarding-url");

	if (!onBoardingURL.size()) {
		onBoardingURL =
			StreamElementsConfig::GetInstance()->GetUrlOnBoarding();
	}

	{
		// Manipulate on-boarding URL query string to reflect forceOnboarding state

		const char *QS_ONBOARDING = "onboarding";
		const char *QS_IMPORT = "import";

		QUrl url(onBoardingURL.c_str());
		QUrlQuery query(url);

		if (query.hasQueryItem(QS_ONBOARDING)) {
			query.removeQueryItem(QS_ONBOARDING);
		}

		if (query.hasQueryItem(QS_IMPORT)) {
			query.removeQueryItem(QS_IMPORT);
		}

		switch (uiModifier) {
		case OnBoarding:
			query.addQueryItem(QS_ONBOARDING, "1");
			break;
		case Import:
			query.addQueryItem(QS_IMPORT, "1");
			break;
		case Default:
		default:
			break;
		}

		url.setQuery(query.query());

		onBoardingURL = url.toString().toStdString();
	}

	StopOnBoardingUI();

	GetWidgetManager()->PushCentralBrowserWidget(onBoardingURL.c_str(),
						     nullptr);
	GetHotkeyManager()->RemoveAllManagedHotkeyBindings();

	// This also clears StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN
	StreamElementsConfig::GetInstance()->SetStartupFlags(
		StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);

	QtPostTask([this]() -> void {
		GetObsSceneManager()->Reset();

		GetMenuManager()->Update();

		PersistState();
	});
}

void StreamElementsGlobalStateManager::StopOnBoardingUI()
{
	GetWorkerManager()->RemoveAll();
	GetWidgetManager()->HideNotificationBar();
	GetWidgetManager()->RemoveAllDockWidgets();
	GetWidgetManager()->DestroyCurrentCentralBrowserWidget();

	GetNativeOBSControlsManager()->Reset();
}

void StreamElementsGlobalStateManager::SwitchToOBSStudio()
{
	typedef std::vector<std::string> ids_t;

	ids_t workers;
	ids_t widgets;

	GetWorkerManager()->GetIdentifiers(workers);
	GetWidgetManager()->GetDockBrowserWidgetIdentifiers(widgets);

	bool isEnabled = false;

	isEnabled |= (!!workers.size());
	isEnabled |= (!!widgets.size());
	isEnabled |= GetWidgetManager()->HasNotificationBar();
	isEnabled |= GetWidgetManager()->HasCentralBrowserWidget();

	if (isEnabled) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			mainWindow(),
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Confirmation.Title"),
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Confirmation.Text"),
			QMessageBox::Ok | QMessageBox::Cancel);

		if (reply == QMessageBox::Ok) {
			StreamElementsGlobalStateManager::GetInstance()
				->StopOnBoardingUI();

			///
			// Also resets StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN
			//
			// This is to keep our state consistent and not display "Logout"
			// when we are in fact already logged out.
			//
			StreamElementsConfig::GetInstance()->SetStartupFlags(
				StreamElementsConfig::
					STARTUP_FLAGS_ONBOARDING_MODE);

			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->Reset();

			GetMenuManager()->Update();
		}
	} else {
		QMessageBox::question(
			mainWindow(),
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Disabled.Title"),
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Disabled.Text"),
			QMessageBox::Ok);
	}
}

void StreamElementsGlobalStateManager::DeleteCookies()
{
	// TODO: Specify specific cookies to delete, or, work with OBS community to provide a delete filter
	if (m_cefCookieManager) {
		m_cefCookieManager->DeleteCookies("", "");
	}
}

static void DispatchJSEventAllBrowsers(const char *eventName,
				       const char *jsonString)
{
	StreamElementsGlobalStateManager::GetInstance()
		->GetWebsocketApiServer()
		->DispatchJSEvent("system", eventName, jsonString);
}

void StreamElementsGlobalStateManager::Reset(bool deleteAllCookies,
					     UiModifier uiModifier)
{
	DispatchJSEventAllBrowsers("hostStateReset", "null");

	// Allow short time for event to reach all renderers
	//
	// This is an unpleasant hack which allows widgets a
	// vague opportunity to clean up before they get
	// destroyed.
	os_sleep_ms(100);

	if (deleteAllCookies) {
		DeleteCookies();

		GetMenuManager()->Reset();
	}

	StartOnBoardingUI(uiModifier);
}

void StreamElementsGlobalStateManager::SerializeUserInterfaceState(
	CefRefPtr<CefValue> &output)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	QByteArray geometry = mainWindow()->saveGeometry();
	QByteArray windowState = mainWindow()->saveState();

	d->SetString("geometry",
		     base64_encode(geometry.begin(), geometry.size()));
	d->SetString("windowState",
		     base64_encode(windowState.begin(), windowState.size()));

	output->SetDictionary(d);
}

bool StreamElementsGlobalStateManager::DeserializeUserInterfaceState(
	CefRefPtr<CefValue> input)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	bool result = false;

	if (d->HasKey("geometry") && d->GetType("geometry") == VTYPE_STRING) {
		auto geometry = d->GetValue("geometry");

		if (geometry.get() && geometry->GetType() == VTYPE_STRING) {
			if (IsTraceLogLevel()) {
				blog(LOG_INFO,
				     "obs-streamelements-core: state: restoring geometry: %s",
				     geometry->GetString().ToString().c_str());
			}

			if (mainWindow()->restoreGeometry(
				    QByteArray::fromStdString(base64_decode(
					    geometry->GetString().ToString())))) {
				// https://bugreports.qt.io/browse/QTBUG-46620
				if (mainWindow()->isMaximized()) {
					mainWindow()->setGeometry(
#if QT_VERSION_MAJOR >= 6
						QApplication::primaryScreen()
#else
						QApplication::desktop()
#endif
							->availableGeometry(
#if QT_VERSION_MAJOR < 6
								mainWindow()
#endif
							)
					);
				}

				result = true;
			} else {
				blog(LOG_ERROR,
				     "obs-streamelements-core: state: failed restoring geometry: %s",
				     geometry->GetString().ToString().c_str());
			}
		}
	}

	if (d->HasKey("windowState") &&
	    d->GetType("windowState") == VTYPE_STRING) {
		auto windowState = d->GetValue("windowState");

		if (windowState.get() &&
		    windowState->GetType() == VTYPE_STRING) {
			if (IsTraceLogLevel()) {
				blog(LOG_INFO,
				     "obs-streamelements-core: state: restoring windowState: %s",
				     windowState->GetString()
					     .ToString()
					     .c_str());
			}

			if (mainWindow()->restoreState(QByteArray::fromStdString(
				    base64_decode(windowState->GetString()
							  .ToString())))) {
				result = true;
			} else {
				blog(LOG_ERROR,
				     "obs-streamelements-core: state: failed restoring windowState: %s",
				     windowState->GetString()
					     .ToString()
					     .c_str());
			}
		}
	}

	return result;
}

void StreamElementsGlobalStateManager::PersistState(bool sendEventToGuest)
{
	PREVENT_RECURSIVE_REENTRY();

	if (!m_persistStateEnabled) {
		return;
	}

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	CefRefPtr<CefValue> dockingWidgetsState = CefValue::Create();
	CefRefPtr<CefValue> notificationBarState = CefValue::Create();
	CefRefPtr<CefValue> workersState = CefValue::Create();
	CefRefPtr<CefValue> hotkeysState = CefValue::Create();
	CefRefPtr<CefValue> userInterfaceState = CefValue::Create();
	CefRefPtr<CefValue> outputPreviewTitleBarState = CefValue::Create();
	CefRefPtr<CefValue> outputPreviewFrameState = CefValue::Create();

	GetWidgetManager()->SerializeDockingWidgets(dockingWidgetsState);
	GetWidgetManager()->SerializeNotificationBar(notificationBarState);
	GetWorkerManager()->Serialize(workersState);
	GetHotkeyManager()->SerializeHotkeyBindings(hotkeysState, true);
	SerializeUserInterfaceState(userInterfaceState);
	GetNativeOBSControlsManager()->SerializePreviewTitleBar(
		outputPreviewTitleBarState);
	GetNativeOBSControlsManager()->SerializePreviewFrame(
		outputPreviewFrameState);

	rootDictionary->SetValue("dockingBrowserWidgets", dockingWidgetsState);
	rootDictionary->SetValue("notificationBar", notificationBarState);
	rootDictionary->SetValue("workers", workersState);
	rootDictionary->SetValue("hotkeyBindings", hotkeysState);
	rootDictionary->SetValue("userInterfaceState", userInterfaceState);
	rootDictionary->SetValue("outputPreviewTitleBarState",
				 outputPreviewTitleBarState);
	rootDictionary->SetValue("outputPreviewFrameState",
				 outputPreviewFrameState);

	root->SetDictionary(rootDictionary);

	CefString json = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	std::string base64EncodedJSON = base64_encode(json.ToString());

	StreamElementsConfig::GetInstance()->SetStartupState(base64EncodedJSON);

	if (sendEventToGuest) {
		AdviseHostUserInterfaceStateChanged();
	}
}

void StreamElementsGlobalStateManager::RestoreState()
{
	std::string base64EncodedJSON =
		StreamElementsConfig::GetInstance()->GetStartupState();

	if (!base64EncodedJSON.size()) {
		return;
	}

	if (IsTraceLogLevel()) {
		blog(LOG_INFO,
		     "obs-streamelements-core: state: restoring state from base64: %s",
		     base64EncodedJSON.c_str());
	}

	CefString json = base64_decode(base64EncodedJSON);

	if (!json.size()) {
		return;
	}

	if (IsTraceLogLevel()) {
		blog(LOG_INFO,
		     "obs-streamelements-core: state: restoring state from json: %s",
		     json.ToString().c_str());
	}

	CefRefPtr<CefValue> root =
		CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);
	CefRefPtr<CefDictionaryValue> rootDictionary = root->GetDictionary();

	if (!rootDictionary.get()) {
		return;
	}

	auto dockingWidgetsState =
		rootDictionary->GetValue("dockingBrowserWidgets");
	auto notificationBarState = rootDictionary->GetValue("notificationBar");
	auto workersState = rootDictionary->GetValue("workers");
	auto hotkeysState = rootDictionary->GetValue("hotkeyBindings");
	auto userInterfaceState =
		rootDictionary->GetValue("userInterfaceState");
	auto outputPreviewTitleBarState =
		rootDictionary->GetValue("outputPreviewTitleBarState");
	auto outputPreviewFrameState =
		rootDictionary->GetValue("outputPreviewFrameState");


	if (workersState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring workers: %s",
			     CefWriteJSON(workersState, JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		GetWorkerManager()->Deserialize(workersState);
	}

	if (dockingWidgetsState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring docking widgets: %s",
			     CefWriteJSON(dockingWidgetsState,
					  JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		GetWidgetManager()->DeserializeDockingWidgets(
			dockingWidgetsState);
	}

	if (notificationBarState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring notification bar: %s",
			     CefWriteJSON(notificationBarState,
					  JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		GetWidgetManager()->DeserializeNotificationBar(
			notificationBarState);
	}

	if (hotkeysState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring hotkey bindings: %s",
			     CefWriteJSON(hotkeysState, JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		GetHotkeyManager()->DeserializeHotkeyBindings(hotkeysState);
	}

	if (userInterfaceState.get()) {
		DeserializeUserInterfaceState(userInterfaceState);
	}

	if (outputPreviewTitleBarState.get()) {
		GetNativeOBSControlsManager()->DeserializePreviewTitleBar(
			outputPreviewTitleBarState);
	}

	if (outputPreviewFrameState.get()) {
		GetNativeOBSControlsManager()->DeserializePreviewFrame(
			outputPreviewFrameState);
	}
}

void StreamElementsGlobalStateManager::OnObsExit()
{
	PersistState(false);

	m_persistStateEnabled = false;
}

bool StreamElementsGlobalStateManager::DeserializeStatusBarTemporaryMessage(
	CefRefPtr<CefValue> input)
{
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("text")) {
		int timeoutMilliseconds = 0;
		std::string text = d->GetString("text");

		if (d->HasKey("timeoutMilliseconds")) {
			timeoutMilliseconds = d->GetInt("timeoutMilliseconds");
		}

		mainWindow()->statusBar()->showMessage(text.c_str(),
						       timeoutMilliseconds);

		return true;
	}

	return false;
}

bool StreamElementsGlobalStateManager::DeserializeModalDialog(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url")) {
		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;

		if (d->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad =
				d->GetString("executeJavaScriptOnLoad");
		}

		int width = 800;
		int height = 600;
		bool isIncognito = false;

		if (d->HasKey("width")) {
			width = d->GetInt("width");
		}

		if (d->HasKey("height")) {
			height = d->GetInt("height");
		}

		if (d->HasKey("incognitoMode") && d->GetBool("incognitoMode")) {
			isIncognito = true;
		}

		StreamElementsBrowserDialog dialog(mainWindow(), url,
						   executeJavaScriptOnLoad,
						   isIncognito, "modalDialog");

		if (d->HasKey("title")) {
			dialog.setWindowTitle(QString(
				d->GetString("title").ToString().c_str()));
		}

		dialog.setFixedSize(width, height);

		if (dialog.exec() == QDialog::Accepted) {
			output =
				CefParseJSON(dialog.result(),
					     JSON_PARSER_ALLOW_TRAILING_COMMAS);

			return true;
		}
	}

	return false;
}

bool StreamElementsGlobalStateManager::HasNonModalDialog(const char* id) {
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	return m_nonModalDialogs.count(id) > 0;
}

std::string StreamElementsGlobalStateManager::GetNonModalDialogUrl(const char *id)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_nonModalDialogs.count(id) > 0) {
		m_nonModalDialogs[id]->getUrl();
	}

	return "";
}

void StreamElementsGlobalStateManager::SerializeAllNonModalDialogs(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefDictionaryValue> result = CefDictionaryValue::Create();

	for (auto pair : m_nonModalDialogs) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", pair.first);
		d->SetString("url", pair.second->getUrl());
		d->SetString(
			"executeJavaScriptOnLoad",
			pair.second->getExecuteJavaScriptOnLoad());
		d->SetBool(
			"isIncognito",
			pair.second->getIsIncognito());
		d->SetString(
			"title",
			pair.second->windowTitle().toStdString());
		d->SetInt("width", pair.second->width());
		d->SetInt("height", pair.second->height());

		result->SetDictionary(pair.first, d);
	}

	output->SetDictionary(result);
}

bool StreamElementsGlobalStateManager::DeserializeCloseNonModalDialogsByIds(
	CefRefPtr<CefValue> input)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (input->GetType() != VTYPE_LIST)
		return false;

	auto list = input->GetList();

	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return false;
	}

	for (size_t i = 0; i < list->GetSize(); ++i) {
		std::string id = list->GetString(i);

		if (!m_nonModalDialogs.count(id))
			continue;

		auto dialog = m_nonModalDialogs[id];

		// Schedule dialog reject
		QMetaObject::invokeMethod(dialog, &QDialog::reject,
					  Qt::QueuedConnection);
	}

	return true;
}

bool StreamElementsGlobalStateManager::DeserializeFocusNonModalDialogById(
	CefRefPtr<CefValue> input)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (input->GetType() != VTYPE_STRING)
		return false;

	std::string id = input->GetString();

	if (!m_nonModalDialogs.count(id))
		return false;

	auto dialog = m_nonModalDialogs[id];

	dialog->activateWindow();

	return true;
}

bool StreamElementsGlobalStateManager::DeserializeNonModalDialogDimensionsById(
	CefRefPtr<CefValue> dialogId, CefRefPtr<CefValue> dimensions)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (dialogId->GetType() != VTYPE_STRING)
		return false;

	if (dimensions->GetType() != VTYPE_DICTIONARY)
		return false;

	auto d = dimensions->GetDictionary();

	if (!d->HasKey("width") || d->GetType("width") != VTYPE_INT)
		return false;

	if (!d->HasKey("height") || d->GetType("height") != VTYPE_INT)
		return false;

	std::string id = dialogId->GetString();

	if (!m_nonModalDialogs.count(id))
		return false;

	int width = d->GetInt("width");
	int height = d->GetInt("height");

	if (width < 1)
		return false;

	if (height < 1)
		return false;

	auto dialog = m_nonModalDialogs[id];

	int centerX = dialog->x() + (dialog->width() / 2);
	int centerY = dialog->y() + (dialog->height() / 2);

	dialog->setFixedWidth(width);
	dialog->setFixedHeight(height);
	dialog->move(centerX - (width / 2), centerY - (height / 2));

	return true;
}

std::shared_ptr<std::promise<CefRefPtr<CefValue>>> StreamElementsGlobalStateManager::DeserializeNonModalDialog(CefRefPtr<CefValue> input)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	auto promise = std::make_shared<std::promise<CefRefPtr<CefValue>>>();

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url")) {
		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;
		std::string id;

		if (d->HasKey("id") && d->GetType("id") == VTYPE_STRING) {
			id = d->GetString("id");
		}

		while (!id.size() || m_nonModalDialogs.count(id)) {
			id = CreateGloballyUniqueIdString();
		}

		if (d->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad =
				d->GetString("executeJavaScriptOnLoad");
		}

		int width = 800;
		int height = 600;
		bool isIncognito = false;

		if (d->HasKey("width")) {
			width = d->GetInt("width");
		}

		if (d->HasKey("height")) {
			height = d->GetInt("height");
		}

		if (d->HasKey("incognitoMode") && d->GetBool("incognitoMode")) {
			isIncognito = true;
		}

		auto dialog = new StreamElementsBrowserDialog(
			mainWindow(), url, executeJavaScriptOnLoad, isIncognito,
			"nonModalDialog");

		if (d->HasKey("title")) {
			dialog->setWindowTitle(QString(
				d->GetString("title").ToString().c_str()));
		}

		dialog->setFixedSize(width, height);

		m_nonModalDialogs[id] = dialog;

		dialog->connect(dialog, &QDialog::finished,
				[this, dialog, promise, id](int result) {
					std::lock_guard<decltype(m_mutex)> guard(
						m_mutex);

					m_nonModalDialogs.erase(id);

					CefRefPtr<CefValue> output =
						CefValue::Create();

					output->SetNull();

					if (result == QDialog::Accepted) {
						output = CefParseJSON(
							dialog->result(),
							JSON_PARSER_ALLOW_TRAILING_COMMAS);
					}

					dialog->deleteLater();

					promise->set_value(output);
				});

		dialog->setModal(false);
		dialog->show();
	} else {
		auto nullResult = CefValue::Create();
		nullResult->SetNull();

		promise->set_value(nullResult);
	}

	return promise;
}

bool StreamElementsGlobalStateManager::DeserializePopupWindow(
	CefRefPtr<CefValue> input)
{
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url")) {
		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;

		bool enableHostApi = d->HasKey("enableHostApi") &&
				     d->GetBool("enableHostApi");

		if (d->HasKey("executeJavaScriptOnLoad")) {
			executeJavaScriptOnLoad =
				d->GetString("executeJavaScriptOnLoad");
		}

		std::shared_ptr<StreamElementsApiMessageHandler>
			apiMessageHandler = nullptr;

		apiMessageHandler =
			std::make_shared<StreamElementsApiMessageHandler>("popupWindow");

		QMainWindow *window = new QMainWindow();

		auto browserWidget = new StreamElementsBrowserWidget(
			nullptr, StreamElementsMessageBus::DEST_UI, url.c_str(),
			executeJavaScriptOnLoad.c_str(),
			"reload", "popupWindow",
			CreateGloballyUniqueIdString().c_str(),
			apiMessageHandler, false);

		window->setCentralWidget(browserWidget);

		window->show();

		return true;
	}

	return false;
}

void StreamElementsGlobalStateManager::ReportIssue()
{
	obs_frontend_push_ui_translation(obs_module_get_string);

	StreamElementsReportIssueDialog dialog(mainWindow());

	obs_frontend_pop_ui_translation();

	if (dialog.exec() == QDialog::Accepted) {
	}
}

#ifndef WIN32
void StreamElementsGlobalStateManager::UninstallPlugin()
{
	QMessageBox::information(mainWindow(), "Unsupported",
				 "This function is not currently supported");
}
#else
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
void StreamElementsGlobalStateManager::UninstallPlugin()
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
