#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#ifdef USE_QT_LOOP
#include "browser-app.hpp"
#endif

#include <functional>
#include <mutex>
#include <regex>

/* ========================================================================= */

//
// This block deals with a Win32-specific bug inherent to Qt 5.15.2:
// when OBS loses input focus (app is deactivated), the currently
// focused browser widget does not receive a KillFocus event, although
// CEF does lose input focus.
//
// This makes any operations dependant on correctly tracking browser
// widgets' focus state unstable.
//
// To mitigate this, on Windows, we subscribe to WH_CALLWNDPROC hook
// for the application. Once the app receives a Windows event indicating
// that the app was deactivated, we call `clearFocus()` on the currently
// focused browser widget (if any).
//
// This results in the browser widget's focused state correctly reflecting
// the focused state of the CEF browser it embeds.
//
// The Win32 hook approach is specific to Win32.
//
// On macOS focusIn() and focusOut() signals emitted by QWidget seem to
// function properly.
//

#include <set>

//
// Registry of all browser widgets instances.
//
// StreamElementsBrowserWidget registers in it's ctor
// and deregisters it it's dtor.
//
static std::recursive_mutex g_AppActiveTrackerMutex;
static std::set<QWidget *> g_AppActiveTrackerWidgets;

//
// Called by our hook procedure when app deactivates (loses focus).
//
static void HandleAppActiveTrackerWidgetUnfocus()
{
	std::lock_guard<decltype(g_AppActiveTrackerMutex)> guard(
		g_AppActiveTrackerMutex);

	for (auto &widget : g_AppActiveTrackerWidgets) {
		if (widget->hasFocus()) {
			widget->clearFocus();
		}
	}
}

#ifdef _WIN32
// Our WH_CALLWNDPROC hook handle
static HHOOK g_AppActiveTrackerHook = NULL;

// Our WH_CALLWNDPROC hook procedure
static LRESULT CALLBACK AppActiveTrackerHook(_In_ int nCode, _In_ WPARAM wParam,
					     _In_ LPARAM lParam)
{
	CWPSTRUCT *msg = (CWPSTRUCT *)lParam;

	if (msg->message == WM_ACTIVATEAPP) {
		// Windows message indicating of app active/inactive state

		if (!msg->wParam) {
			// App deactivated (lost focus)

			if (QThread::currentThread() == qApp->thread()) {
				HandleAppActiveTrackerWidgetUnfocus();
			} else {
				// We're unlikely to reach here, but just in case
				QtPostTask([]() {
					HandleAppActiveTrackerWidgetUnfocus();
				});
			}
		}
	}

	return CallNextHookEx(NULL, nCode, wParam, lParam);
}
#endif

//
// Subscribe to receive notifications re app becoming active/inactive
//
static void InitAppActiveTracker()
{
#ifdef _WIN32
	if (g_AppActiveTrackerHook)
		return;

	g_AppActiveTrackerHook =
		SetWindowsHookExA(WH_CALLWNDPROC, AppActiveTrackerHook,
				  NULL, GetCurrentThreadId());

	if (!g_AppActiveTrackerHook) {
		blog(LOG_ERROR,
		     "InitAppActiveTracker: SetWindowsHookExA call failed");
	} else {
		blog(LOG_INFO,
		     "InitAppActiveTracker: SetWindowsHookExA succeeded");
	}
#endif
}

//
// Unsubscribe from notifications re app becoming active/inactive
//
static void ShutdownAppActiveTracker()
{
#ifdef _WIN32
	if (!g_AppActiveTrackerHook)
		return;

	if (UnhookWindowsHookEx(g_AppActiveTrackerHook)) {
		g_AppActiveTrackerHook = NULL;

		blog(LOG_INFO,
		     "ShutdownAppActiveTracker: UnhookWindowsHookEx succeeded");
	} else {
		blog(LOG_ERROR,
		     "ShutdownAppActiveTracker: UnhookWindowsHookEx call failed");
	}
#endif
}

//
// Register a QWidget interested in losing focus when app
// becomes inactive.
//
static void RegisterAppActiveTrackerWidget(QWidget *widget)
{
	std::lock_guard<decltype(g_AppActiveTrackerMutex)> guard(
		g_AppActiveTrackerMutex);

	g_AppActiveTrackerWidgets.emplace(widget);

	if (g_AppActiveTrackerWidgets.size() == 1) {
		InitAppActiveTracker();
	}
}

