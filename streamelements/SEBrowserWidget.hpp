#pragma once

#include "SEUtils.hpp"
#include "SEBrowserWidget.hpp"
#include "SEMessageBus.hpp"
#include "SEWebsocketApiServer.hpp"
#include "SEHostEventDispatcher.hpp"

class SEVideoCompositionManager;

#include <QWidget>
#include <QPointer>
#include <QHideEvent>
#include <QCloseEvent>

#include <util/platform.h>
#include <util/threading.h>
#include "cef-headers.hpp"

#include <pthread.h>
#include <functional>
#include <mutex>

#include "SEApiMessageHandler.hpp"

#include "SEVideoComposition.hpp"
#include "SEVideoCompositionViewWidget.hpp"

#include <QtWidgets>

#ifdef APPLE
#include <QMacCocoaViewContainer>
#include <Cocoa/Cocoa.h>
#endif

class QCefWidget;
class QCefCookieManager;

class SEBrowserWidget :
	public QWidget

{
	Q_OBJECT

private:
	std::string m_url;
	std::string m_executeJavaScriptCodeOnLoad;
	std::string m_reloadPolicy;
	std::string m_pendingLocationArea;
	std::shared_ptr<SEApiMessageHandler> m_requestedApiMessageHandler;

	//
	// The services bundle is deliberately not a member: storing it would
	// put an edge from every widget back to the bundle, in a class the
	// bundle's own widget managers destroy. What this class needs after
	// construction is these two, by name.
	//
	// Weak, all three, because of the one thing this class cannot
	// promise: when it dies. Its owners defer deletion on purpose -- a
	// background worker's dock goes through SEDeleteDockWidgetWhenSafe()
	// and a dock widget can ride into Qt's own teardown -- so a browser
	// widget routinely outlives Shutdown().
	//
	// Held strongly, such a widget kept these three services alive with
	// it, and through the composition manager the custom compositions
	// and their signal data too. Measured: four services still standing
	// at the end of Shutdown() on a clean run, all of them collapsing
	// the instant one surviving worker widget was deleted. Weak is what
	// makes "the plug-in has shut down" and "its services are gone" the
	// same moment, which is what the audit at the end of Shutdown()
	// checks.
	//
	// None of the three is a ring -- no service in here owns a browser
	// widget -- so this was lifetime extension rather than a leak. A
	// failed lock means the plug-in is gone, which is exactly when none
	// of the calls below has anything left to do: there is no registry
	// to unregister from, no composition to look up, and no page to
	// dispatch to.
	//
	std::weak_ptr<SEWebsocketApiServer> m_apiServer;
	std::weak_ptr<SEVideoCompositionManager> m_videoCompositionManager;

	//
	// Three, now: a widget announces its own visibility to its page and
	// rebinds window.host.hostContainerHidden when it changes.
	//
	std::weak_ptr<SEHostEventDispatcher> m_hostEventDispatcher;
	SEMessageBus::message_destination_filter_flags_t
		m_messageDestinationFlags;

	bool m_isIncognito = false;

	// QPointer: both are Qt children of this widget, so Qt may destroy them
	// first during teardown. RemoveVideoCompositionView() deletes them by
	// hand and must not do so twice (CORE-786).
	QPointer<QWidget> m_activeVideoCompositionViewWidgetContainer = nullptr;
	QPointer<SEVideoCompositionViewWidget>
		m_activeVideoCompositionViewWidget = nullptr;

	bool m_isDestroyed = false;

public:
	SEBrowserWidget(QWidget *parent,
			SEMessageBus::message_destination_filter_flags_t
				messageDestinationFlags,
			const char *const url,
			const char *const executeJavaScriptCodeOnLoad,
			const char *const reloadPolicy,
			const char *const locationArea, const char *const id,
			std::shared_ptr<SEApiServices> apiServices,
			std::shared_ptr<SEApiMessageHandler> apiMessageHandler =
				nullptr,
			bool isIncognito = false);

	~SEBrowserWidget();

	// To be called by dialogs, since CEF interprets the window close event as a signal to destroy the browser on it's own
	void DestroyBrowser();

private:
	void UpdateCoords();

	void ShutdownApiMessagehandler()
	{
		if (!m_requestedApiMessageHandler.get())
			return;

		m_requestedApiMessageHandler->Shutdown();
	}

public:
	std::string GetStartUrl();
	std::string GetExecuteJavaScriptCodeOnLoad();
	std::string GetReloadPolicy();
	std::string GetCurrentUrl();

	void BrowserReload(bool ignoreCache);
	void BrowserLoadInitialPage(const char* const url = nullptr);

	void SetVideoCompositionView(
		std::shared_ptr<SEVideoCompositionBase>,
		QRect &coords);
	void RemoveVideoCompositionView();

	void DeserializeVideoCompositionView(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output);

	void SerializeVideoCompositionView(CefRefPtr<CefValue> &output);

private:
	std::string GetInitialPageURLInternal();

private:
	QCefCookieManager *m_separateCookieManager = nullptr;
	QWidget *m_cefWidgetContainer = nullptr;
	QCefWidget *m_cefWidget = nullptr;
	std::string m_clientId;

private:
	bool m_isWidgetInitialized = false;

protected:
	virtual bool event(QEvent* event) override
	{
		if (!m_isWidgetInitialized) {
			AdviseHostWidgetHiddenChange(!isVisible());

			m_isWidgetInitialized = true;
		}

		if (event->type() == QEvent::Polish) {
			AdviseHostWidgetHiddenChange(!isVisible());
		}

		return QWidget::event(event);
	}

	virtual void showEvent(QShowEvent* showEvent) override
	{
		QWidget::showEvent(showEvent);

		UpdateCoords();

		AdviseHostWidgetHiddenChange(!isVisible());

		emit browserStateChanged();
	}

	virtual void hideEvent(QHideEvent *hideEvent) override
	{
		QWidget::hideEvent(hideEvent);

		AdviseHostWidgetHiddenChange(!isVisible());

		emit browserStateChanged();
	}

	virtual void resizeEvent(QResizeEvent* event) override
	{
		QWidget::resizeEvent(event);

		UpdateCoords();

		emit browserStateChanged();
	}

	virtual void moveEvent(QMoveEvent* event) override
	{
		QWidget::moveEvent(event);

		emit browserStateChanged();
	}

	virtual void focusInEvent(QFocusEvent *event) override;
	virtual void focusOutEvent(QFocusEvent *event) override;

private:
	bool m_advisedHostWidgetHiddenChange = false;
	bool m_prevAdvisedHostWidgetHiddenState = false;

protected:
	void AdviseHostWidgetHiddenChange(bool isHidden)
	{
		if (m_requestedApiMessageHandler) {
			m_requestedApiMessageHandler->setInitialHiddenState(!isVisible());
		}

		if (m_advisedHostWidgetHiddenChange) {
			if (m_prevAdvisedHostWidgetHiddenState == isHidden) {
				return;
			}
		}

		m_advisedHostWidgetHiddenChange = true;
		m_prevAdvisedHostWidgetHiddenState = isHidden;

		// Change window.host.hostHidden
		{
			// Context created, request creation of window.host object
			// with API methods
			CefRefPtr<CefValue> root = CefValue::Create();

			CefRefPtr<CefDictionaryValue> rootDictionary = CefDictionaryValue::Create();
			root->SetDictionary(rootDictionary);

			rootDictionary->SetBool("hostContainerHidden", isHidden);

			// Convert data to JSON
			CefString jsonString =
				CefWriteJSON(root, JSON_WRITER_DEFAULT);

			// Send request to renderer process
			CefRefPtr<CefProcessMessage> msg =
				CefProcessMessage::Create("CefRenderProcessHandler::BindJavaScriptProperties");

			msg->GetArgumentList()->SetString(0, "host");
			msg->GetArgumentList()->SetString(1, jsonString);

			if (auto dispatcher = m_hostEventDispatcher.lock()) {
				dispatcher->DispatchClientMessage(m_clientId,
								 msg);
			}
		}

		// Dispatch hostVisibilityChanged event
		{
			if (auto dispatcher = m_hostEventDispatcher.lock()) {
				dispatcher->DispatchJSEventContainer(
					m_clientId,
					"hostContainerVisibilityChanged",
					"null");
			}
		}
	}

signals:
	void browserStateChanged();

private:
	void emitBrowserStateChanged()
	{
		emit browserStateChanged();
	}

private:
	static std::recursive_mutex s_mutex;
	static std::map<std::string, SEBrowserWidget *> s_widgets;

	SEWebsocketApiServer::message_handler_t m_msgHandler =
		nullptr;

public:
	static SEBrowserWidget *
	GetWidgetByMessageTargetId(std::string target);
};

