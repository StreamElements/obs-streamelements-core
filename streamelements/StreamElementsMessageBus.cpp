#include "StreamElementsMessageBus.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"

std::shared_ptr<StreamElementsMessageBus> StreamElementsMessageBus::s_instance = nullptr;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL = 0xFFFFFFFFUL;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL_LOCAL = 0x0000FFFFUL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_UI = 0x00000001UL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_WORKER = 0x00000002UL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_BROWSER_SOURCE = 0x00000004UL;

const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_ALL_EXTERNAL = 0xFFFF0000UL;
const StreamElementsMessageBus::message_destination_filter_flags_t StreamElementsMessageBus::DEST_EXTERNAL_CONTROLLER = 0x00010000UL;

const char* const StreamElementsMessageBus::SOURCE_APPLICATION = "application";
const char* const StreamElementsMessageBus::SOURCE_WEB = "web";
const char* const StreamElementsMessageBus::SOURCE_EXTERNAL = "external";

StreamElementsMessageBus::StreamElementsMessageBus(Private) :
	m_external_controller_server(this)
{

}

StreamElementsMessageBus::~StreamElementsMessageBus()
{

}

std::shared_ptr<StreamElementsMessageBus> StreamElementsMessageBus::GetInstance()
{
	if (!s_instance) {
		s_instance = std::make_shared<StreamElementsMessageBus>(Private());
	}

	return s_instance;
}

void StreamElementsMessageBus::Destroy()
{
	s_instance = nullptr;
}

void StreamElementsMessageBus::AddListener(std::string target, message_destination_filter_flags_t type)
{
	std::lock_guard<std::recursive_mutex> guard(m_listener_list_mutex);

	m_listener_list[target] = std::make_shared<ListenerItem>(target, type);

	auto httpRequestHandler = [this, target](const HttpServer::request_t &req,
					 HttpServer::response_t &res) -> void {

		CefRefPtr<CefValue> root = CefValue::Create();

		CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

		std::string requestId = CreateGloballyUniqueIdString();

		// ID
		rootDict->SetString("id", requestId.c_str());

		// Method, path
		rootDict->SetString("method", req.method.c_str());
		rootDict->SetString("url", req.path.c_str());

		// Body
		rootDict->SetString("body", req.body.c_str());

		// Headers
		CefRefPtr<CefDictionaryValue> headers =
			CefDictionaryValue::Create();

		for (auto kv : req.headers) {
			headers->SetString(kv.first, kv.second);
		}

		rootDict->SetDictionary("headers", headers);

		// Query string params
		CefRefPtr<CefDictionaryValue> query =
			CefDictionaryValue::Create();

		for (auto kv : req.params) {
			query->SetString(kv.first, kv.second);
		}

		rootDict->SetDictionary("query", query);

		root->SetDictionary(rootDict);

		auto requestState = std::make_shared<WaitingHttpRequestState>(&req,
									      &res);
		
		{
			std::lock_guard<decltype(m_waiting_http_requests_mutex)>
				guard(m_waiting_http_requests_mutex);

			m_waiting_http_requests[requestId] = requestState;
				
		}

		NotifyEventListener(target, "browser", "http",
				    "urn:http:server:browser",
				    "hostMessageReceived", root);

		bool requestHandled = false;
		
		for (int i = 0; i < 15000 / 10 && !requestHandled; ++i) {
			if (requestState->future.wait_for(std::chrono::milliseconds(10)) != std::future_status::timeout)
			{
				requestHandled = true;
			} else {
				QApplication::processEvents();
			}
		}
		
		if (!requestHandled) {
			// Nobody handled the request
			res.status = 404;
			res.reason = "Request Not Handled";
		}

		{
			std::lock_guard<decltype(m_waiting_http_requests_mutex)>
				guard(m_waiting_http_requests_mutex);

			m_waiting_http_requests.erase(requestId);
		}
	};

	m_listener_http_servers[target] =
		std::make_shared<StreamElementsHttpServerManager>(
			httpRequestHandler);
}

void StreamElementsMessageBus::RemoveListener(std::string target)
{
	std::lock_guard<std::recursive_mutex> guard(m_listener_list_mutex);

	m_listener_list.erase(target);

	m_listener_http_servers.erase(target);
}

