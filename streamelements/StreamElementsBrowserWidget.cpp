#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "../panel/browser-panel.hpp"

struct QCef;
static QCef *cefMgr;

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
	QWidget *parent,
	StreamElementsMessageBus::message_destination_filter_flags_t
		messageDestinationFlags, const char *const url,
	const char *const executeJavaScriptCodeOnLoad,
	const char *const reloadPolicy, const char *const locationArea,
	const char *const id,
	StreamElementsApiMessageHandler *apiMessageHandler, bool isIncognito)
	: QWidget(parent),
	  m_messageDestinationFlags(messageDestinationFlags),
	  m_url(url),
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

	m_clientId = CreateGloballyUniqueIdString();

	if (m_requestedApiMessageHandler == nullptr) {
		m_requestedApiMessageHandler =
			new StreamElementsApiMessageHandler();
	}

	uint16_t port = StreamElementsGlobalStateManager::GetInstance()
				->GetWebsocketApiServer()
				->GetPort();

	StreamElementsGlobalStateManager::GetInstance()
		->GetWebsocketApiServer()
		->RegisterMessageHandler(
			m_clientId, [this](std::string source,
					 CefRefPtr<CefProcessMessage> msg) {
				m_requestedApiMessageHandler
					->OnProcessMessageReceived(source, msg,
								   0);
			});

	if (!cefMgr) {
		cefMgr = obs_browser_init_panel();
	}

	if (!cefMgr->initialized()) {
		cefMgr->init_browser();
		cefMgr->wait_for_browser_init();
	}

	m_cefWidget =
		cefMgr->create_widget(nullptr, GetInitialPageURLInternal());

	char portBuffer[8];

	std::string script =
		"window.host = window.host || {}; window.host.endpoint = { source: '" +
		m_clientId + "', port: '" + itoa(port, portBuffer, 10) +
		"', ws: new WebSocket(`ws://localhost:" +
		itoa(port, portBuffer, 10) + "`) };\n" +
		"window.host.endpoint.ws.onopen = () => {" +
		"console.log('ws.onopen');" +
		"window.host.endpoint.ws.send(JSON.stringify({ type: 'register', payload: { id: window.host.endpoint.source }}));" +
		"};" + "window.host.endpoint.ws.onmessage = (event) => {" +
		"	const json = JSON.parse(event.data);\n" +
		"	console.log('ws.onmessage: ', json);"
		"	if (json.type === 'register:response') {" +
		"		window.host.endpoint.callbacks = {};\n" +
		"		window.host.endpoint.callbackIdSequence = 0;\n" +
		"		window.host.endpoint.ws.send(JSON.stringify({ type: 'dispatch', source: window.host.endpoint.source, payload: { name: 'CefRenderProcessHandler::OnContextCreated', args: [] } }));\n" +
		"	} else if (json.type === 'dispatch') {\n" +
		"		if (json.payload.name === 'CefRenderProcessHandler::BindJavaScriptProperties') {\n" +
		"			const defs = JSON.parse(json.payload.args[1]);\n" +
		"			window[json.payload.args[0]] = window[json.payload.args[0]] || {};\n" +
		"			for (const key of Object.keys(defs)) {\n" +
		"				window[json.payload.args[0]][key] = defs[key];\n" +
		"			}\n" +
		"		} else if (json.payload.name === 'CefRenderProcessHandler::BindJavaScriptFunctions') {\n" +
		"			const defs = JSON.parse(json.payload.args[1]);\n" +
		"			for (const key of Object.keys(defs)) {\n" +
		"				const fullName = `window.${json.payload.args[0]}.${key}`;\n" +
		"				window[json.payload.args[0]][key] = (...args) => {\n" +
		"					const callback = args.pop();\n" +
		"					const callbackId = ++window.host.endpoint.callbackIdSequence;\n" +
		"					window.host.endpoint.callbacks[callbackId] = callback;\n" +
		"					window.host.endpoint.ws.send(JSON.stringify({\n" +
		"						type: 'dispatch', payload: {\n" +
		"							name: defs[key].message,\n" +
		"							args: [ 4, defs[key].message, fullName, key, ...(args.map(arg => JSON.stringify(arg))), callbackId ]\n" +
		"						}\n" +
		"					}));\n" +
		"				};\n" +
		"			}\n" +
		"		} else if (json.payload.name === 'executeCallback') {\n" +
		"			const [ callbackId, ...args ] = json.payload.args;\n" +
		"			window.host.endpoint.callbacks[callbackId](...args);\n" +
		"			delete window.host.endpoint.callbacks[callbackId];\n" +
		"		} else if (json.payload.name === 'DispatchJSEvent') {\n" +
		"			window.dispatchEvent(new CustomEvent(json.payload.args[0], { detail: JSON.parse(json.payload.args[1]) }));\n" +
		"		}\n" + "	}\n" + "};";

	m_cefWidget->setStartupScript(script + m_executeJavaScriptCodeOnLoad);

	this->layout()->addWidget(m_cefWidget);

	// TODO: Add argument to change message bus destination flags
	StreamElementsMessageBus::GetInstance()->AddListener(
		m_clientId, m_messageDestinationFlags);
}

StreamElementsBrowserWidget::~StreamElementsBrowserWidget()
{
	UnregisterAppActiveTrackerWidget(this);

	m_requestedApiMessageHandler = nullptr;
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

std::string StreamElementsBrowserWidget::GetStartUrl()
{
	return m_url;
}

std::string StreamElementsBrowserWidget::GetCurrentUrl()
{
	SYNC_ACCESS();

	return m_url;
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
	return false;
}

bool StreamElementsBrowserWidget::BrowserHistoryCanGoForward()
{
	return false;
}

void StreamElementsBrowserWidget::BrowserHistoryGoBack()
{
}

void StreamElementsBrowserWidget::BrowserHistoryGoForward()
{
}

void StreamElementsBrowserWidget::BrowserReload(bool ignoreCache)
{
	m_cefWidget->reloadPage();
}

void StreamElementsBrowserWidget::BrowserLoadInitialPage(const char *const url)
{
	if (url) {
		m_url = url;
	}

	m_cefWidget->setURL(m_url);
	m_cefWidget->reloadPage();
}

void StreamElementsBrowserWidget::focusInEvent(QFocusEvent *event)
{
	QWidget::focusInEvent(event);

	blog(LOG_INFO, "QWidget::focusInEvent: reason %d: %s", event->reason(),
	     m_url.c_str());

	StreamElementsGlobalStateManager::GetInstance()
		->GetMenuManager()
		->SetFocusedBrowserWidget(this);

	m_cefWidget->setFocus();
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

		m_cefWidget->clearFocus();
	}
}

void StreamElementsBrowserWidget::BrowserCopy()
{
}

void StreamElementsBrowserWidget::BrowserCut()
{
}

void StreamElementsBrowserWidget::BrowserPaste()
{
}

void StreamElementsBrowserWidget::BrowserSelectAll()
{
}
