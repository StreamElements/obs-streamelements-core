#pragma once

#include "StreamElementsUtils.hpp"
#include "StreamElementsBrowserWidget.hpp"

#include <QWidget>
#include <QHideEvent>

#include <util/platform.h>
#include <util/threading.h>
#include <include/cef_base.h>
#include <include/cef_version.h>
#include <include/cef_app.h>
#include <include/cef_task.h>
#include <include/base/cef_bind.h>
#include <include/wrapper/cef_closure_task.h>
#include <include/base/cef_lock.h>

#include <pthread.h>
#include <functional>
#include <mutex>

#include "../browser-client.hpp"

#include "StreamElementsAsyncTaskQueue.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsApiMessageHandler.hpp"

#include <QtWidgets>

#ifdef APPLE
#include <QMacCocoaViewContainer>
#include <Cocoa/Cocoa.h>
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 6, 0)
#define SUPPORTS_FRACTIONAL_SCALING
#endif

class StreamElementsBrowserWidget:
	public QWidget

{
	Q_OBJECT

private:
	std::string m_url;
	std::string m_executeJavaScriptCodeOnLoad;
	std::string m_reloadPolicy;
	std::string m_pendingLocationArea;
	std::string m_pendingId;
	CefRefPtr<StreamElementsApiMessageHandler> m_requestedApiMessageHandler;

	QSize m_sizeHint;

	bool m_isIncognito = false;

public:
	StreamElementsBrowserWidget(
		QWidget* parent,
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad,
		const char* const reloadPolicy,
		const char* const locationArea,
		const char* const id,
		StreamElementsApiMessageHandler* apiMessageHandler = nullptr,
		bool isIncognito = false);

	~StreamElementsBrowserWidget();

	void setSizeHint(QSize& size)
	{
		m_sizeHint = size;
	}

	void setSizeHint(const int w, const int h)
	{
		m_sizeHint = QSize(w, h);
	}

public:
	std::string GetStartUrl();
	std::string GetExecuteJavaScriptCodeOnLoad();
	std::string GetReloadPolicy();
	std::string GetCurrentUrl();

	bool BrowserHistoryCanGoBack();
	bool BrowserHistoryCanGoForward();
	void BrowserHistoryGoBack();
	void BrowserHistoryGoForward();
	void BrowserReload(bool ignoreCache);
	void BrowserLoadInitialPage(const char* const url = nullptr);

private:
	///
	// Browser initialization
	//
	// Create browser or navigate back to home page (obs-browser-wcui-browser-dialog.html)
	//
	void InitBrowserAsync();
	void CefUIThreadExecute(std::function<void()> func, bool async);

private slots:
	void InitBrowserAsyncInternal();

private:
	std::string GetInitialPageURLInternal();

private:
	StreamElementsAsyncTaskQueue m_task_queue;
	cef_window_handle_t m_window_handle;
	CefRefPtr<CefBrowser> m_cef_browser;

private:
	bool m_isWidgetInitialized = false;

protected:
	virtual bool event(QEvent* event) override
	{
		if (!m_isWidgetInitialized) {
			AdviseHostWidgetHiddenChange(!isVisible());

			InitBrowserAsync();

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

		if (isVisible()) {
			// http://doc.qt.io/qt-5/qwidget.html#visible-prop
			//
			// A widget that happens to be obscured by other windows
			// on the screen is considered to be visible. The same
			// applies to iconified windows and windows that exist on
			// another virtual desktop (on platforms that support this
			// concept). A widget receives spontaneous show and hide
			// events when its mapping status is changed by the window
			// system, e.g. a spontaneous hide event when the user
			// minimizes the window, and a spontaneous show event when
			// the window is restored again.
			//
			ShowBrowser();
		}

		AdviseHostWidgetHiddenChange(!isVisible());

		emit browserStateChanged();
	}

	virtual void hideEvent(QHideEvent *hideEvent) override
	{
		QWidget::hideEvent(hideEvent);

		if (!isVisible()) {
			// http://doc.qt.io/qt-5/qwidget.html#visible-prop
			//
			// A widget that happens to be obscured by other windows
			// on the screen is considered to be visible. The same
			// applies to iconified windows and windows that exist on
			// another virtual desktop (on platforms that support this
			// concept). A widget receives spontaneous show and hide
			// events when its mapping status is changed by the window
			// system, e.g. a spontaneous hide event when the user
			// minimizes the window, and a spontaneous show event when
			// the window is restored again.
			//
			HideBrowser();
		}

		AdviseHostWidgetHiddenChange(!isVisible());

		emit browserStateChanged();
	}

	virtual void resizeEvent(QResizeEvent* event) override
	{
		QWidget::resizeEvent(event);

		UpdateBrowserSize();

		emit browserStateChanged();
	}

	virtual void moveEvent(QMoveEvent* event) override
	{
		QWidget::moveEvent(event);

		UpdateBrowserSize();

		emit browserStateChanged();
	}

	virtual void changeEvent(QEvent* event) override
	{
		QWidget::changeEvent(event);

		if (event->type() == QEvent::ParentChange) {
			if (!parent()) {
				HideBrowser();
			}
		}
	}

	virtual void focusInEvent(QFocusEvent *event) override;
	virtual void focusOutEvent(QFocusEvent *event) override;

private:
	void UpdateBrowserSize()
	{
		if (!!m_cef_browser.get()) {
#ifdef SUPPORTS_FRACTIONAL_SCALING
			QSize size = this->size() * devicePixelRatioF();
#else
			QSize size = this->size() * devicePixelRatio();
#endif

#ifdef WIN32
			// Make sure window updates on multiple monitors with different DPI

			cef_window_handle_t hWnd = m_cef_browser->GetHost()->GetWindowHandle();

			::SetWindowPos(hWnd, nullptr, 0, 0, size.width(), size.height(),
				SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);

			::SendMessage(hWnd, WM_SIZE, 0,
				MAKELPARAM(size.width(), size.height()));

			/*
			::SetWindowPos(hWnd, HWND_TOP, 0, 0, width(), height(), SWP_DRAWFRAME | SWP_SHOWWINDOW);

			::MoveWindow(hWnd, 0, 0, width(), height(), TRUE);

			// Make sure window updates on multiple monitors with different DPI
			::SendMessage(hWnd, WM_SIZE, 0, MAKELPARAM(width(), height()));
			*/
#endif
		}
	}

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

		if (!m_cef_browser.get()) {
			return;
		}

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

			SendBrowserProcessMessage(m_cef_browser, PID_RENDERER,
						  msg);
		}

		// Dispatch hostVisibilityChanged event
		{
			CefRefPtr<CefProcessMessage> msg =
				CefProcessMessage::Create("DispatchJSEvent");
			CefRefPtr<CefListValue> args = msg->GetArgumentList();

			args->SetString(0, "hostContainerVisibilityChanged");
			args->SetString(1, "null");

			SendBrowserProcessMessage(m_cef_browser, PID_RENDERER,
						  msg);
		}
	}

	void HideBrowser()
	{
		if (m_cef_browser.get() != NULL) {
#ifdef WIN32
			::ShowWindow(
				m_cef_browser->GetHost()->GetWindowHandle(),
				SW_HIDE);
#endif
		}
	}

	void ShowBrowser()
	{
		if (m_cef_browser.get() != NULL) {
#ifdef WIN32
			::ShowWindow(
				m_cef_browser->GetHost()->GetWindowHandle(),
				SW_SHOW);
#endif
		}
	}

	void DestroyBrowser()
	{
		std::lock_guard<std::mutex> guard(m_create_destroy_mutex);

		if (!!m_cef_browser.get()) {
			HideBrowser();

#ifdef WIN32
			// Detach browser to prevent WM_CLOSE event from being sent
			// from CEF to the parent window.
			::SetParent(
				m_cef_browser->GetHost()->GetWindowHandle(),
				0L);

			m_cef_browser->GetHost()->WasHidden(true);
			// Calling this on MacOS causes quit signal to propagate to the main window
			// and quit the app
			m_cef_browser->GetHost()->CloseBrowser(true);
#endif
			
			m_cef_browser = nullptr;
		}
	}

