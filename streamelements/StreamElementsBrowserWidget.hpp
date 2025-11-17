#pragma once

#include "StreamElementsUtils.hpp"
#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsMessageBus.hpp"
#include "StreamElementsWebsocketApiServer.hpp"

#include <QWidget>
#include <QHideEvent>
#include <QCloseEvent>

#include <util/platform.h>
#include <util/threading.h>
#include "cef-headers.hpp"

#include <pthread.h>
#include <functional>
#include <mutex>

#include "StreamElementsApiMessageHandler.hpp"

#include "StreamElementsVideoComposition.hpp"
#include "StreamElementsVideoCompositionViewWidget.hpp"

#include <QtWidgets>

#ifdef APPLE
#include <QMacCocoaViewContainer>
#include <Cocoa/Cocoa.h>
#endif

class QCefWidget;
class QCefCookieManager;

class StreamElementsBrowserWidget :
	public QWidget

{
	Q_OBJECT

private:
	std::string m_url;
	std::string m_executeJavaScriptCodeOnLoad;
	std::string m_reloadPolicy;
	std::string m_pendingLocationArea;
	std::shared_ptr<StreamElementsApiMessageHandler> m_requestedApiMessageHandler;
	StreamElementsMessageBus::message_destination_filter_flags_t
		m_messageDestinationFlags;

	bool m_isIncognito = false;

	QWidget *m_activeVideoCompositionViewWidgetContainer = nullptr;
	StreamElementsVideoCompositionViewWidget
		*m_activeVideoCompositionViewWidget = nullptr;

	bool m_isDestroyed = false;

public:
	StreamElementsBrowserWidget(
		QWidget* parent,
		StreamElementsMessageBus::
			message_destination_filter_flags_t messageDestinationFlags,
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad,
		const char* const reloadPolicy,
		const char* const locationArea,
		const char* const id,
		std::shared_ptr<StreamElementsApiMessageHandler> apiMessageHandler = nullptr,
		bool isIncognito = false);

	~StreamElementsBrowserWidget();

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
		std::shared_ptr<StreamElementsVideoCompositionBase>,
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

			DispatchClientMessage(m_clientId, msg);
		}

		// Dispatch hostVisibilityChanged event
		{
			DispatchJSEventContainer(m_clientId,
					      "hostContainerVisibilityChanged",
					      "null");
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
	static std::map<std::string, StreamElementsBrowserWidget *> s_widgets;

	StreamElementsWebsocketApiServer::message_handler_t m_msgHandler =
		nullptr;

public:
	static StreamElementsBrowserWidget *
	GetWidgetByMessageTargetId(std::string target);
};

