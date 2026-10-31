#include "SEPopupWindowManager.hpp"

#include "SEUtils.hpp"
#include "SEApiServices.hpp"
#include "SEApiMessageHandler.hpp"
#include "SEBrowserWidget.hpp"
#include "SEMessageBus.hpp"

#include <QMainWindow>

#include <obs-module.h>

#include <algorithm>
#include <string>

SEPopupWindowManager::SEPopupWindowManager(
	std::shared_ptr<SEApiServices> apiServices)
	: m_apiServices(apiServices)
{
}

SEPopupWindowManager::~SEPopupWindowManager()
{
	//
	// Close each browser while its widget tree is still intact, then
	// delete the window -- the same order, and for the same reason, as
	// ~SEBrowserWidgetManager uses for docks (CORE-1060):
	// closeBrowser() turns a nested Qt event loop, and doing that from
	// inside ~QWidget hands control to unrelated code while the tree is
	// half destroyed. DestroyBrowser() is idempotent, so the widget's own
	// destructor call becomes a no-op.
	//
	// Nothing did this before: the windows were untracked, so an open
	// pop-up and its CEF browser went into Qt's teardown live.
	//
	for (auto &popup : m_windows) {
		if (popup.browser) {
			popup.browser->DestroyBrowser();
		}

		if (popup.window) {
			delete popup.window;
		}
	}

	m_windows.clear();
}

bool SEPopupWindowManager::DeserializePopupWindow(CefRefPtr<CefValue> input)
{
	//
	// A typed getter on the CefValue model is
	// `if (m_type != VTYPE_X) throw;`, and a bare throw outside a catch
	// is std::terminate -- so every read of a page-supplied value needs
	// its type checked first, members included: HasKey() alone forwards
	// to the same getter.
	//
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("url") && d->GetType("url") == VTYPE_STRING) {
		auto apiServices = m_apiServices.lock();

		if (!apiServices) {
			blog(LOG_WARNING,
			     "obs-streamelements-core: SEPopupWindowManager: not opening a pop-up window: the services bundle is gone");

			return false;
		}

		std::string url = d->GetString("url").ToString();
		std::string executeJavaScriptOnLoad;

		//
		// Read and then ignored, which is how this has always
		// behaved: every pop-up gets the host API. Kept as-is
		// because a page may depend on it.
		//
		bool enableHostApi = d->HasKey("enableHostApi") &&
				     d->GetType("enableHostApi") ==
					     VTYPE_BOOL &&
				     d->GetBool("enableHostApi");

		if (d->HasKey("executeJavaScriptOnLoad") &&
		    d->GetType("executeJavaScriptOnLoad") == VTYPE_STRING) {
			executeJavaScriptOnLoad =
				d->GetString("executeJavaScriptOnLoad");
		}

		std::shared_ptr<SEApiMessageHandler> apiMessageHandler =
			nullptr;

		apiMessageHandler = std::make_shared<SEApiMessageHandler>(
			"popupWindow", apiServices);

		QMainWindow *window = new QMainWindow();

		auto browserWidget = new SEBrowserWidget(
			nullptr, SEMessageBus::DEST_UI, url.c_str(),
			executeJavaScriptOnLoad.c_str(), "reload",
			"popupWindow", CreateGloballyUniqueIdString().c_str(),
			apiServices, apiMessageHandler, false);

		window->setCentralWidget(browserWidget);

		window->show();

		//
		// Drop the windows Qt has already destroyed before adding
		// another, so a long session does not accumulate dead slots.
		//
		m_windows.erase(std::remove_if(m_windows.begin(),
					       m_windows.end(),
					       [](const PopupWindow &popup) {
						       return !popup.window;
					       }),
				m_windows.end());

		m_windows.push_back({window, browserWidget});

		return true;
	}

	return false;
}
