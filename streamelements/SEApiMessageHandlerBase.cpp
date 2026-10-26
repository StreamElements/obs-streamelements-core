#include "SEApiMessageHandlerBase.hpp"

#include "cef-headers.hpp"

#include "Version.hpp"
#include "SEUtils.hpp"
#include "SEUtils/SELogUtils.hpp"
#include "SEWebsocketApiServer.hpp"

/* Incoming messages from renderer process */
const char *MSG_ON_CONTEXT_CREATED =
	"CefRenderProcessHandler::OnContextCreated";
const char *MSG_INCOMING_API_CALL = "SEApiMessageHandler::OnIncomingApiCall";

/* Outgoing messages to renderer process */
const char *MSG_BIND_JAVASCRIPT_FUNCTIONS =
	"CefRenderProcessHandler::BindJavaScriptFunctions";
const char *MSG_BIND_JAVASCRIPT_PROPS =
	"CefRenderProcessHandler::BindJavaScriptProperties";

bool SEApiMessageHandlerBase::IsPluginInitialized()
{
	if (!obs_initialized())
		return false;

	auto services = m_services.lock();

	return services && services->ready;
}

bool SEApiMessageHandlerBase::OnProcessMessageReceived(
	std::shared_ptr<SEWebsocketApiServer::ClientInfo> source,
	CefRefPtr<CefProcessMessage> message, const long cefClientId)
{
	if (!IsPluginInitialized())
		return true;

	// This should prevent concurrent access to m_apiCallHandlers during initialization
	std::unique_lock processMessageReceivedLock(
		m_processMessageReceivedMutex);

	const std::string &name = message->GetName();

	if (name == MSG_ON_CONTEXT_CREATED) {
		RegisterIncomingApiCallHandlers();

		RegisterIncomingApiCallHandlersInternal(source);
		RegisterApiPropsInternal(source);
		DispatchHostReadyEventInternal(source);

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

			for (size_t i = headerSize; i < args->GetSize() - 1;
			     ++i) {
				CefRefPtr<CefValue> parsedValue = CefParseJSON(
					args->GetString(i),
					JSON_PARSER_ALLOW_TRAILING_COMMAS);

				if (!parsedValue) {
					parsedValue = CefValue::Create();
					parsedValue->SetNull();
				}

				callArgs->SetValue(callArgs->GetSize(),
						   parsedValue);
			}

			struct local_context {
				std::shared_ptr<SEApiMessageHandlerBase> self;
				std::string id;
				std::shared_ptr<SEWebsocketApiServer::ClientInfo>
					target;
				CefRefPtr<CefProcessMessage> message;
				CefRefPtr<CefListValue> callArgs;
				CefRefPtr<CefValue> result;
				std::function<void()> complete;
				int cef_app_callback_id;
				long cefClientId;
				std::shared_ptr<SEWebsocketApiServer::ClientInfo>
					source;

				std::shared_ptr<SEApiContextItem>
					apiContextHandle;
			};

			local_context *context = new local_context();

			context->self = this->Clone();
			context->id = id;
			context->target = source;
			context->message = message;
			context->callArgs = callArgs;
			context->result = result;
			context->cef_app_callback_id =
				message->GetArgumentList()->GetInt(
					message->GetArgumentList()->GetSize() -
					1);
			context->cefClientId = cefClientId;
			context->source = source;
			context->complete = [context]() {
				if (!context->self->IsPluginInitialized()) {
					SELog(SE_LOG_ERROR,
					      "obs-streamelements-core[%s %s]: API: plugin is no longer initialized while completing call to '%s', callback id %d",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id);

					RemoveApiContext(
						context->apiContextHandle);

					delete context;

					return;
				}

				if (IsTraceLogLevel()) {
					SELog(SE_LOG_INFO,
					      "obs-streamelements-core[%s %s]: API: completed call to '%s', callback id %d",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id);
				}

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

					//
					// Locked here too: a reply that
					// arrives after teardown has nothing
					// to send on. It must still release
					// the context, which the early return
					// this replaces skipped -- a leak
					// that goes from nearly unreachable
					// to routine once a lock can fail.
					//
					auto services =
						context->self->m_services.lock();

					auto apiServer =
						services
							? services->websocketApiServer
							: nullptr;

					if (apiServer) {
						apiServer->DispatchClientMessage(
							"system",
							context->source, msg);
					}
				}

				RemoveApiContext(context->apiContextHandle);

				delete context;
			};

			{
				CefRefPtr<CefValue> callArgsValue =
					CefValue::Create();
				callArgsValue->SetList(context->callArgs);
				if (IsTraceLogLevel()) {
					SELog(SE_LOG_INFO,
					      "obs-streamelements-core[%s %s]: API: posting call to '%s', callback id %d, args: %s",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id,
					      CefWriteJSON(callArgsValue,
							   JSON_WRITER_DEFAULT)
						      .ToString()
						      .c_str());
				}
			}

			QtPostTask([context]() -> void {
				if (!context->self->IsPluginInitialized()) {
					SELog(SE_LOG_ERROR,
					      "obs-streamelements-core[%s %s]: API: plugin is no longer initialized while performing call to '%s', callback id %d",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id);

					context->complete();
					return;
				}

				if (!context->self->m_runtimeStatus->m_running) {
					SELog(SE_LOG_ERROR,
					      "obs-streamelements-core[%s %s]: API: message handler no longer initialized while performing call to '%s', callback id %d",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id);

					context->complete();
					return;
				}

				// The crash consent prompt is modal, and a
				// modal loop keeps draining this queue --
				// so without this, calls posted before
				// the fault run inside the crash handler,
				// on the thread that crashed. See
				// IsCrashReportingInProgress().
				if (IsCrashReportingInProgress()) {
					SELog(SE_LOG_ERROR,
					      "obs-streamelements-core[%s %s]: API: a crash is being reported; refusing call to '%s', callback id %d",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id);

					context->complete();
					return;
				}

				if (IsTraceLogLevel()) {
					SELog(SE_LOG_INFO,
					      "obs-streamelements-core[%s %s]: API: performing call to '%s', callback id %d",
					      context->target->m_target.c_str(),
					      context->target->m_unique_id
						      .c_str(),
					      context->id.c_str(),
					      context->cef_app_callback_id);
				}

				//
				// The other dispatch site, and it
				// locks the bundle for the same
				// reason: with nothing for the
				// handler to call, complete the call
				// rather than run it.
				//
				auto apiServices =
					context->self->m_services.lock();

				if (!apiServices) {
					context->complete();
					return;
				}

				context->apiContextHandle = PushApiContext(
					context->id, context->callArgs);

				context->self->m_apiCallHandlers[context->id](
					context->self, apiServices,
					context->message, context->callArgs,
					context->result, context->target,
					context->cefClientId,
					context->complete);
			});
		}

		return true;
	}

	return false;
}

