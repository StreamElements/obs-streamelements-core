#include "StreamElementsApiMessageHandler.hpp"

#include "../cef-headers.hpp"

#include <include/cef_parser.h> // CefParseJSON, CefWriteJSON

#include "Version.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsMessageBus.hpp"
#include "StreamElementsPleaseWaitWindow.hpp"

#include <QDesktopServices>
#include <QUrl>

#include <codecvt>

std::shared_ptr<StreamElementsApiMessageHandler::InvokeHandler>
	StreamElementsApiMessageHandler::InvokeHandler::s_singleton = nullptr;

/* Incoming messages from renderer process */
const char *MSG_ON_CONTEXT_CREATED =
	"CefRenderProcessHandler::OnContextCreated";
const char *MSG_INCOMING_API_CALL =
	"StreamElementsApiMessageHandler::OnIncomingApiCall";

/* Outgoing messages to renderer process */
const char *MSG_BIND_JAVASCRIPT_FUNCTIONS =
	"CefRenderProcessHandler::BindJavaScriptFunctions";
const char *MSG_BIND_JAVASCRIPT_PROPS =
	"CefRenderProcessHandler::BindJavaScriptProperties";

bool StreamElementsApiMessageHandler::OnProcessMessageReceived(
	CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
	CefRefPtr<CefFrame> /*frame*/,
#endif
	CefProcessId /*source_process*/, CefRefPtr<CefProcessMessage> message,
	const long cefClientId)
{
	const std::string &name = message->GetName();

	if (name == MSG_ON_CONTEXT_CREATED) {
		RegisterIncomingApiCallHandlersInternal(browser);
		RegisterApiPropsInternal(browser);
		DispatchHostReadyEventInternal(browser);

		return true;
	} else if (name == MSG_INCOMING_API_CALL) {
		CefRefPtr<CefValue> result = CefValue::Create();
		result->SetBool(false);

		CefRefPtr<CefListValue> args = message->GetArgumentList();

		const int headerSize = args->GetInt(0);
		std::string id = args->GetString(2).ToString();

		id = id.substr(id.find_last_of('.') +
			       1); // window.host.XXX -> XXX

		if (m_apiCallHandlers.count(id)) {
			CefRefPtr<CefListValue> callArgs =
				CefListValue::Create();

			for (int i = headerSize; i < args->GetSize() - 1; ++i) {
				CefRefPtr<CefValue> parsedValue = CefParseJSON(
					args->GetString(i),
					JSON_PARSER_ALLOW_TRAILING_COMMAS);

				callArgs->SetValue(callArgs->GetSize(),
						   parsedValue);
			}

			struct local_context {
				StreamElementsApiMessageHandler *self;
				std::string id;
				CefRefPtr<CefBrowser> browser;
				CefRefPtr<CefProcessMessage> message;
				CefRefPtr<CefListValue> callArgs;
				CefRefPtr<CefValue> result;
				std::function<void()> complete;
				int cef_app_callback_id;
				long cefClientId;
			};

			local_context *context = new local_context();

			context->self = this;
			context->id = id;
			context->browser = browser;
			context->message = message;
			context->callArgs = callArgs;
			context->result = result;
			context->cef_app_callback_id =
				message->GetArgumentList()->GetInt(
					message->GetArgumentList()->GetSize() -
					1);
			context->cefClientId = cefClientId;
			context->complete = [context]() {
				blog(LOG_INFO,
				     "obs-browser[%lu]: API: completed call to '%s', callback id %d",
				     context->cefClientId, context->id.c_str(),
				     context->cef_app_callback_id);

				if (context->cef_app_callback_id != -1) {
					// Invoke result callback
					CefRefPtr<CefProcessMessage> msg =
						CefProcessMessage::Create(
							"executeCallback");

					CefRefPtr<CefListValue> callbackArgs =
						msg->GetArgumentList();
					callbackArgs->SetInt(
						0,
						context->cef_app_callback_id);
					callbackArgs->SetString(
						1,
						CefWriteJSON(
							context->result,
							JSON_WRITER_DEFAULT));

					SendBrowserProcessMessage(
						context->browser, PID_RENDERER,
						msg);
				}
			};

			{
				CefRefPtr<CefValue> callArgsValue =
					CefValue::Create();
				callArgsValue->SetList(context->callArgs);
				blog(LOG_INFO,
				     "obs-browser[%lu]: API: posting call to '%s', callback id %d, args: %s",
				     context->cefClientId, context->id.c_str(),
				     context->cef_app_callback_id,
				     CefWriteJSON(callArgsValue,
						  JSON_WRITER_DEFAULT)
					     .ToString()
					     .c_str());
			}

			QtPostTask(
				[context]() {
					blog(LOG_INFO,
					     "obs-browser[%lu]: API: performing call to '%s', callback id %d",
					     context->cefClientId,
					     context->id.c_str(),
					     context->cef_app_callback_id);

					context->self
						->m_apiCallHandlers[context->id](
							context->self,
							context->message,
							context->callArgs,
							context->result,
							context->browser,
							context->cefClientId,
							context->complete);

					delete context;
				});
		}

		return true;
	}

	return false;
}