void StreamElementsMessageBus::DeserializeHttpRequestResponse(
	CefRefPtr<CefValue> idInput, CefRefPtr<CefValue> responseInput,
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> browser_list_guard(m_listener_list_mutex);

	std::lock_guard<decltype(m_waiting_http_requests_mutex)> waiting_http_requests_guard(
		m_waiting_http_requests_mutex);

	output->SetBool(false);

	if (idInput->GetType() != VTYPE_STRING)
		return;

	std::string requestId = idInput->GetString();

	if (!m_waiting_http_requests.count(requestId))
		return;

	if (responseInput->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = responseInput->GetDictionary();

	auto res = m_waiting_http_requests[requestId]->response;

	res->status = 200;
	res->reason = "OK";
	res->body = "";

	if (d->HasKey("statusCode") && d->GetType("statusCode") == VTYPE_INT) {
		res->status = d->GetInt("statusCode");
	}

	if (d->HasKey("statusText") &&
	    d->GetType("statusText") == VTYPE_STRING) {
		res->reason = d->GetString("statusText");
	}

	if (d->HasKey("body")) {
		if (d->GetType("body") == VTYPE_STRING) {
			res->set_content(d->GetString("body").ToString(),
					 "text/plain");
		} else {
			res->set_content(CefWriteJSON(d->GetValue("body"),
						      JSON_WRITER_DEFAULT)
						 .ToString(),
					 "application/json");
		}
	}

	if (d->HasKey("headers") && d->GetType("headers") == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> headers =
			d->GetDictionary("headers");

		CefDictionaryValue::KeyList keys;
		if (headers->GetKeys(keys)) {
			for (auto key : keys) {
				if (headers->GetType(key) == VTYPE_STRING) {
					std::string val =
						headers->GetString(key)
							.ToString();

					res->set_header(key.ToString().c_str(), val.c_str());
				}
			}
		}
	}

	m_waiting_http_requests[requestId]->promise.set_value();

	output->SetBool(true);
}

void StreamElementsMessageBus::DeserializeBrowserHttpServer(
	std::string target, CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_listener_list_mutex);

	output->SetNull();

	if (!m_listener_http_servers.count(target))
		return;

	m_listener_http_servers[target]->DeserializeHttpServer(
		input, output);
}

void StreamElementsMessageBus::SerializeBrowserHttpServers(
	std::string target, CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_listener_list_mutex);

	output->SetNull();

	if (!m_listener_http_servers.count(target))
		return;

	m_listener_http_servers[target]->SerializeHttpServers(
		output);
}

void StreamElementsMessageBus::RemoveBrowserHttpServersByIds(
	std::string target, CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<std::recursive_mutex> guard(m_listener_list_mutex);

	output->SetNull();

	if (!m_listener_http_servers.count(target))
		return;

	m_listener_http_servers[target]->RemoveHttpServersByIds(
		input, output);
}

void StreamElementsMessageBus::NotifyEventListener(std::string target,
						   std::string scope,
						   std::string source,
	std::string sourceAddress, std::string event,
	CefRefPtr<CefValue> payload)
{
	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

	rootDict->SetString("scope", scope);
	rootDict->SetString("source", source);
	rootDict->SetString("sourceAddress", sourceAddress);
	rootDict->SetValue("message", payload->Copy());

	root->SetDictionary(rootDict);

	std::string payloadJson = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	DispatchJSEventContainer(target, event, payloadJson);
}

void StreamElementsMessageBus::NotifyAllLocalEventListeners(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	std::string event,
	CefRefPtr<CefValue> payload)
{
	std::lock_guard<std::recursive_mutex> guard(m_listener_list_mutex);

	if (m_listener_list.empty()) {
		return;
	}

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDict = CefDictionaryValue::Create();

	rootDict->SetString("scope", "broadcast");
	rootDict->SetString("source", source);
	rootDict->SetString("sourceAddress", sourceAddress);
	rootDict->SetValue("message", payload->Copy());

	root->SetDictionary(rootDict);

	std::string payloadJson = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	for (auto kv : m_listener_list) {
		auto browser = kv.first;

		if (kv.second->flags & types) {
			DispatchJSEventContainer(kv.second->target, event,
					      payloadJson);
		}
	}
}

void StreamElementsMessageBus::NotifyAllExternalEventListeners(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	std::string event,
	CefRefPtr<CefValue> payload)
{
	if (DEST_EXTERNAL_CONTROLLER & types) {
		m_external_controller_server.SendEventAllClients(
			source,
			sourceAddress,
			event,
			payload);
	}
}

bool StreamElementsMessageBus::HandleSystemCommands(
	message_destination_filter_flags_t types,
	std::string source,
	std::string sourceAddress,
	CefRefPtr<CefValue> payload)
{
	if (source != SOURCE_EXTERNAL || payload->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> root = payload->GetDictionary();

	if (!root->HasKey("payload") || root->GetType("payload") != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> payloadDict = root->GetDictionary("payload");

	if (!payloadDict->HasKey("class") ||
		!payloadDict->HasKey("command") ||
		payloadDict->GetType("command") != VTYPE_DICTIONARY ||
		payloadDict->GetString("class") != "command") {
		return false;
	}

	CefRefPtr<CefDictionaryValue> commandDict = payloadDict->GetDictionary("command");

	if (!commandDict->HasKey("name") || commandDict->GetType("name") != VTYPE_STRING) {
		return false;
	}

	std::string commandId = commandDict->GetString("name");

	if (commandId == "SYS$QUERY:STATE") {
		PublishSystemState();

		return true;
	}

	return false;
}

void StreamElementsMessageBus::PublishSystemState()
{
	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> payload = CefDictionaryValue::Create();

	bool isLoggedIn =
		StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN == (StreamElementsConfig::GetInstance()->GetStartupFlags() & StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

	payload->SetBool("isSignedIn", isLoggedIn);

	root->SetDictionary(payload);

	NotifyAllExternalEventListeners(
		DEST_ALL_EXTERNAL,
		SOURCE_APPLICATION,
		"OBS",
		"SYS$REPORT:STATE",
		root);
}