void SEApiMessageHandlerBase::InvokeApiCallHandlerAsync(
	CefRefPtr<CefProcessMessage> message,
	std::shared_ptr<SEWebsocketApiServer::ClientInfo> target,
	std::string invokeId, CefRefPtr<CefListValue> invokeArgs,
	std::function<void(CefRefPtr<CefValue>)> result_callback,
	const long cefClientId, const bool enable_logging)
{
	//
	// Locked once, here, for the whole call. If the bundle is gone there
	// is nothing for any handler to do, so the call is answered with null
	// before anything else happens: before the handler lookup, before
	// Clone(), and before the recursive API mutex is taken.
	//
	// This replaces both halves of the guard that used to stand here --
	// the null check on m_services and the IsInitialized() reach into the
	// singleton -- and it answers the caller instead of leaving the
	// page's promise pending forever.
	//
	auto apiServices = m_services.lock();

	if (!apiServices) {
		CefRefPtr<CefValue> nullResult = CefValue::Create();

		nullResult->SetNull();

		result_callback(nullResult);

		return;
	}

	auto runtimeStatus = m_runtimeStatus;

	if (!runtimeStatus->m_running)
		return;

	CefRefPtr<CefValue> result = CefValue::Create();
	result->SetNull();

	if (!m_apiCallHandlers.count(invokeId)) {
		SELog(SE_LOG_ERROR,
		      "obs-streamelements-core[%s %s]: API: invalid API call to '%s'",
		      target->m_target.c_str(), target->m_unique_id.c_str(),
		      invokeId.c_str());

		result_callback(result);

		return;
	}

	if (enable_logging) {
		SELog(SE_LOG_INFO,
		      "obs-streamelements-core[%s %s]: API: performing call to '%s'",
		      target->m_target.c_str(), target->m_unique_id.c_str(),
		      invokeId.c_str());
	}

	auto handler = m_apiCallHandlers[invokeId];

	handler(this->Clone(), apiServices, message, invokeArgs, result, target,
		cefClientId, [=]() {
			if (enable_logging) {
				SELog(SE_LOG_INFO,
				      "obs-streamelements-core[%s %s]: API: completed call to '%s'",
				      target->m_target.c_str(),
				      target->m_unique_id.c_str(),
				      invokeId.c_str());
			}

			if (runtimeStatus->m_running) {
				result_callback(result);
			}
		});
}

