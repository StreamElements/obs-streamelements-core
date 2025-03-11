#include "StreamElementsBrowserWidget.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsVideoCompositionViewWidget.hpp"

#include "../../obs-browser/panel/browser-panel.hpp"
#include "../deps/base64/base64.hpp"

#ifndef _WIN32
#include <unistd.h>
#endif

#ifdef USE_QT_LOOP
#include "browser-app.hpp"
#endif

#include <functional>
#include <mutex>
#include <shared_mutex>
#include <regex>

/* ========================================================================= */

static std::shared_mutex s_widgetRegistryMutex;
static std::map<StreamElementsBrowserWidget *, bool> s_widgetRegistry;

/* ========================================================================= */

std::recursive_mutex StreamElementsBrowserWidget::s_mutex;
std::map<std::string, StreamElementsBrowserWidget *>
	StreamElementsBrowserWidget::s_widgets;

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
	} else if (IsTraceLogLevel()) {
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

		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "ShutdownAppActiveTracker: UnhookWindowsHookEx succeeded");
		}
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

#include <QVBoxLayout>

#ifndef _WIN32
static const char* itoa(int input, char* buf, int radix)
{
	sprintf(buf, "%d", input);
	
	return buf;
}
#endif

StreamElementsBrowserWidget::StreamElementsBrowserWidget(
	QWidget *parent,
	StreamElementsMessageBus::message_destination_filter_flags_t
		messageDestinationFlags, const char *const url,
	const char *const executeJavaScriptCodeOnLoad,
	const char *const reloadPolicy, const char *const locationArea,
	const char *const id,
	std::shared_ptr<StreamElementsApiMessageHandler> apiMessageHandler, bool isIncognito)
	: QWidget(parent),
	  m_messageDestinationFlags(messageDestinationFlags),
	  m_url(url),
	  m_executeJavaScriptCodeOnLoad(executeJavaScriptCodeOnLoad == nullptr
						? ""
						: executeJavaScriptCodeOnLoad),
	  m_reloadPolicy(reloadPolicy),
	  m_pendingLocationArea(locationArea == nullptr ? "" : locationArea),
	  m_clientId(id == nullptr ? CreateGloballyUniqueIdString()
				    : id),
	  m_requestedApiMessageHandler(apiMessageHandler),
	  m_isIncognito(isIncognito)
{
	std::lock_guard<decltype(s_mutex)> guard(s_mutex);

	// Create native window
	setAttribute(Qt::WA_NativeWindow);

	// Focus on click
	setFocusPolicy(Qt::ClickFocus);

	// This influences docking widget width/height
	//setMinimumWidth(200);
	//setMinimumHeight(200);

	setContentsMargins(0, 0, 0, 0);
	setLayout(new QVBoxLayout());
	layout()->setContentsMargins(0, 0, 0, 0);

	QSizePolicy policy;
	policy.setHorizontalPolicy(QSizePolicy::MinimumExpanding);
	policy.setVerticalPolicy(QSizePolicy::MinimumExpanding);
	setSizePolicy(policy);

	RegisterAppActiveTrackerWidget(this);

	if (m_requestedApiMessageHandler == nullptr) {
		m_requestedApiMessageHandler =
			std::make_shared<StreamElementsApiMessageHandler>("unknown");
	}

	m_requestedApiMessageHandler->SetBrowserWidget(this);

	uint16_t port = StreamElementsGlobalStateManager::GetInstance()
				->GetWebsocketApiServer()
				->GetPort();

	m_msgHandler = [this](std::string source,
			      CefRefPtr<CefProcessMessage> msg) {
		std::shared_lock<decltype(s_widgetRegistryMutex)> lock;

		if (!s_widgetRegistry.count(this))
			return;

		if (!m_requestedApiMessageHandler.get())
			return;

		m_requestedApiMessageHandler->OnProcessMessageReceived(source,
								       msg, 0);
	};

	StreamElementsGlobalStateManager::GetInstance()
		->GetWebsocketApiServer()
		->RegisterMessageHandler(
			m_clientId, m_msgHandler);

	if (m_isIncognito) {
		char cookie_store_name[128];

#ifdef _WIN32
		sprintf(cookie_store_name, "anonymous_%d.tmp", (int)GetCurrentProcessId());
#else
		sprintf(cookie_store_name, "anonymous_%d.tmp", getpid());
#endif

		StreamElementsGlobalStateManager::GetInstance()
			->GetCleanupManager()
			->AddPath(obs_module_config_path(cookie_store_name));

		m_separateCookieManager =
			StreamElementsGlobalStateManager::GetInstance()
				->GetCef()
				->create_cookie_manager(cookie_store_name, false);

		m_separateCookieManager->DeleteCookies("", "");
	}

	m_cefWidget =
		StreamElementsGlobalStateManager::GetInstance()
			->GetCef()
			->create_widget(
				nullptr, GetInitialPageURLInternal(),
				m_separateCookieManager
					? m_separateCookieManager
					: StreamElementsGlobalStateManager::
						  GetInstance()
							  ->GetCookieManager());

	char portBuffer[8];

	std::string script =
		"window.host = window.host || {}; window.host.endpoint = { source: '" +
		m_clientId + "', port: '" + itoa(port, portBuffer, 10) +
		"', ws: new WebSocket(`ws://localhost:" +
		itoa(port, portBuffer, 10) + "`) };\n" +
		"window.host.endpoint.ws.onopen = () => {" +
		"window.host.endpoint.ws.send(JSON.stringify({ type: 'register', payload: { id: window.host.endpoint.source }}));" +
		"};" + "window.host.endpoint.ws.onmessage = (event) => {" +
		"	const json = JSON.parse(event.data);\n" +
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
		"			window[json.payload.args[0]] = window[json.payload.args[0]] || {};\n" +
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
		"			window.host.endpoint.callbacks[callbackId](...(args.map(arg => JSON.parse(arg))));\n" +
		"			delete window.host.endpoint.callbacks[callbackId];\n" +
		"		} else if (json.payload.name === 'DispatchJSEvent') {\n" +
		"			window.dispatchEvent(new CustomEvent(json.payload.args[0], { detail: JSON.parse(json.payload.args[1]) }));\n" +
		"		}\n" + "	}\n" + "};";

	script += "(function() {\n";
	script += "	let pressed = '';\n";
	script += "\n";
	script += "	function parseKeyEvent(e) {\n";
	script += "		const combo = [];\n";
	script += "		if (e.ctrlKey) combo.push('Ctrl');\n";
	script += "		if (e.altKey) combo.push('Alt');\n";
	script += "		if (e.shiftKey) combo.push('Shift');\n";
	script +=
		"		if (e.key && e.key.length == 1) combo.push(e.key.toUpperCase());\n";
	script += "\n";
	script += "		return {\n";
	script +=
		"			description: combo.join(' + '),\n";
	script += "			keyCode: e.keyCode,\n";
	script += "			keyName: e.key,\n";
	script += "			keySymbol: e.key,\n";
	script += "			virtualKeyCode: e.keyCode,\n";
	script += "			left: /Left$/.test(e.code),\n";
	script +=
		"			right: /Right$/.test(e.code),\n";
	script += "			altKey: e.altKey,\n";
	script += "			ctrlKey: e.ctrlKey,\n";
	script += "			commandKey: e.metaKey,\n";
	script += "			shiftKey: e.shiftKey,\n";
	script += "			capsLock: false,\n";
	script += "			numLock: false,\n";
	script += "			mouseLeftButton: false,\n";
	script += "			mouseMidButton: false,\n";
	script += "			mouseRightButton: false,\n";
	script += "		};\n";
	script += "	}\n";
	script += "\n";
	script += "	window.addEventListener('keydown', e => {\n";
	script += "		if (e.repeat) return;\n";
	script += "		const key = parseKeyEvent(e);\n";
	script += "		const keyId = key.description || key.keySymbol;\n";
	script +=
		"		if (keyId == pressed) return;\n";
	script += "		pressed = keyId;\n";
	script +=
		"		window.dispatchEvent(new CustomEvent('hostContainerKeyCombinationPressed', { detail: key }));\n";
	script += "	});\n";
	script += "\n";
	script += "	window.addEventListener('keyup', e => {\n";
	script += "		if (e.repeat) return;\n";
	script += "		const key = parseKeyEvent(e);\n";
	script += "		pressed = '';\n";
	script +=
		"		window.dispatchEvent(new CustomEvent('hostContainerKeyCombinationReleased', { detail: key }));\n";
	script += "	});\n";
	script += "})();\n";

	m_cefWidget->setStartupScript(script + m_executeJavaScriptCodeOnLoad);

	this->layout()->addWidget(m_cefWidget);

	StreamElementsMessageBus::GetInstance()->AddListener(
		m_clientId, m_messageDestinationFlags);

	s_widgets[m_clientId] = this;

	m_cefWidget->setContentsMargins(0, 0, 0, 0);

	setContentsMargins(0, 0, 0, 0);

	{
		std::unique_lock<decltype(s_widgetRegistryMutex)> lock(
			s_widgetRegistryMutex);

		s_widgetRegistry[this] = true;
	}

	QObject::connect(this, &QObject::destroyed,
			 [this](QObject *obj) { DestroyBrowser(); });
}

