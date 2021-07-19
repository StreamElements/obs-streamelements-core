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

/* ========================================================================= */

void register_cookie_manager(CefRefPtr<CefCookieManager> cm);
void unregister_cookie_manager(CefRefPtr<CefCookieManager> cm);
void flush_cookie_manager(CefRefPtr<CefCookieManager> cm);
void flush_cookie_managers();

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

static std::string GetCEFStoragePath()
{
	std::string version = GetCefVersionString();

	BPtr<char> rpath = obs_module_config_path(version.c_str());

#ifdef WIN32
	BPtr<char> path = os_get_abs_path_ptr(rpath.Get());

	return path.Get();
#else
	return rpath.Get();
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
	blog(LOG_INFO, "SetMainWindowStyle");

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

			StreamElementsCefClient::DispatchJSEvent(
				"hostUIThemeChanged", json.dump());

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
		StreamElementsCefClient::DispatchJSEvent(name, args);

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

class BrowserTask : public CefTask {
public:
	std::function<void()> task;

	inline BrowserTask(std::function<void()> task_) : task(task_) {}
	virtual void Execute() override { task(); }

	IMPLEMENT_REFCOUNTING(BrowserTask);
};

static bool QueueCEFTask(std::function<void()> task)
{
	return CefPostTask(TID_UI,
			   CefRefPtr<BrowserTask>(new BrowserTask(task)));
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

			m_appStateListener =
				new ApplicationStateListener();
			m_themeChangeListener =
				new ThemeChangeListener();
#ifdef WIN32
			mainWindow()->addDockWidget(
				Qt::NoDockWidgetArea,
				m_themeChangeListener);
#else
			mainWindow()->addDockWidget(
				Qt::BottomDockWidgetArea,
				m_themeChangeListener);
#endif

			std::string storagePath = GetCEFStoragePath();
			int os_mkdirs_ret = os_mkdirs(storagePath.c_str());

			char *webRootPath = obs_module_file("localwebroot");
			m_localWebFilesServer =
				new StreamElementsLocalWebFilesServer(
					webRootPath ? webRootPath : "");
			bfree(webRootPath);
			m_cookieManager =
				new StreamElementsCookieManager(storagePath);
			m_httpClient =
				new StreamElementsHttpClient();
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
			m_windowStateEventFilter =
				new WindowStateChangeEventFilter(
					mainWindow());

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
								"Live Support Clicked");

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
                    "OBS.Live for Mac version %s powered by StreamElements",
#else
					"OBS.Live version %s powered by StreamElements",
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
				     "obs-browser: init: set on-boarding mode due to error creating new cookie storage path: %s",
				     storagePath.c_str());

				isOnBoarding = true;
				onBoardingReason =
					"Failed creating new cookie storage folder";
			} else if (MKDIR_SUCCESS == os_mkdirs_ret) {
				blog(LOG_INFO,
				     "obs-browser: init: set on-boarding mode due to new cookie storage path: %s",
				     storagePath.c_str());

				isOnBoarding = true;
				onBoardingReason = "New cookie storage folder";
			} else if (StreamElementsConfig::GetInstance()
					   ->GetStreamElementsPluginVersion() !=
				   STREAMELEMENTS_PLUGIN_VERSION) {
				blog(LOG_INFO,
				     "obs-browser: init: set on-boarding mode due to configuration version mismatch");

				isOnBoarding = true;
				onBoardingReason =
					"State saved by other version of the plug-in";
			} else if (StreamElementsConfig::GetInstance()
					   ->GetStartupFlags() &
				   StreamElementsConfig::
					   STARTUP_FLAGS_ONBOARDING_MODE) {
				blog(LOG_INFO,
				     "obs-browser: init: set on-boarding mode due to start-up flags");

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
				json11::Json::object eventProps = {
					{"isOnBoarding", isOnBoarding},
				};
				if (isOnBoarding) {
					eventProps["onBoardingReason"] =
						onBoardingReason.c_str();
				}

				GetAnalyticsEventsManager()
					->trackEvent("Initialized", eventProps);
			}
		});

	QtPostTask([]() {
		// Update visible state
		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->Update();
	});

	register_cookie_manager(CefCookieManager::GetGlobalManager(nullptr));

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

	unregister_cookie_manager(CefCookieManager::GetGlobalManager(nullptr));

	obs_frontend_remove_event_callback(handle_obs_frontend_event, nullptr);

	//flush_cookie_manager(GetCookieManager()->GetCefCookieManager());
	//flush_cookie_managers();

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
			delete m_localWebFilesServer;
			delete m_externalSceneDataProviderManager;
			delete m_httpClient;
			// delete m_nativeObsControlsManager; // Singleton
			delete m_profilesManager;
			delete m_backupManager;
			delete m_cleanupManager;
			delete m_previewManager;
			delete m_windowStateEventFilter;
			delete m_cookieManager;
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
	class CookieVisitor : public CefCookieVisitor {
	public:
		CookieVisitor() {}
		~CookieVisitor() {}

		virtual bool Visit(const CefCookie &cookie, int count,
				   int total, bool &deleteCookie) override
		{
			UNUSED_PARAMETER(count);
			UNUSED_PARAMETER(total);

			deleteCookie = true;

			CefString domain(&cookie.domain);
			CefString name(&cookie.name);

			// Process cookie whitelist. This is used for
			// preserving two-factor-authentication (2FA)
			// "remember this computer" state.
			//
			if (domain == ".twitch.tv" && name == "authy_id") {
				deleteCookie = false;
			}

			return true;
		}

	public:
		IMPLEMENT_REFCOUNTING(CookieVisitor);
	};

	if (!m_cookieManager->GetCefCookieManager()->VisitAllCookies(
		    new CookieVisitor())) {
		blog(LOG_ERROR,
		     "m_cookieManager->GetCefCookieManager()->VisitAllCookies() failed.");
	}
}