#if ENABLE_CREATE_BROWSER_API
CefRefPtr<CefDictionaryValue>
SEApiMessageHandlerBase::CreateBrowserArgsDictionary()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	CefRefPtr<CefDictionaryValue> seRoot = CefDictionaryValue::Create();

	seRoot->SetDictionary("api", CreateApiSpecDictionaryInternal());

	rootDictionary->SetDictionary("streamelements", seRoot);

	return rootDictionary;
}

CefRefPtr<CefDictionaryValue>
SEApiMessageHandlerBase::CreateApiSpecDictionaryInternal()
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
SEApiMessageHandlerBase::CreateApiCallHandlersDictionaryInternal()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	for (auto apiCallHandler : m_apiCallHandlers) {
		CefRefPtr<CefDictionaryValue> function =
			CefDictionaryValue::Create();

		function->SetString("message", MSG_INCOMING_API_CALL);

		rootDictionary->SetDictionary(apiCallHandler.first, function);
	}

	return rootDictionary;
}

void SEApiMessageHandlerBase::RegisterIncomingApiCallHandlersInternal(
	std::shared_ptr<SEWebsocketApiServer::ClientInfo> target)
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

	//
	// No availability check: this holds its own reference to the server,
	// so it does not care whether the global state manager is still
	// there. A handler built without services dispatches nothing, which
	// is the same thing it registers.
	//
	auto services = m_services.lock();
	auto apiServer = services ? services->websocketApiServer : nullptr;

	if (!apiServer)
		return;

	apiServer->DispatchClientMessage("system", target, msg);
}

CefRefPtr<CefDictionaryValue>
SEApiMessageHandlerBase::CreateApiPropsDictionaryInternal()
{
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	rootDictionary->SetBool("hostReady", true);
	rootDictionary->SetBool("hostContainerHidden", m_initialHiddenState);
	rootDictionary->SetInt("apiMajorVersion", HOST_API_VERSION_MAJOR);
	rootDictionary->SetInt("apiMinorVersion", HOST_API_VERSION_MINOR);

	return rootDictionary;
}

void SEApiMessageHandlerBase::RegisterApiPropsInternal(
	std::shared_ptr<SEWebsocketApiServer::ClientInfo> target)
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

	//
	// No availability check: this holds its own reference to the server,
	// so it does not care whether the global state manager is still
	// there. A handler built without services dispatches nothing, which
	// is the same thing it registers.
	//
	auto services = m_services.lock();
	auto apiServer = services ? services->websocketApiServer : nullptr;

	if (!apiServer)
		return;

	apiServer->DispatchClientMessage("system", target, msg);
}

void SEApiMessageHandlerBase::DispatchHostReadyEventInternal(
	std::shared_ptr<SEWebsocketApiServer::ClientInfo> target)
{
	DispatchEventInternal(target, "hostReady", "null");
}

void SEApiMessageHandlerBase::DispatchEventInternal(
	std::shared_ptr<SEWebsocketApiServer::ClientInfo> target,
	std::string event, std::string eventArgsJson)
{
	//
	// No availability check: this holds its own reference to the server,
	// so it does not care whether the global state manager is still
	// there. A handler built without services dispatches nothing, which
	// is the same thing it registers.
	//
	auto services = m_services.lock();
	auto apiServer = services ? services->websocketApiServer : nullptr;

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", target, event, eventArgsJson);
}

void SEApiMessageHandlerBase::RegisterIncomingApiCallHandler(
	std::string id, incoming_call_handler_t handler)
{
	m_apiCallHandlers[id] = handler;
}