void StreamElementsApiMessageHandler::InvokeApiCallHandlerAsync(
	CefRefPtr<CefProcessMessage> message, CefRefPtr<CefBrowser> browser,
	std::string invokeId, CefRefPtr<CefListValue> invokeArgs,
	std::function<void(CefRefPtr<CefValue>)> result_callback,
	const long cefClientId,
	const bool enable_logging)
{
	CefRefPtr<CefValue> result = CefValue::Create();
	result->SetNull();

	if (!m_apiCallHandlers.count(invokeId)) {
		blog(LOG_ERROR, "obs-browser[%lu]: API: invalid API call to '%s'",
		     cefClientId, invokeId.c_str());

		result_callback(result);

		return;
	}

	if (enable_logging) {
		blog(LOG_INFO, "obs-browser[%lu]: API: performing call to '%s'",
		     cefClientId, invokeId.c_str());
	}

	auto handler = m_apiCallHandlers[invokeId];

	handler(this, message, invokeArgs, result, browser, cefClientId, [=]() {
		if (enable_logging) {
			blog(LOG_INFO,
			     "obs-browser[%lu]: API: completed call to '%s'",
			     cefClientId, invokeId.c_str());
		}

		result_callback(result);
	});
}

#if ENABLE_CREATE_BROWSER_API
CefRefPtr<CefDictionaryValue>
StreamElementsApiMessageHandler::CreateBrowserArgsDictionary()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	CefRefPtr<CefDictionaryValue> seRoot = CefDictionaryValue::Create();

	seRoot->SetDictionary("api", CreateApiSpecDictionaryInternal());

	rootDictionary->SetDictionary("streamelements", seRoot);

	return rootDictionary;
}

CefRefPtr<CefDictionaryValue>
StreamElementsApiMessageHandler::CreateApiSpecDictionaryInternal()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	CefRefPtr<CefDictionaryValue> propsRoot = CefDictionaryValue::Create();

	propsRoot->SetString("container", "host");
	propsRoot->SetDictionary("items", CreateApiPropsDictionaryInternal());

	rootDictionary->SetDictionary("properties", propsRoot);

	CefRefPtr<CefDictionaryValue> funcsRoot = CefDictionaryValue::Create();

	funcsRoot->SetString("container", "host");
	funcsRoot->SetDictionary("items",
				 CreateApiCallHandlersDictionaryInternal());

	rootDictionary->SetDictionary("functions", funcsRoot);

	return rootDictionary;
}
#endif

CefRefPtr<CefDictionaryValue>
StreamElementsApiMessageHandler::CreateApiCallHandlersDictionaryInternal()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	for (auto apiCallHandler : m_apiCallHandlers) {
		CefRefPtr<CefValue> val = CefValue::Create();

		CefRefPtr<CefDictionaryValue> function =
			CefDictionaryValue::Create();

		function->SetString("message", MSG_INCOMING_API_CALL);

		val->SetDictionary(function);

		rootDictionary->SetValue(apiCallHandler.first, val);
	}

	return rootDictionary;
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlersInternal(
	CefRefPtr<CefBrowser> browser)
{
	// Context created, request creation of window.host object
	// with API methods
	CefRefPtr<CefValue> root = CefValue::Create();

	root->SetDictionary(CreateApiCallHandlersDictionaryInternal());

	// Convert data to JSON
	CefString jsonString = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	// Send request to renderer process
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create(MSG_BIND_JAVASCRIPT_FUNCTIONS);
	msg->GetArgumentList()->SetString(0, "host");
	msg->GetArgumentList()->SetString(1, jsonString);
	SendBrowserProcessMessage(browser, PID_RENDERER, msg);
}

CefRefPtr<CefDictionaryValue>
StreamElementsApiMessageHandler::CreateApiPropsDictionaryInternal()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	rootDictionary->SetBool("hostReady", true);
	rootDictionary->SetBool("hostContainerHidden", m_initialHiddenState);
	rootDictionary->SetInt("apiMajorVersion", HOST_API_VERSION_MAJOR);
	rootDictionary->SetInt("apiMinorVersion", HOST_API_VERSION_MINOR);

	return rootDictionary;
}

void StreamElementsApiMessageHandler::RegisterApiPropsInternal(
	CefRefPtr<CefBrowser> browser)
{
	// Context created, request creation of window.host object
	// with API methods
	CefRefPtr<CefValue> root = CefValue::Create();

	root->SetDictionary(CreateApiPropsDictionaryInternal());

	// Convert data to JSON
	CefString jsonString = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	// Send request to renderer process
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create(MSG_BIND_JAVASCRIPT_PROPS);
	msg->GetArgumentList()->SetString(0, "host");
	msg->GetArgumentList()->SetString(1, jsonString);
	SendBrowserProcessMessage(browser, PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::DispatchHostReadyEventInternal(
	CefRefPtr<CefBrowser> browser)
{
	DispatchEventInternal(browser, "hostReady", "null");
}

void StreamElementsApiMessageHandler::DispatchEventInternal(
	CefRefPtr<CefBrowser> browser, std::string event,
	std::string eventArgsJson)
{
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, eventArgsJson);
	SendBrowserProcessMessage(browser, PID_RENDERER, msg);
}

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandler(
	std::string id, incoming_call_handler_t handler)
{
	m_apiCallHandlers[id] = handler;
}

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name) RegisterIncomingApiCallHandler(name, [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result, CefRefPtr<CefBrowser> browser, const long cefClientId, std::function<void()> complete_callback) { std::lock_guard<std::recursive_mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END()                    \
	complete_callback(); \
	});

void StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	//RegisterIncomingApiCallHandler("getWidgets", [](StreamElementsApiMessageHandler*, CefRefPtr<CefProcessMessage> message, CefRefPtr<CefListValue> args, CefRefPtr<CefValue>& result) {
	//	result->SetBool(true);
	//});

	RegisterIncomingApiCallHandler(
		"batchInvokeSeries",
		[](StreamElementsApiMessageHandler *self,
		   CefRefPtr<CefProcessMessage> message,
		   CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,
		   CefRefPtr<CefBrowser> browser,
		   const long cefClientId,
		   std::function<void()> complete_callback) {
			result->SetNull();

			if (args->GetSize() < 1) {
				complete_callback();
				return;
			}

			if (args->GetType(0) != VTYPE_LIST) {
				complete_callback();
				return;
			}

			struct local_context {
				CefRefPtr<CefValue> queueIndex =
					CefValue::Create();
				CefRefPtr<CefListValue> queue =
					CefListValue::Create();
				CefRefPtr<CefListValue> results =
					CefListValue::Create();
				std::function<void()> process;
				std::function<void()> done;
			};

			local_context *context = new local_context();
			context->queueIndex->SetInt(0);
			context->queue = args->GetList(0);
			context->done = [=]() {
				obs_frontend_defer_save_end();

				result->SetList(context->results);
				complete_callback();
				delete context;
			};

			context->process = [=]() {
				size_t index = context->queueIndex->GetInt();

				if (index >= context->queue->GetSize()) {
					// End of queue
					context->done();
					return;
				}

				if (context->queue->GetType(index) !=
				    VTYPE_DICTIONARY) {
					// Invalid queue element type
					context->done();
					return;
				}

				CefRefPtr<CefDictionaryValue> d =
					context->queue->GetDictionary(index);

				if (!d->HasKey("invoke") ||
				    !d->HasKey("invokeArgs") ||
				    d->GetType("invoke") != VTYPE_STRING ||
				    d->GetType("invokeArgs") != VTYPE_LIST) {
					// Invalid queue element structure
					context->done();
					return;
				}

				std::string invokeId =
					d->GetString("invoke").ToString();

				CefRefPtr<CefListValue> invokeArgs =
					d->GetList("invokeArgs");

				self->InvokeApiCallHandlerAsync(
					message, browser, invokeId, invokeArgs,
					[=](CefRefPtr<CefValue> callResult) {
						context->results->SetValue(
							context->results
								->GetSize(),
							callResult);

						context->queueIndex->SetInt(
							context->queueIndex
								->GetInt() +
							1);

						context->process();
					},
					cefClientId);
			};

			obs_frontend_defer_save_begin();

			context->process();
		});

	API_HANDLER_BEGIN("getStartupFlags");
	{
		result->SetInt(
			StreamElementsConfig::GetInstance()->GetStartupFlags());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStartupFlags");
	{
		if (args->GetSize()) {
			StreamElementsConfig::GetInstance()->SetStartupFlags(
				args->GetValue(0)->GetInt());

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("deleteAllCookies");
	{
		StreamElementsGlobalStateManager::GetInstance()->DeleteCookies();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openDefaultBrowser");
	{
		if (args->GetSize()) {
			CefString url = args->GetValue(0)->GetString();

			QUrl navigate_url = QUrl(url.ToString().c_str(),
						 QUrl::TolerantMode);
			QDesktopServices::openUrl(navigate_url);

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showNotificationBar");
	{
		if (args->GetSize()) {
			CefRefPtr<CefValue> barInfo = args->GetValue(0);

			StreamElementsGlobalStateManager::GetInstance()
				->GetWidgetManager()
				->DeserializeNotificationBar(barInfo);

			StreamElementsGlobalStateManager::GetInstance()
				->PersistState();

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideNotificationBar");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetWidgetManager()
			->HideNotificationBar();

		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showCentralWidget");
	{
		if (args->GetSize()) {
			CefRefPtr<CefDictionaryValue> rootDictionary =
				args->GetValue(0)->GetDictionary();

			if (rootDictionary.get() &&
			    rootDictionary->HasKey("url")) {
				// Remove all central widgets
				while (StreamElementsGlobalStateManager::GetInstance()
					       ->GetWidgetManager()
					       ->DestroyCurrentCentralBrowserWidget()) {
				}

				std::string executeJavaScriptCodeOnLoad;

				if (rootDictionary->HasKey(
					    "executeJavaScriptCodeOnLoad")) {
					executeJavaScriptCodeOnLoad =
						rootDictionary
							->GetString(
								"executeJavaScriptCodeOnLoad")
							.ToString();
				}

				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->PushCentralBrowserWidget(
						rootDictionary->GetString("url")
							.ToString()
							.c_str(),
						executeJavaScriptCodeOnLoad
							.c_str());

				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideCentralWidget");
	{
		while (StreamElementsGlobalStateManager::GetInstance()
			       ->GetWidgetManager()
			       ->DestroyCurrentCentralBrowserWidget()) {
		}

		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addDockingWidget");
	{
		if (args->GetSize()) {
			CefRefPtr<CefValue> widgetInfo = args->GetValue(0);

			std::string id =
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->AddDockBrowserWidget(widgetInfo);

			QDockWidget *dock =
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->GetDockWidget(id.c_str());

			if (dock) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();

				result->SetString(id);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeDockingWidgetsByIds");
	{
		if (args->GetSize()) {
			CefRefPtr<CefListValue> list = args->GetList(0);

			if (list.get()) {
				for (int i = 0; i < list->GetSize(); ++i) {
					CefString id = list->GetString(i);

					StreamElementsGlobalStateManager::
						GetInstance()
							->GetWidgetManager()
							->RemoveDockWidget(
								id.ToString()
									.c_str());
				}

				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllDockingWidgets");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetWidgetManager()
			->SerializeDockingWidgets(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("toggleDockingWidgetFloatingById");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->ToggleWidgetFloatingStateById(
						args->GetString(0)
							.ToString()
							.c_str()));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetDimensionsById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_DICTIONARY) {
			int width = -1;
			int height = -1;

			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(1);

			if (d->HasKey("width") &&
			    d->GetType("width") == VTYPE_INT) {
				width = d->GetInt("width");
			}

			if (d->HasKey("height") &&
			    d->GetType("height") == VTYPE_INT) {
				height = d->GetInt("height");
			}

			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->SetWidgetDimensionsById(
						args->GetString(0)
							.ToString()
							.c_str(),
						width, height));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetPositionById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_DICTIONARY) {
			int left = -1;
			int top = -1;

			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(1);

			if (d->HasKey("left") &&
			    d->GetType("left") == VTYPE_INT) {
				left = d->GetInt("left");
			}

			if (d->HasKey("top") &&
			    d->GetType("top") == VTYPE_INT) {
				top = d->GetInt("top");
			}

			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->SetWidgetPositionById(
						args->GetString(0)
							.ToString()
							.c_str(),
						left, top));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetUrlById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_STRING) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->SetWidgetUrlById(args->GetString(0)
								   .ToString()
								   .c_str(),
							   args->GetString(1)
								   .ToString()
								   .c_str()));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showDockingWidgetById");
	{
		if (args->GetSize() >= 1 && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->ShowDockWidgetById(args->GetString(0)
								     .ToString()
								     .c_str()));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideDockingWidgetById");
	{
		if (args->GetSize() >= 1 && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->HideDockWidgetById(args->GetString(0)
								     .ToString()
								     .c_str()));

			if (result->GetBool()) {
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
				StreamElementsGlobalStateManager::GetInstance()
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("beginOnBoarding");
	{
		StreamElementsGlobalStateManager::GetInstance()->Reset();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("completeOnBoarding");
	{
		while (StreamElementsGlobalStateManager::GetInstance()
			       ->GetWidgetManager()
			       ->DestroyCurrentCentralBrowserWidget()) {
		}

		StreamElementsConfig::GetInstance()->SetStartupFlags(
			StreamElementsConfig::GetInstance()->GetStartupFlags() &
			~StreamElementsConfig::STARTUP_FLAGS_ONBOARDING_MODE);

		//
		// Once on-boarding is complete, we assume our state is signed-in.
		//
		// This is not enough. There are edge cases where the user is signed-in
		// but not yet has completed on-boarding.
		//
		// For those cases, we provide the adviseSignedIn() API call.
		//
		StreamElementsConfig::GetInstance()->SetStartupFlags(
			StreamElementsConfig::GetInstance()->GetStartupFlags() |
			StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showStatusBarTemporaryMessage");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->DeserializeStatusBarTemporaryMessage(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestBegin");
	{
		if (args->GetSize() >= 2) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetBandwidthTestManager()
					->BeginBandwidthTest(args->GetValue(0),
							     args->GetValue(1),
							     browser));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestEnd");
	{
		CefRefPtr<CefValue> options;

		if (args->GetSize()) {
			options = args->GetValue(0);
		}
		result->SetDictionary(
			StreamElementsGlobalStateManager::GetInstance()
				->GetBandwidthTestManager()
				->EndBandwidthTest(options));
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestGetStatus");
	{
		result->SetDictionary(
			StreamElementsGlobalStateManager::GetInstance()
				->GetBandwidthTestManager()
				->GetBandwidthTestStatus());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableEncoders");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetOutputSettingsManager()
			->GetAvailableEncoders(result, nullptr);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableVideoEncoders");
	{
		obs_encoder_type type = OBS_ENCODER_VIDEO;

		StreamElementsGlobalStateManager::GetInstance()
			->GetOutputSettingsManager()
			->GetAvailableEncoders(result, &type);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableAudioEncoders");
	{
		obs_encoder_type type = OBS_ENCODER_AUDIO;

		StreamElementsGlobalStateManager::GetInstance()
			->GetOutputSettingsManager()
			->GetAvailableEncoders(result, &type);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStreamingSettings");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetOutputSettingsManager()
					->SetStreamingSettings(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getEncodingSettings");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetOutputSettingsManager()
			->GetEncodingSettings(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setEncodingSettings");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetOutputSettingsManager()
					->SetEncodingSettings(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentContainerProperties");
	{
		CefRefPtr<StreamElementsCefClient> client =
			static_cast<StreamElementsCefClient *>(
				browser->GetHost()->GetClient().get());

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		result->SetDictionary(d);

		std::string dockingArea = "none";
		std::string id = client->GetContainerId();

		if (id.size()) {
			StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo
				*info = StreamElementsGlobalStateManager::
						GetInstance()
							->GetWidgetManager()
							->GetDockBrowserWidgetInfo(
								id.c_str());

			if (info) {
				dockingArea = info->m_dockingArea;

				delete info;
			}
		}

		d->SetString("id", id.c_str());
		d->SetString("dockingArea", dockingArea.c_str());
		d->SetString("url",
			     browser->GetMainFrame()->GetURL().ToString());
		d->SetString("theme", GetCurrentThemeName());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openPopupWindow");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->DeserializePopupWindow(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addBackgroundWorker");
	{
		if (args->GetSize()) {
			result->SetString(
				StreamElementsGlobalStateManager::GetInstance()
					->GetWorkerManager()
					->DeserializeOne(args->GetValue(0)));

			StreamElementsGlobalStateManager::GetInstance()
				->PersistState(false);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllBackgroundWorkers");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetWorkerManager()
			->Serialize(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeBackgroundWorkersByIds");
	{
		if (args->GetSize()) {
			CefRefPtr<CefListValue> list = args->GetList(0);

			if (list.get()) {
				for (int i = 0; i < list->GetSize(); ++i) {
					CefString id = list->GetString(i);

					StreamElementsGlobalStateManager::
						GetInstance()
							->GetWorkerManager()
							->Remove(id);
				}

				StreamElementsGlobalStateManager::GetInstance()
					->PersistState(false);

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showModalDialog");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->DeserializeModalDialog(args->GetValue(0),
							 result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSystemCPUUsageTimes");
	{
		SerializeSystemTimes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSystemMemoryUsage");
	{
		SerializeSystemMemoryUsage(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSystemHardwareProperties");
	{
		SerializeSystemHardwareProperties(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableInputSourceTypes");
	{
		SerializeAvailableInputSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllHotkeyBindings");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetHotkeyManager()
			->SerializeHotkeyBindings(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllManagedHotkeyBindings");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetHotkeyManager()
			->SerializeHotkeyBindings(result, true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addHotkeyBinding");
	{
		if (args->GetSize()) {
			obs_hotkey_id id =
				StreamElementsGlobalStateManager::GetInstance()
					->GetHotkeyManager()
					->DeserializeSingleHotkeyBinding(
						args->GetValue(0));

			if (id != OBS_INVALID_HOTKEY_ID) {
				result->SetInt((int)id);
			} else {
				result->SetNull();
			}
		} else
			result->SetNull();
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeHotkeyBindingById");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetHotkeyManager()
					->RemoveHotkeyBindingById(
						args->GetInt(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getHostProperties");
	{
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("obsVersionString", obs_get_version_string());
		d->SetString("cefVersionString", GetCefVersionString());
		d->SetString("cefPlatformApiHash", GetCefPlatformApiHash());
		d->SetString("cefUniversalApiHash", GetCefUniversalApiHash());
		d->SetString("hostPluginVersionString",
			     GetStreamElementsPluginVersionString());
		d->SetString("hostApiVersionString",
			     GetStreamElementsApiVersionString());
		d->SetString("hostMachineUniqueId",
			     StreamElementsGlobalStateManager::GetInstance()
				     ->GetAnalyticsEventsManager()
				     ->identity());
		d->SetString("hostSessionUniqueId",
			     StreamElementsGlobalStateManager::GetInstance()
				     ->GetAnalyticsEventsManager()
				     ->sessionId());

#ifdef WIN32
		d->SetString("platform", "windows");
#elif defined(__APPLE__)
		d->SetString("platform", "macos");
#elif defined(__linux__)
		d->SetString("platform", "linux");
#endif

		if (sizeof(void*) == 8) {
			d->SetString("platformArch", "64bit");
		} else {
			d->SetString("platformArch", "32bit");
		}

		result->SetDictionary(d);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseSignedIn");
	{
		StreamElementsConfig *config =
			StreamElementsConfig::GetInstance();

		config->SetStartupFlags(
			config->GetStartupFlags() |
			StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState(
			false);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseSignedOut");
	{
		StreamElementsConfig *config =
			StreamElementsConfig::GetInstance();

		config->SetStartupFlags(
			config->GetStartupFlags() &
			~StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->Update();
		StreamElementsGlobalStateManager::GetInstance()->PersistState(
			false);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("broadcastMessage");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_DICTIONARY) {
			CefRefPtr<CefValue> message = args->GetValue(0);

			StreamElementsMessageBus::GetInstance()
				->NotifyAllMessageListeners(
					StreamElementsMessageBus::DEST_ALL_LOCAL,
					StreamElementsMessageBus::SOURCE_WEB,
					browser ? browser->GetMainFrame()
							  ->GetURL()
							  .ToString()
						: "urn:streamelements:internal",
					message);

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("broadcastEvent");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(0);

			if (d->HasKey("name") &&
			    d->GetType("name") == VTYPE_STRING) {
				std::string sourceUrl =
					browser ? browser->GetMainFrame()
							  ->GetURL()
							  .ToString()
						: "urn:streamelements:internal";

				StreamElementsMessageBus::GetInstance()
					->NotifyAllLocalEventListeners(
						StreamElementsMessageBus::
								DEST_ALL_LOCAL &
							~StreamElementsMessageBus::
								DEST_BROWSER_SOURCE,
						StreamElementsMessageBus::
							SOURCE_WEB,
						sourceUrl, "hostEventReceived",
						args->GetValue(0));

				StreamElementsMessageBus::GetInstance()
					->NotifyAllExternalEventListeners(
						StreamElementsMessageBus::
							DEST_ALL_EXTERNAL,
						StreamElementsMessageBus::
							SOURCE_WEB,
						sourceUrl, d->GetString("name"),
						d->GetValue("data"));

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemBrowserSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsBrowserSource(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemBrowserSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsBrowserSource(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemGameCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsGameCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemGameCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsGameCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemVideoCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsVideoCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemVideoCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsVideoCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemObsNativeSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsNativeSource(args->GetValue(0),
							     result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemObsNativeSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsNativeSource(args->GetValue(0),
							     result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemGroup");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsSceneItemGroup(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemGroup");
	{
		result->SetNull();

		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsSceneItemGroup(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllCurrentSceneItems");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SerializeObsSceneItems(args->GetValue(0),
							 result);
		} else {
			CefRefPtr<CefValue> nullArg = CefValue::Create();

			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SerializeObsSceneItems(nullArg, result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllSceneItems");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SerializeObsSceneItems(args->GetValue(0),
							 result);
		} else {
			CefRefPtr<CefValue> nullArg = CefValue::Create();

			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SerializeObsSceneItems(nullArg, result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeCurrentSceneItemsByIds");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->RemoveObsSceneItemsByIds(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeSceneItemsByIds");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->RemoveObsSceneItemsByIds(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("invokeCurrentSceneItemDefaultActionById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->InvokeCurrentSceneItemDefaultActionById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("invokeCurrentSceneItemDefaultContextMenuById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->InvokeCurrentSceneItemDefaultContextMenuById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneItemsAuxiliaryActions");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeSceneItemsAuxiliaryActions(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentSceneItemsAuxiliaryActions");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeSceneItemsAuxiliaryActions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setScenesAuxiliaryActions");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeScenesAuxiliaryActions(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getScenesAuxiliaryActions");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeScenesAuxiliaryActions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneItemPropertiesById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SetObsSceneItemPropertiesById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setSceneItemPropertiesById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SetObsSceneItemPropertiesById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("ungroupCurrentSceneItemGroupById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->UngroupObsSceneItemsByGroupId(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("ungroupSceneItemGroupById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->UngroupObsSceneItemsByGroupId(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableInputSourceClasses");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeInputSourceClasses(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSourceClassProperties");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SerializeSourceClassProperties(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getHostReleaseGroupProperties");
	{
		std::string quality =
			ReadProductEnvironmentConfigurationString("Quality");
		std::string manifestUrl =
			ReadProductEnvironmentConfigurationString(
				"ManifestUrl");

		if (quality.size() && manifestUrl.size()) {
			CefRefPtr<CefDictionaryValue> d =
				CefDictionaryValue::Create();

			d->SetString("quality", quality.c_str());
			d->SetString("manifestUrl", manifestUrl.c_str());

			result->SetDictionary(d);
		} else {
			result->SetNull();
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setHostReleaseGroupProperties");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(0);

			if (d->HasKey("quality") &&
			    d->GetType("quality") == VTYPE_STRING &&
			    d->HasKey("manifestUrl") &&
			    d->GetType("manifestUrl") == VTYPE_STRING) {
				std::string quality = d->GetString("quality");
				std::string manifestUrl =
					d->GetString("manifestUrl");

				bool writeResult =
					WriteProductEnvironmentConfigurationStrings(
						{{"", "Quality", quality},
						 {"", "ManifestUrl",
						  manifestUrl}});

				result->SetBool(writeResult);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("queryHostReleaseGroupUpdateAvailability");
	{
		bool allowDowngrade = false;
		bool forceInstall = false;
		bool allowUseLastResponse = true;

		if (args->GetSize() > 0) {
			if (args->GetValue(0)->GetType() == VTYPE_DICTIONARY) {
				CefRefPtr<CefDictionaryValue> d =
					args->GetDictionary(0);

				if (d->HasKey("allowDowngrade") &&
				    d->GetType("allowDowngrade") ==
					    VTYPE_BOOL) {
					allowDowngrade =
						d->GetBool("allowDowngrade");
				}

				if (d->HasKey("forceInstall") &&
				    d->GetType("forceInstall") ==
					    VTYPE_BOOL) {
					forceInstall =
						d->GetBool("forceInstall");
				}

				if (d->HasKey("allowUseLastResponse") &&
				    d->GetType("allowUseLastResponse") == VTYPE_BOOL) {
					allowUseLastResponse =
						d->GetBool("allowUseLastResponse");
				}
			}
		}

		calldata_t *cd = calldata_create();
		calldata_set_bool(cd, "allow_downgrade", allowDowngrade);
		calldata_set_bool(cd, "force_install", forceInstall);
		calldata_set_bool(cd, "allow_use_last_response",
				  allowUseLastResponse);

		signal_handler_signal(
			obs_get_signal_handler(),
			"streamelements_request_check_for_updates_silent",
			cd);

		calldata_free(cd);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setContainerForeignPopupWindowsProperties");
	{
		if (args->GetSize()) {
			CefRefPtr<StreamElementsCefClient> client =
				static_cast<StreamElementsCefClient *>(
					browser->GetHost()->GetClient().get());

			if (!!client.get()) {
				result->SetBool(
					client->DeserializeForeignPopupWindowsSettings(
						args->GetValue(0)));
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getContainerForeignPopupWindowsProperties");
	{
		CefRefPtr<StreamElementsCefClient> client =
			static_cast<StreamElementsCefClient *>(
				browser->GetHost()->GetClient().get());

		if (!!client.get()) {
			client->SerializeForeignPopupWindowsSettings(result);
		} else {
			result->SetNull();
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getExternalSceneDataProviders");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetExternalSceneDataProviderManager()
			->SerializeProviders(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getExternalSceneDataSceneCollections");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetExternalSceneDataProviderManager()
				->SerializeProviderSceneCollections(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getExternalSceneDataSceneCollectionContent");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetExternalSceneDataProviderManager()
				->SerializeProviderSceneColletion(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("httpRequestText");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetHttpClient()
				->DeserializeHttpRequestText(args->GetValue(0),
							     result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getStreamingStatus");
	{
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetBool("isStreamingActive",
			   obs_frontend_streaming_active());

		result->SetDictionary(d);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("requestStreamingStart");
	{
		obs_frontend_streaming_start();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("requestStreamingStop");
	{
		obs_frontend_streaming_stop();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStreamingStartUIHandlerProperties");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetNativeOBSControlsManager()
					->DeserializeStartStreamingUIHandlerProperties(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseStreamingStartUIRequestAccepted");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetNativeOBSControlsManager()
			->AdviseRequestStartStreamingAccepted();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseStreamingStartUIRequestRejected");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetNativeOBSControlsManager()
			->AdviseRequestStartStreamingRejected();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getUserInterfaceState");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->SerializeUserInterfaceState(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setUserInterfaceState");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->DeserializeUserInterfaceState(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllScenes");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeObsScenes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentScene");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeObsCurrentScene(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addScene");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsScene(args->GetValue(0),
						      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SetCurrentObsSceneById(args->GetValue(0),
							 result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeScenesByIds");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->RemoveObsScenesByIds(args->GetValue(0),
						       result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setScenePropertiesById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->SetObsScenePropertiesById(args->GetValue(0),
							    result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setAuxiliaryMenuItems");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->DeserializeAuxiliaryMenuItems(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAuxiliaryMenuItems");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetMenuManager()
			->SerializeAuxiliaryMenuItems(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllProfiles");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetProfilesManager()
			->SerializeAllProfiles(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentProfile");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetProfilesManager()
			->SerializeCurrentProfile(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentProfile");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetProfilesManager()
					->DeserializeCurrentProfileById(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentProfile");
	{
		if (args->GetSize()) {
			result->SetBool(
				StreamElementsGlobalStateManager::GetInstance()
					->GetProfilesManager()
					->DeserializeCurrentProfileById(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllSceneCollections");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeObsSceneCollections(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentSceneCollectionProperties");
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetObsSceneManager()
			->SerializeObsCurrentSceneCollection(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneCollection");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsSceneCollection(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneCollectionById");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetObsSceneManager()
				->DeserializeObsCurrentSceneCollectionById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("createUserEnvironmentBackupPackage");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetBackupManager()
				->CreateLocalBackupPackage(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("queryUserEnvironmentBackupPackageContent");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetBackupManager()
				->QueryBackupPackageContent(args->GetValue(0),
							    result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("restoreUserEnvironmentBackupPackageContent");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetBackupManager()
				->RestoreBackupPackageContent(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("reloadAllBrowserSources");
	{
		extern void ReloadAllBrowserSources();

		ReloadAllBrowserSources();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("beginDeferSaveTransaction");
	{
		result->SetString(CreateTimedObsApiTransaction());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("completeDeferSaveTransaction");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_STRING) {
			CompleteTimedObsApiTransaction(args->GetString(0));

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllCookies");
	{
		if (args->GetSize()) {
			StreamElementsGlobalStateManager::GetInstance()
				->SerializeCookies(args->GetValue(0), result);
		} else {
			StreamElementsGlobalStateManager::GetInstance()
				->SerializeCookies(CefValue::Create(), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("dispatchKeyboardEvent");
	{
		if (args->GetSize()) {
			CefKeyEvent keyEvent;
			if (DeserializeCefKeyEvent(args->GetValue(0),
						   keyEvent)) {
				browser->GetHost()->SendKeyEvent(keyEvent);

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("dispatchMouseEvent");
	{
		if (args->GetSize()) {
			CefMouseEvent mouseEvent;
			CefMouseEventType mouseEventType;

			bool deserializeResult = true;

			deserializeResult &= DeserializeCefMouseEventType(
				args->GetValue(0), mouseEventType);

			deserializeResult &= DeserializeCefMouseEvent(
				args->GetValue(0), mouseEvent);

			if (deserializeResult) {
				switch (mouseEventType) {
				case Down:
				case Up:
					CefBrowserHost::MouseButtonType mouseButton;

					if (DeserializeCefMouseButtonType(
						    args->GetValue(0), mouseButton)) {
						int mouseEventCount =
							DeserializeCefMouseEventCount(
								args->GetValue(0),
								mouseEventCount);

						browser->GetHost()->SendMouseClickEvent(
							mouseEvent, mouseButton,
							mouseEventType == Up,
							mouseEventCount);

						result->SetBool(true);
					}
					break;
				case Move:
					browser->GetHost()->SendMouseMoveEvent(
						mouseEvent, false);

					result->SetBool(true);
					break;
				case Wheel:
					CefMouseWheelEventArgs
						mouseWheelEventArgs;
					if (DeserializeCefMouseWheelEventArgs(
						    args->GetValue(0),
						    mouseWheelEventArgs)) {
						browser->GetHost()
							->SendMouseWheelEvent(
								mouseEvent,
								mouseWheelEventArgs
									.deltaX,
								mouseWheelEventArgs
									.deltaY);

						result->SetBool(true);
					}
					break;
				}
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addBrowserScopedHttpServer");
	{
		if (args->GetSize()) {
			StreamElementsMessageBus::GetInstance()
				->DeserializeBrowserHttpServer(
					browser, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllBrowserScopedHttpServers");
	{
		StreamElementsMessageBus::GetInstance()
			->SerializeBrowserHttpServers(browser, result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeBrowserScopedHttpServersByIds");
	{
		if (args->GetSize()) {
			StreamElementsMessageBus::GetInstance()
				->RemoveBrowserHttpServersByIds(
					browser, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("sendHttpRequestResponse");
	{
		if (args->GetSize() >= 2) {
			StreamElementsMessageBus::GetInstance()
				->DeserializeHttpRequestResponse(
					args->GetValue(0), args->GetValue(1),
					result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setShowBuiltInMenuItems");
	{
		if (args->GetSize() >= 1 && args->GetType(0) == VTYPE_BOOL) {
			StreamElementsGlobalStateManager::GetInstance()
				->GetMenuManager()
				->SetShowBuiltInMenuItems(args->GetBool(0));

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getShowBuiltInMenuItems");
	{
		result->SetBool(StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()
					->GetShowBuiltInMenuItems());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("crashProgram");
	{
		// Crash
		*((int *)0x133713337) = 12345; // exception
        
		UNUSED_PARAMETER(result);
	}
	API_HANDLER_END();
}

bool StreamElementsApiMessageHandler::InvokeHandler::InvokeApiCallAsync(
	std::string invoke, CefRefPtr<CefListValue> args,
	std::function<void(CefRefPtr<CefValue>)> callback)
{
	if (!m_apiCallHandlers.count(invoke))
		return false;

	blog(LOG_INFO,
	     "obs-browser: StreamElementsApiMessageHandler::InvokeHandler::InvokeApiCallAsync: '%s', [%d]",
	     invoke.c_str(), args->GetSize());

	incoming_call_handler_t handler = m_apiCallHandlers[invoke];

	struct local_context {
		CefRefPtr<CefValue> result;
		std::function<void(CefRefPtr<CefValue>)> callback;
	};

	local_context *context = new local_context();

	context->result = CefValue::Create();
	context->result->SetBool(false);
	context->callback = callback;

	handler(this, nullptr, args, context->result, nullptr, -1, [context]() {
		context->callback(context->result);

		delete context;
	});

	return true;
}