void StreamElementsGlobalStateManager::SerializeCookies(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	class CookieVisitor : public CefCookieVisitor {
	public:
		std::string domainFilter;
		std::string nameFilter;

		CefRefPtr<CefListValue> result;
		os_event_t *event;

	public:
		CookieVisitor(os_event_t *completionEvent)
			: result(CefListValue::Create()), event(completionEvent)
		{
		}

		~CookieVisitor() {
			os_event_signal(event);
		}

		virtual bool Visit(const CefCookie &cookie, int count,
				   int total, bool &deleteCookie) override
		{
			UNUSED_PARAMETER(count);
			UNUSED_PARAMETER(total);

			CefString value(&cookie.value);
			CefString domain(&cookie.domain);
			CefString name(&cookie.name);

			if (domainFilter.size() && domain != domainFilter)
				return true;

			if (nameFilter.size() && name != nameFilter)
				return true;

			CefRefPtr<CefDictionaryValue> d =
					CefDictionaryValue::Create();

			d->SetString("name", name);
			d->SetString("value", value);
			d->SetString("domain", domain);
			d->SetString("path", CefString(&cookie.path));
			d->SetBool("httponly", cookie.httponly);
			d->SetBool("secure", cookie.secure);

			result->SetDictionary(result->GetSize(), d);

			return true;
		}

	public:
		IMPLEMENT_REFCOUNTING(CookieVisitor);
	};

	os_event_t *event;
	os_event_init(&event, OS_EVENT_TYPE_AUTO);

	bool success = false;
	CefRefPtr<CefListValue> list = CefListValue::Create();

	CefRefPtr<CookieVisitor> visitor = new CookieVisitor(event);

	visitor->result = list;

	if (input && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d =
			input->GetDictionary();

		if (d->HasKey("domain") &&
			d->GetType("domain") == VTYPE_STRING) {
			visitor->domainFilter = d->GetString("domain");
		}

		if (d->HasKey("name") &&
		    d->GetType("name") == VTYPE_STRING) {
			visitor->nameFilter = d->GetString("name");
		}
	}

	success =
		m_cookieManager->GetCefCookieManager()->VisitAllCookies(
			visitor);

	// VisitAllCookies() is executed on an async thread.
	//
	// Visitor destructor will be called onse VisitAllCookies() is done,
	// and will indicate completion by raising event.
	//
	visitor = nullptr;

	if (!success) {
		blog(LOG_ERROR,
		     "m_cookieManager->GetCefCookieManager()->VisitAllCookies() failed.");
	} else {
		os_event_wait(event);

		output->SetList(list);
	}

	os_event_destroy(event);
}

extern void DispatchJSEvent(std::string eventName, std::string jsonString, BrowserSource* browser);