//
// Unregister a QWidget no longer interested in losing
// focus when app becomes inactive.
//
static void UnregisterAppActiveTrackerWidget(QWidget *widget)
{
	std::lock_guard<decltype(g_AppActiveTrackerMutex)> guard(
		g_AppActiveTrackerMutex);

	g_AppActiveTrackerWidgets.erase(widget);

	if (g_AppActiveTrackerWidgets.size() == 0) {
		ShutdownAppActiveTracker();
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

#include <QVBoxLayout>

StreamElementsBrowserWidget::StreamElementsBrowserWidget(
	QWidget *parent, const char *const url,
	const char *const executeJavaScriptCodeOnLoad,
	const char *const reloadPolicy, const char *const locationArea,
	const char *const id,
	StreamElementsApiMessageHandler *apiMessageHandler, bool isIncognito)
	: QWidget(parent),
	  m_url(url),
	  m_window_handle(0),
	  m_task_queue("StreamElementsBrowserWidget task queue"),
	  m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad == nullptr
						? ""
						: executeJavaScriptCodeOnLoad),
	  m_reloadPolicy(reloadPolicy),
	  m_pendingLocationArea(locationArea == nullptr ? "" : locationArea),
	  m_pendingId(id == nullptr ? "" : id),
	  m_requestedApiMessageHandler(apiMessageHandler),
	  m_isIncognito(isIncognito)
{
	// Create native window
	setAttribute(Qt::WA_NativeWindow);

	// Focus on click
	setFocusPolicy(Qt::ClickFocus);

	// This influences docking widget width/height
	//setMinimumWidth(200);
	//setMinimumHeight(200);

	setLayout(new QVBoxLayout());

	QSizePolicy policy;
	policy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
	policy.setVerticalPolicy(QSizePolicy::MinimumExpanding);
	setSizePolicy(policy);

	RegisterAppActiveTrackerWidget(this);
}

StreamElementsBrowserWidget::~StreamElementsBrowserWidget()
{
	UnregisterAppActiveTrackerWidget(this);

	DestroyBrowser();

	m_requestedApiMessageHandler = nullptr;
}

void StreamElementsBrowserWidget::InitBrowserAsync()
{
	if (!!m_cef_browser.get()) {
		return;
	}

	// Make sure InitBrowserAsyncInternal() runs in Qt UI thread
	QMetaObject::invokeMethod(this, "InitBrowserAsyncInternal");
}

std::string StreamElementsBrowserWidget::GetInitialPageURLInternal()
{
	std::string htmlString = LoadResourceString(":/html/loading.html");
	htmlString = std::regex_replace(htmlString, std::regex("\\$\\{URL\\}"),
					m_url);
	std::string base64uri =
		"data:text/html;base64," +	
		CefBase64Encode(htmlString.c_str(), htmlString.size())
			.ToString();

	return base64uri;
}

void StreamElementsBrowserWidget::InitBrowserAsyncInternal()
{
	if (!!m_cef_browser.get()) {
		return;
	}

	m_window_handle = (cef_window_handle_t)winId();

	CefUIThreadExecute(
		[this]() {
			std::lock_guard<std::mutex> guard(
				m_create_destroy_mutex);

			if (!!m_cef_browser.get()) {
				return;
			}

			StreamElementsBrowserWidget *self = this;

			// CEF window attributes
			CefWindowInfo windowInfo;
			windowInfo.windowless_rendering_enabled = false;

			QSize size = self->size();

#ifdef WIN32
#ifdef SUPPORTS_FRACTIONAL_SCALING
			size *= devicePixelRatioF();
#else
			size *= devicePixelRatio();
#endif

			// Client area rectangle
			//RECT clientRect = {0, 0, size.width(), size.height()};
			windowInfo.SetAsChild(self->m_window_handle,
					      CefRect(0, 0, size.width(),
						      size.height()));
#else
#if CHROME_VERSION_BUILD < 4430
			windowInfo.SetAsChild(self->m_window_handle, 0, 0,
					      size.width(), size.height());
#else
			windowInfo.SetAsChild(self->m_window_handle,
					      CefRect(0, 0, size.width(),
						      size.height()));
#endif
#endif
			CefBrowserSettings cefBrowserSettings;

			cefBrowserSettings.Reset();
			cefBrowserSettings.javascript_close_windows =
				STATE_DISABLED;
			cefBrowserSettings.local_storage = STATE_ENABLED;
			cefBrowserSettings.databases = STATE_ENABLED;
			//cefBrowserSettings.web_security = STATE_ENABLED;
			cefBrowserSettings.webgl = STATE_ENABLED;
			const int DEFAULT_FONT_SIZE = 16;
			cefBrowserSettings.default_font_size = DEFAULT_FONT_SIZE;
			cefBrowserSettings.default_fixed_font_size = DEFAULT_FONT_SIZE;
			//cefBrowserSettings.minimum_font_size = DEFAULT_FONT_SIZE;
			//cefBrowserSettings.minimum_logical_font_size = DEFAULT_FONT_SIZE;

			if (m_requestedApiMessageHandler == nullptr) {
				m_requestedApiMessageHandler =
					new StreamElementsApiMessageHandler();
			}

			CefRefPtr<StreamElementsCefClient> cefClient =
				new StreamElementsCefClient(
					m_executeJavaScriptCodeOnLoad,
					m_requestedApiMessageHandler,
					new StreamElementsBrowserWidget_EventHandler(
						this),
					StreamElementsMessageBus::DEST_UI);

			cefClient->SetLocationArea(m_pendingLocationArea);
			cefClient->SetContainerId(m_pendingId);

			CefRefPtr<CefRequestContext> cefRequestContext =
				StreamElementsGlobalStateManager::GetInstance()
					->GetCookieManager()
					->GetCefRequestContext();

				if (m_isIncognito)
			{
				CefRequestContextSettings
					cefRequestContextSettings;

				cefRequestContextSettings.Reset();

				///
				// CefRequestContext with empty cache path = incognito mode
				//
				// Docs:
				// https://magpcss.org/ceforum/viewtopic.php?f=6&t=10508
				// https://magpcss.org/ceforum/apidocs3/projects/(default)/CefRequestContext.html#GetCachePath()
				//
				CefString(
					&cefRequestContextSettings.cache_path) =
					"";

				cefRequestContext =
					CefRequestContext::CreateContext(
						cefRequestContextSettings,
						nullptr);
			}

			m_cef_browser = CefBrowserHost::CreateBrowserSync(
				windowInfo, cefClient,
				GetInitialPageURLInternal(), cefBrowserSettings,
#if CHROME_VERSION_BUILD >= 3770
#if ENABLE_CREATE_BROWSER_API
				m_requestedApiMessageHandler
					? m_requestedApiMessageHandler
						  ->CreateBrowserArgsDictionary()
					: CefRefPtr<CefDictionaryValue>(),
#else
				CefRefPtr<CefDictionaryValue>(),
#endif
#endif
				cefRequestContext);

			UpdateBrowserSize();
		},
		true);
}

void StreamElementsBrowserWidget::CefUIThreadExecute(std::function<void()> func,
						     bool async)
{
	if (!async) {
#ifdef USE_QT_LOOP
		if (QThread::currentThread() == qApp->thread()) {
			func();
			return;
		}
#endif
		os_event_t *finishedEvent;
		os_event_init(&finishedEvent, OS_EVENT_TYPE_AUTO);
		bool success = QueueCEFTask([&]() {
			func();

			os_event_signal(finishedEvent);
		});
		if (success) {
			os_event_wait(finishedEvent);
		}

		os_event_destroy(finishedEvent);
	} else {
#ifdef USE_QT_LOOP
		QueueBrowserTask(m_cef_browser,
				 [this, func](CefRefPtr<CefBrowser>) {
					 func();
				 });
#else
		QueueCEFTask([this, func]() { func(); });
#endif
	}
}

std::string StreamElementsBrowserWidget::GetStartUrl()
{
	return m_url;
}

std::string StreamElementsBrowserWidget::GetCurrentUrl()
{
	SYNC_ACCESS();

	if (!m_cef_browser.get()) {
		return m_url;
	}

	CefRefPtr<CefFrame> mainFrame = m_cef_browser->GetMainFrame();

	if (!mainFrame.get()) {
		return m_url;
	}

	std::string url = mainFrame->GetURL().ToString();

	if (url.substr(0, 5) == "data:") {
		// "data:" scheme means we're still at the loading page URL.
		//
		// Use the initially specified URL in that case.
		url = m_url;
	}

	return url;
}

std::string StreamElementsBrowserWidget::GetExecuteJavaScriptCodeOnLoad()
{
	return m_executeJavaScriptCodeOnLoad;
}

std::string StreamElementsBrowserWidget::GetReloadPolicy()
{
	return m_reloadPolicy;
}

bool StreamElementsBrowserWidget::BrowserHistoryCanGoBack()
{
	if (!m_cef_browser.get()) {
		return false;
	}

	return m_cef_browser->CanGoBack();
}

bool StreamElementsBrowserWidget::BrowserHistoryCanGoForward()
{
	if (!m_cef_browser.get()) {
		return false;
	}

	return m_cef_browser->CanGoForward();
}

void StreamElementsBrowserWidget::BrowserHistoryGoBack()
{
	if (!BrowserHistoryCanGoBack()) {
		return;
	}

	m_cef_browser->GoBack();
}

void StreamElementsBrowserWidget::BrowserHistoryGoForward()
{
	if (!m_cef_browser.get()) {
		return;
	}

	m_cef_browser->GoForward();
}

void StreamElementsBrowserWidget::BrowserReload(bool ignoreCache)
{
	if (!m_cef_browser.get()) {
		return;
	}

	if (ignoreCache) {
		m_cef_browser->ReloadIgnoreCache();
	} else {
		m_cef_browser->Reload();
	}
}

void StreamElementsBrowserWidget::BrowserLoadInitialPage(const char *const url)
{
	if (!m_cef_browser.get()) {
		return;
	}

	if (url) {
		m_url = url;
	}

	if (m_cef_browser->GetMainFrame()->GetURL() == m_url) {
		m_cef_browser->ReloadIgnoreCache();
	} else {
		m_cef_browser->GetMainFrame()->LoadURL(
			GetInitialPageURLInternal());
	}
}

void StreamElementsBrowserWidget::focusInEvent(QFocusEvent *event)
{
	QWidget::focusInEvent(event);

	blog(LOG_INFO, "QWidget::focusInEvent: reason %d: %s", event->reason(),
	     m_url.c_str());

	StreamElementsGlobalStateManager::GetInstance()
		->GetMenuManager()
		->SetFocusedBrowserWidget(this);

	if (!!m_cef_browser.get() && m_cef_browser->GetHost()) {
		// Notify CEF that it got focus
		m_cef_browser->GetHost()->SetFocus(true);
	}
}

void StreamElementsBrowserWidget::focusOutEvent(QFocusEvent *event)
{
	QWidget::focusOutEvent(event);

	blog(LOG_INFO, "QWidget::focusOutEvent: %s", m_url.c_str());

	if (event->reason() != Qt::MenuBarFocusReason && event->reason() != Qt::PopupFocusReason) {
		// QMenuBar & QMenu grab input focus when open.
		//
		// Since we want the Edit menu to respect the currently focused browser
		// widget, we must make certain that focus grabbed due to QMenuBar or
		// QMenu (popup) grabbing the focus, does not reset currently focused
		// widget state.
		//
		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->SetFocusedBrowserWidget(nullptr);

		if (!!m_cef_browser.get() && m_cef_browser->GetHost()) {
			// Notify CEF that it lost focus
			m_cef_browser->GetHost()->SetFocus(false);
		}
	}
}

void StreamElementsBrowserWidget::BrowserCopy()
{
	if (!m_cef_browser.get())
		return;

	CefRefPtr<CefFrame> frame = m_cef_browser->GetFocusedFrame();

	if (!frame.get())
		return;

	frame->Copy();
}

void StreamElementsBrowserWidget::BrowserCut()
{
	if (!m_cef_browser.get())
		return;

	CefRefPtr<CefFrame> frame = m_cef_browser->GetFocusedFrame();

	if (!frame.get())
		return;

	frame->Cut();
}

void StreamElementsBrowserWidget::BrowserPaste()
{
	if (!m_cef_browser.get())
		return;

	CefRefPtr<CefFrame> frame = m_cef_browser->GetFocusedFrame();

	if (!frame.get())
		return;

	frame->Paste();
}

void StreamElementsBrowserWidget::BrowserSelectAll()
{
	if (!m_cef_browser.get())
		return;

	CefRefPtr<CefFrame> frame = m_cef_browser->GetFocusedFrame();

	if (!frame.get())
		return;

	frame->SelectAll();
}