StreamElementsBrowserWidget*
StreamElementsBrowserWidget::GetWidgetByMessageTargetId(std::string target)
{
	std::lock_guard<decltype(s_mutex)> guard(s_mutex);

	if (s_widgets.count(target)) {
		return s_widgets[target];
	}

	return nullptr;
}

StreamElementsBrowserWidget::~StreamElementsBrowserWidget()
{
	DestroyBrowser();
}

void StreamElementsBrowserWidget::DestroyBrowser()
{
	ShutdownApiMessagehandler();

	{
		std::unique_lock<decltype(s_widgetRegistryMutex)> lock(
			s_widgetRegistryMutex);

		if (s_widgetRegistry.count(this) > 0) {
			s_widgetRegistry.erase(this);
		}
	}

	std::lock_guard<decltype(s_mutex)> guard(s_mutex);

	RemoveVideoCompositionView();

	if (StreamElementsGlobalStateManager::IsInstanceAvailable()) {
		auto apiServer = StreamElementsGlobalStateManager::GetInstance()
					 ->GetWebsocketApiServer();

		if (apiServer) {
			apiServer->UnregisterMessageHandler(m_clientId,
							    m_msgHandler);
		}
	}

	if (m_requestedApiMessageHandler) {
		m_requestedApiMessageHandler->SetBrowserWidget(nullptr);
	}

	if (s_widgets.count(m_clientId)) {
		s_widgets.erase(m_clientId);
	}

	UnregisterAppActiveTrackerWidget(this);

	m_requestedApiMessageHandler = nullptr;

	if (m_cefWidget) {
		m_cefWidget->closeBrowser();

		m_cefWidget = nullptr;
	}

	if (m_separateCookieManager) {
		m_separateCookieManager->DeleteCookies("", "");

		delete m_separateCookieManager;
	}
}

