#include "SESessionManager.hpp"

#include "SEUtils.hpp"
#include "SEApiServices.hpp"
#include "SEConfig.hpp"

#include "SEBrowserWidgetManager.hpp"
#include "SEHotkeyManager.hpp"
#include "SEMenuManager.hpp"
#include "SENativeObsUIControlsManager.hpp"
#include "SEObsSceneManager.hpp"
#include "SEOutputManager.hpp"
#include "SESharedVideoCompositionManager.hpp"
#include "SEStatePersistence.hpp"
#include "SEVideoCompositionManager.hpp"
#include "SEWebsocketApiServer.hpp"
#include "SEWorkerManager.hpp"

#include "../../obs-browser/panel/browser-panel.hpp"

#include <QMainWindow>
#include <QMessageBox>
#include <QUrl>
#include <QUrlQuery>

#include <obs-module.h>

#include <util/platform.h>

#include <string>
#include <vector>

SESessionManager::SESessionManager(QMainWindow *mainWindow,
				   QCefCookieManager *cefCookieManager,
				   std::shared_ptr<SEApiServices> apiServices)
	: m_mainWindow(mainWindow),
	  m_cefCookieManager(cefCookieManager),
	  m_apiServices(apiServices)
{
}

SESessionManager::~SESessionManager() {}

void SESessionManager::DeleteCookies()
{
	// TODO: Specify specific cookies to delete, or, work with OBS community to provide a delete filter
	if (m_cefCookieManager) {
		m_cefCookieManager->DeleteCookies("", "");
	}
}

void SESessionManager::Reset(bool deleteAllCookies, UiModifier uiModifier)
{
	auto apiServices = m_apiServices.lock();

	if (!apiServices) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: SESessionManager: not resetting: the services bundle is gone");

		return;
	}

	if (apiServices->apiMessageSink) {
		apiServices->apiMessageSink->DispatchJSEvent(
			"system", "hostStateReset", "null");
	}

	// Allow short time for event to reach all renderers
	//
	// This is an unpleasant hack which allows widgets a
	// vague opportunity to clean up before they get
	// destroyed.
	os_sleep_ms(100);

	if (deleteAllCookies) {
		DeleteCookies();

		apiServices->menuManager->Reset();
	}

	StartOnBoardingUI(uiModifier);
}

void SESessionManager::StartOnBoardingUI(UiModifier uiModifier)
{
	auto apiServices = m_apiServices.lock();

	if (!apiServices) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: SESessionManager: not starting the on-boarding UI: the services bundle is gone");

		return;
	}

	std::string onBoardingURL =
		GetCommandLineOptionValue("streamelements-onboarding-url");

	if (!onBoardingURL.size()) {
		onBoardingURL = apiServices->config->GetUrlOnBoarding();
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

	apiServices->widgetManager->PushCentralBrowserWidget(
		onBoardingURL.c_str(), nullptr);
	apiServices->hotkeyManager->RemoveAllManagedHotkeyBindings();

	// This also clears SEConfig::STARTUP_FLAGS_SIGNED_IN
	apiServices->config->SetStartupFlags(
		SEConfig::STARTUP_FLAGS_ONBOARDING_MODE);

	//
	// The deferred half holds the bundle for the length of the task
	// instead of capturing this: the task is posted from a reset, and a
	// reset is one of the paths that can be followed by teardown.
	//
	QtPostTask([apiServices]() -> void {
		apiServices->obsSceneManager->Reset();
		apiServices->outputManager->Reset();
		apiServices->sharedVideoCompositionManager->Reset();
		apiServices->videoCompositionManager->Reset();

		apiServices->menuManager->Update();

		apiServices->statePersistence->PersistState();
	});
}

void SESessionManager::StopOnBoardingUI()
{
	auto apiServices = m_apiServices.lock();

	if (!apiServices) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: SESessionManager: not stopping the on-boarding UI: the services bundle is gone");

		return;
	}

	apiServices->workerManager->RemoveAll();
	apiServices->widgetManager->HideNotificationBar();
	apiServices->widgetManager->RemoveAllDockWidgets();
	apiServices->widgetManager->DestroyCurrentCentralBrowserWidget();

	apiServices->nativeObsUIControlsManager->Reset();
}

void SESessionManager::SwitchToOBSStudio()
{
	auto apiServices = m_apiServices.lock();

	if (!apiServices) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: SESessionManager: not switching to OBS Studio: the services bundle is gone");

		return;
	}

	typedef std::vector<std::string> ids_t;

	ids_t workers;
	ids_t widgets;

	apiServices->workerManager->GetIdentifiers(workers);
	apiServices->widgetManager->GetDockBrowserWidgetIdentifiers(widgets);

	bool isEnabled = false;

	isEnabled |= (!!workers.size());
	isEnabled |= (!!widgets.size());
	isEnabled |= apiServices->widgetManager->HasNotificationBar();
	isEnabled |= apiServices->widgetManager->HasCentralBrowserWidget();

	if (isEnabled) {
		QMessageBox::StandardButton reply;
		reply = QMessageBox::question(
			m_mainWindow,
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Confirmation.Title"),
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Confirmation.Text"),
			QMessageBox::Ok | QMessageBox::Cancel);

		if (reply == QMessageBox::Ok) {
			StopOnBoardingUI();

			///
			// Also resets SEConfig::STARTUP_FLAGS_SIGNED_IN
			//
			// This is to keep our state consistent and not display "Logout"
			// when we are in fact already logged out.
			//
			apiServices->config->SetStartupFlags(
				SEConfig::STARTUP_FLAGS_ONBOARDING_MODE);

			apiServices->obsSceneManager->Reset();
			apiServices->outputManager->Reset();
			apiServices->sharedVideoCompositionManager->Reset();
			apiServices->videoCompositionManager->Reset();

			apiServices->menuManager->Update();
		}
	} else {
		QMessageBox::question(
			m_mainWindow,
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Disabled.Title"),
			obs_module_text(
				"StreamElements.Action.StopOnBoardingUI.Disabled.Text"),
			QMessageBox::Ok);
	}
}