private:
	std::mutex m_create_destroy_mutex;

signals:
	void browserStateChanged();

	// Fired when focused DOM node changes in the browser,
	// indicating whether it is editable.
	void browserFocusedDOMNodeEditableChanged(bool isEditable);

public:
	// Is currently focused DOM node in the browser editable?
	bool isBrowserFocusedDOMNodeEditable() { return m_isBrowserFocusedDOMNodeEditable; }

private:
	void emitBrowserStateChanged()
	{
		emit browserStateChanged();
	}

	bool m_isBrowserFocusedDOMNodeEditable = false;
	void setBrowserFocusedDOMNodeEditable(bool isEditable)
	{
		if (isEditable == m_isBrowserFocusedDOMNodeEditable)
			return;

		m_isBrowserFocusedDOMNodeEditable = isEditable;

		emit browserFocusedDOMNodeEditableChanged(isEditable);
	}

public:
	void BrowserCopy();
	void BrowserCut();
	void BrowserPaste();
	void BrowserSelectAll();

	//
	// Receives events from StreamElementsCefClient and translates
	// them to calls which StreamElementsBrowserWidget understands.
	//
	// Calls to StreamElementsBrowserWidget are made on the QT app
	// thread.
	//
	class StreamElementsBrowserWidget_EventHandler :
		public StreamElementsCefClientEventHandler
	{
	public:
		StreamElementsBrowserWidget_EventHandler(StreamElementsBrowserWidget* widget) : m_widget(widget)
		{ }

		~StreamElementsBrowserWidget_EventHandler()
		{
			if (m_last_pending_future.valid()) {
				m_last_pending_future.wait_for(std::chrono::milliseconds(1000));
			}

			m_widget = nullptr;
		}

	public:
		// CEF loading state was changed
		virtual void OnLoadingStateChange(CefRefPtr<CefBrowser> /*browser*/,
			bool /*isLoading*/,
			bool /*canGoBack*/,
			bool /*canGoForward*/) override
		{
			std::lock_guard<decltype(m_mutex)> guard(m_mutex);

			m_last_pending_future = QtPostTask([this]() {
				if (!m_widget)
					return;

				m_widget->emitBrowserStateChanged();
			});
		}

		// CEF got focus
		virtual void OnGotFocus(CefRefPtr<CefBrowser>) override {
			std::lock_guard<decltype(m_mutex)> guard(m_mutex);

			m_last_pending_future = QtPostTask([this]() {
				if (!m_widget)
					return;

				m_widget->setFocus();
			});
		}

		// Focused DOM node in CEF changed
		virtual void OnFocusedDOMNodeChanged(CefRefPtr<CefBrowser>,
						     bool isEditable) override
		{
			std::lock_guard<decltype(m_mutex)> guard(m_mutex);

			m_last_pending_future = QtPostTask([this, isEditable]() {
				if (!m_widget)
					return;

				m_widget->setBrowserFocusedDOMNodeEditable(
					isEditable);
			});
		}

	private:
		StreamElementsBrowserWidget* m_widget;
		std::mutex m_mutex;
		std::future<void> m_last_pending_future;
	};

};