static void DispatchJSEventAllBrowsers(const char *eventName,
				       const char *jsonString)
{
	StreamElementsCefClient::DispatchJSEvent(eventName, jsonString);
	DispatchJSEvent(eventName, jsonString, nullptr);
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
			blog(LOG_INFO,
			     "obs-browser: state: restoring geometry: %s",
			     geometry->GetString().ToString().c_str());

			if (mainWindow()->restoreGeometry(
				    QByteArray::fromStdString(base64_decode(
					    geometry->GetString().ToString())))) {
				// https://bugreports.qt.io/browse/QTBUG-46620
				if (mainWindow()->isMaximized()) {
					mainWindow()->setGeometry(
						QApplication::desktop()
							->availableGeometry(
								mainWindow()));
				}

				result = true;
			} else {
				blog(LOG_ERROR,
				     "obs-browser: state: failed restoring geometry: %s",
				     geometry->GetString().ToString().c_str());
			}
		}
	}

	if (d->HasKey("windowState") &&
	    d->GetType("windowState") == VTYPE_STRING) {
		auto windowState = d->GetValue("windowState");

		if (windowState.get() &&
		    windowState->GetType() == VTYPE_STRING) {
			blog(LOG_INFO,
			     "obs-browser: state: restoring windowState: %s",
			     windowState->GetString().ToString().c_str());

			if (mainWindow()->restoreState(QByteArray::fromStdString(
				    base64_decode(windowState->GetString()
							  .ToString())))) {
				result = true;
			} else {
				blog(LOG_ERROR,
				     "obs-browser: state: failed restoring windowState: %s",
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

	//flush_cookie_manager(GetCookieManager()->GetCefCookieManager());
	//flush_cookie_managers();

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

	blog(LOG_INFO, "obs-browser: state: restoring state from base64: %s",
	     base64EncodedJSON.c_str());

	CefString json = base64_decode(base64EncodedJSON);

	if (!json.size()) {
		return;
	}

	blog(LOG_INFO, "obs-browser: state: restoring state from json: %s",
	     json.ToString().c_str());

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
		blog(LOG_INFO, "obs-browser: state: restoring workers: %s",
		     CefWriteJSON(workersState, JSON_WRITER_DEFAULT)
			     .ToString()
			     .c_str());
		GetWorkerManager()->Deserialize(workersState);
	}

	if (dockingWidgetsState.get()) {
		blog(LOG_INFO,
		     "obs-browser: state: restoring docking widgets: %s",
		     CefWriteJSON(dockingWidgetsState, JSON_WRITER_DEFAULT)
			     .ToString()
			     .c_str());
		GetWidgetManager()->DeserializeDockingWidgets(
			dockingWidgetsState);
	}

	if (notificationBarState.get()) {
		blog(LOG_INFO,
		     "obs-browser: state: restoring notification bar: %s",
		     CefWriteJSON(notificationBarState, JSON_WRITER_DEFAULT)
			     .ToString()
			     .c_str());
		GetWidgetManager()->DeserializeNotificationBar(
			notificationBarState);
	}

	if (hotkeysState.get()) {
		blog(LOG_INFO,
		     "obs-browser: state: restoring hotkey bindings: %s",
		     CefWriteJSON(hotkeysState, JSON_WRITER_DEFAULT)
			     .ToString()
			     .c_str());
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

#include <include/cef_parser.h> // CefParseJSON, CefWriteJSON

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
						   isIncognito);

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

		QueueCEFTask([this, url, executeJavaScriptOnLoad,
			      enableHostApi]() {
			CefWindowInfo windowInfo;
#ifdef WIN32
			windowInfo.SetAsPopup(0, ""); // Initial title
#else
			// TODO: TBD: Check if special handling is required for MacOS
#endif

			CefBrowserSettings cefBrowserSettings;

			cefBrowserSettings.Reset();
			cefBrowserSettings.javascript_close_windows =
				STATE_ENABLED;
			cefBrowserSettings.local_storage = STATE_ENABLED;

			StreamElementsApiMessageHandler *apiMessageHandler =
				enableHostApi
					? new StreamElementsApiMessageHandler()
					: nullptr;

			CefRefPtr<StreamElementsCefClient> cefClient =
				new StreamElementsCefClient(
					executeJavaScriptOnLoad,
					apiMessageHandler,
					nullptr,
					StreamElementsMessageBus::DEST_UI);

			CefRefPtr<CefBrowser> browser =
				CefBrowserHost::CreateBrowserSync(
					windowInfo, cefClient, url.c_str(),
					cefBrowserSettings,
#if CHROME_VERSION_BUILD >= 3770
#if ENABLE_CREATE_BROWSER_API
				apiMessageHandler
					? apiMessageHandler
						  ->CreateBrowserArgsDictionary()
					: CefRefPtr<CefDictionaryValue>(),
#else
				CefRefPtr<CefDictionaryValue>(),
#endif
#endif
				GetCookieManager()
						->GetCefRequestContext());
		});
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