std::string StreamElementsBrowserWidget::GetInitialPageURLInternal()
{
	return m_url;
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
}

void StreamElementsBrowserWidget::focusInEvent(QFocusEvent *event)
{
	QWidget::focusInEvent(event);

	if (IsTraceLogLevel()) {
		blog(LOG_INFO, "QWidget::focusInEvent: reason %d: %s",
		     event->reason(), m_url.c_str());
	}

	m_cefWidget->setFocus();
}

void StreamElementsBrowserWidget::focusOutEvent(QFocusEvent *event)
{
	QWidget::focusOutEvent(event);

	if (IsTraceLogLevel()) {
		blog(LOG_INFO, "QWidget::focusOutEvent: %s", m_url.c_str());
	}

	if (event->reason() != Qt::MenuBarFocusReason && event->reason() != Qt::PopupFocusReason) {
		m_cefWidget->clearFocus();
	}
}

void StreamElementsBrowserWidget::RemoveVideoCompositionView()
{
	if (!m_activeVideoCompositionViewWidget)
		return;

	m_activeVideoCompositionViewWidget->hide();
	m_activeVideoCompositionViewWidget->Destroy();
	m_activeVideoCompositionViewWidget->deleteLater();

	m_activeVideoCompositionViewWidget = nullptr;
}

void StreamElementsBrowserWidget::SetVideoCompositionView(
	std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition, QRect &coords)
{
	if (m_activeVideoCompositionViewWidget &&
	    m_activeVideoCompositionViewWidget->GetVideoComposition().get() !=
		    videoComposition.get()) {
		// Existing video composition is not the same as the one being set
		RemoveVideoCompositionView();
	}

	if (!m_activeVideoCompositionViewWidget) {
		m_activeVideoCompositionViewWidget =
			new StreamElementsVideoCompositionViewWidget(
				this, videoComposition);
	}

	m_activeVideoCompositionViewWidget->setGeometry(coords);
	m_activeVideoCompositionViewWidget->show();
}

void StreamElementsBrowserWidget::SerializeVideoCompositionView(
	CefRefPtr<CefValue>&
	output)
{
	output->SetNull();

	if (!m_activeVideoCompositionViewWidget)
		return;

	auto root = CefDictionaryValue::Create();

	root->SetString("videoCompositionId", m_activeVideoCompositionViewWidget
						      ->GetVideoComposition()
						      ->GetName());

	auto geometry = CefDictionaryValue::Create();

	const QRect rect = m_activeVideoCompositionViewWidget->geometry();

	geometry->SetInt("left", rect.x());
	geometry->SetInt("top", rect.y());
	geometry->SetInt("width", rect.width());
	geometry->SetInt("height", rect.height());

	root->SetDictionary("geometry", geometry);

	output->SetDictionary(root);
}

void StreamElementsBrowserWidget::DeserializeVideoCompositionView(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("videoCompositionId") ||
	    root->GetType("videoCompositionId") != VTYPE_STRING)
		return;

	if (!root->HasKey("geometry") ||
	    root->GetType("geometry") != VTYPE_DICTIONARY)
		return;

	std::string videoCompositionId = root->GetString("videoCompositionId");

	auto videoComposition = StreamElementsGlobalStateManager::GetInstance()
		->GetVideoCompositionManager()
		->GetVideoCompositionById(videoCompositionId);

	if (!videoComposition.get())
		return;

	auto geometry = root->GetDictionary("geometry");

	if (!geometry->HasKey("left") || geometry->GetType("left") != VTYPE_INT)
		return;
	if (!geometry->HasKey("top") || geometry->GetType("top") != VTYPE_INT)
		return;
	if (!geometry->HasKey("width") || geometry->GetType("width") != VTYPE_INT)
		return;
	if (!geometry->HasKey("height") || geometry->GetType("height") != VTYPE_INT)
		return;

	QRect rect(geometry->GetInt("left"), geometry->GetInt("top"),
		   geometry->GetInt("width"), geometry->GetInt("height"));

	if (rect.width() <= 1)
		return;

	if (rect.height() <= 1)
		return;

	SetVideoCompositionView(videoComposition, rect);

	SerializeVideoCompositionView(output);
}
