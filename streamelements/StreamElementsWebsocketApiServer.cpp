#include "StreamElementsWebsocketApiServer.hpp"
#include "StreamElementsUtils.hpp"

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

std::shared_ptr<StreamElementsWebsocketApiServer::ClientInfo>
StreamElementsWebsocketApiServer::AddConnection(std::string target,
	std::string unique_id,
	connection_hdl_t con_hdl)
{
	std::unique_lock<decltype(m_mutex)> guard(m_mutex);

	auto clientInfo = std::make_shared<ClientInfo>(target, unique_id, con_hdl);

	m_connection_map[con_hdl] = clientInfo;

	if (!m_target_to_connection_hdl_map.count(target))
		m_target_to_connection_hdl_map[target] =
			std::make_shared<connection_hdl_container_t>();

	m_target_to_connection_hdl_map[target]->m_con_hdl_map[con_hdl] =
		clientInfo;

	return clientInfo;
}

void StreamElementsWebsocketApiServer::RemoveConnection(
	connection_hdl_t con_hdl)
{
	std::unique_lock<decltype(m_mutex)> guard(m_mutex);

	if (!m_connection_map.count(con_hdl))
		return;

	auto clientInfo = m_connection_map[con_hdl];

	m_connection_map.erase(con_hdl);

	std::string id = clientInfo->m_target;

	if (!m_target_to_connection_hdl_map.count(id))
		return;

	auto container = m_target_to_connection_hdl_map[id];

	container->m_con_hdl_map.erase(con_hdl);

	if (container->m_con_hdl_map.empty()) {
		m_target_to_connection_hdl_map.erase(id);
	}
}


StreamElementsWebsocketApiServer::StreamElementsWebsocketApiServer()
{
	// Set logging settings
	m_endpoint.set_error_channels(websocketpp::log::elevel::all);
	m_endpoint.set_access_channels(websocketpp::log::alevel::all ^
				       websocketpp::log::alevel::frame_payload);

	m_endpoint.init_asio();

	/*
	m_endpoint.set_open_handler(
		[this](websocketpp::connection_hdl con_hdl) {
			std::lock_guard<decltype(m_mutex)> guard(m_mutex);

			// Connect
			auto connection = m_endpoint.get_con_from_hdl(con_hdl);

			//connection->send("test on open");
		});
	*/

	m_endpoint.set_close_handler(
		[this](websocketpp::connection_hdl con_hdl) {
			// Disconnect
			RemoveConnection(con_hdl);
		});

	m_endpoint.set_message_handler(
		[this](websocketpp::connection_hdl con_hdl,
		       std::shared_ptr<message_t> msg) {
			auto connection = m_endpoint.get_con_from_hdl(con_hdl);

			std::string msgData = msg->get_payload();

			ParseIncomingMessage(con_hdl, msgData);

			//connection->send(msgData + " - test");
		});

	websocketpp::lib::error_code ec;

	auto localhost = asio::ip::make_address_v4("127.0.0.1");

	// Find first available port
	do {
		ec.clear();

		m_endpoint.listen(
			asio::ip::tcp::endpoint(
				localhost,
				m_port
			), ec);

		if (ec.value()) {
			++m_port;
		}
	} while (ec.value() && m_port < 65535);

	m_endpoint.start_accept();

	m_thread = std::thread([this]() { m_endpoint.run(); });
}

StreamElementsWebsocketApiServer::~StreamElementsWebsocketApiServer()
{
	m_endpoint.stop();
	m_thread.join();
}

void StreamElementsWebsocketApiServer::ParseIncomingMessage(
	connection_hdl_t con_hdl, std::string payload)
{
	CefRefPtr<CefValue> val =
		CefParseJSON(payload, JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!val.get() || (val->GetType() != VTYPE_DICTIONARY))
		return;

	CefRefPtr<CefDictionaryValue> root = val->GetDictionary();

	if (root->GetType("type") != VTYPE_STRING)
		return;

	std::string type = root->GetString("type");

	if (type == "dispatch") {
		ParseIncomingDispatchMessage(con_hdl, root);
	} else if (type == "register") {
		ParseIncomingRegisterMessage(con_hdl, root);
	}
}

void StreamElementsWebsocketApiServer::ParseIncomingRegisterMessage(
	connection_hdl_t con_hdl,
				CefRefPtr<CefDictionaryValue> root)
{
	if (root->GetType("payload") != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> payload = root->GetDictionary("payload");

	if (payload->GetType("id") != VTYPE_STRING)
		return;

	std::string id = payload->GetString("id");
	std::string unique_id = CreateGloballyUniqueIdString();

	{
		std::shared_lock<decltype(m_dispatch_handlers_map_mutex)>
			lock(m_dispatch_handlers_map_mutex);

		if (!m_dispatch_handlers_map.count(id))
			return;
	}

	auto clientInfo = AddConnection(id, unique_id, con_hdl);

	auto response = CefValue::Create();
	auto responseDict = CefDictionaryValue::Create();
	responseDict->SetString("id", id);
	responseDict->SetString("unique_id", unique_id);
	response->SetDictionary(responseDict);

	DispatchClientMessage("system", clientInfo, "register:response", response);
}

bool StreamElementsWebsocketApiServer::DispatchClientMessage(
	std::string source, std::shared_ptr<ClientInfo> clientInfo,
	CefRefPtr<CefProcessMessage> msg)
{
	auto payload = CefDictionaryValue::Create();

	payload->SetString("name", msg->GetName());
	payload->SetList("args", msg->GetArgumentList()->Copy());

	auto val = CefValue::Create();
	val->SetDictionary(payload);

	return DispatchClientMessage(source, clientInfo, "dispatch", val);
}

bool StreamElementsWebsocketApiServer::DispatchClientMessage(
	std::string source, std::shared_ptr<ClientInfo> clientInfo,
		std::string type, CefRefPtr<CefValue> payload)
{
	auto root = CefDictionaryValue::Create();

	root->SetString("type", type);
	root->SetString("source", source);
	root->SetString("target", clientInfo->m_target);
	root->SetString("target_unique_id", clientInfo->m_unique_id);

	root->SetValue("payload", payload);

	auto rootVal = CefValue::Create();

	rootVal->SetDictionary(root);

	std::string json = CefWriteJSON(rootVal, JSON_WRITER_DEFAULT);

	std::shared_lock<decltype(m_mutex)> guard(m_mutex);

	bool result = false;

	try {
		auto connection =
			m_endpoint.get_con_from_hdl(clientInfo->m_con_hdl);

		if (!connection)
			return false;

		return !connection->send(json);
	} catch(...) {
		return false;
	}
}

bool StreamElementsWebsocketApiServer::RegisterMessageHandler(
	std::string target, message_handler_t handler)
{
	std::unique_lock<decltype(m_dispatch_handlers_map_mutex)> guard(
		m_dispatch_handlers_map_mutex);

	if (m_dispatch_handlers_map.count(target))
		return false;

	m_dispatch_handlers_map[target] = handler;

	return true;
}

bool StreamElementsWebsocketApiServer::UnregisterMessageHandler(
	std::string target, message_handler_t handler)
{
	std::unique_lock<decltype(m_dispatch_handlers_map_mutex)> guard(
		m_dispatch_handlers_map_mutex);

	if (m_dispatch_handlers_map.count(target)) {
		m_dispatch_handlers_map.erase(target);

		return true;
	}

	return false;
}

void StreamElementsWebsocketApiServer::ParseIncomingDispatchMessage(
	connection_hdl_t con_hdl, CefRefPtr<CefDictionaryValue> root)
{
	// Check if msg target registered
	if (!m_connection_map.count(con_hdl))
		return;

	// Get msg source from connection
	auto clientInfo = m_connection_map[con_hdl];

	if (root->GetType("payload") != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> payload = root->GetDictionary("payload");

	if (payload->GetType("name") != VTYPE_STRING ||
	    payload->GetType("args") != VTYPE_LIST)
		return;

	std::string name = payload->GetString("name");
	auto args = payload->GetList("args");

	auto msg = CefProcessMessage::Create(name);
	auto msgArgs = msg->GetArgumentList();

	for (size_t i = 0; i < args->GetSize(); ++i) {
		msgArgs->SetValue(i, args->GetValue(i));
	}

	std::shared_lock<decltype(m_dispatch_handlers_map_mutex)>
		lock(m_dispatch_handlers_map_mutex);

	// Check if handlers are registered
	if (!m_dispatch_handlers_map.count(clientInfo->m_target))
		return;

	m_dispatch_handlers_map[clientInfo->m_target](clientInfo, msg);
}

bool StreamElementsWebsocketApiServer::DispatchJSEvent(std::string source, std::shared_ptr<ClientInfo> clientInfo,
						       std::string event,
						       std::string json)
{
	auto msg = CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, json);

	return DispatchClientMessage(source, clientInfo, msg);
}

bool StreamElementsWebsocketApiServer::DispatchJSEvent(std::string source, std::string event,
						       std::string json)
{
	auto msg = CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, json);

	std::vector<std::shared_ptr<ClientInfo>> targets;

	{
		std::shared_lock<decltype(m_mutex)> guard(m_mutex);

		for (auto it : m_connection_map) {
			targets.push_back(it.second);
		}
	}

	for (auto target : targets) {
		DispatchClientMessage(source, target, msg);
	}

	return true;
}

bool StreamElementsWebsocketApiServer::DispatchTargetJSEvent(std::string source,
							     std::string target,
							     std::string event,
							     std::string json)
{
	auto msg = CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, json);

	std::vector<std::shared_ptr<ClientInfo>> targets;

	{
		std::shared_lock<decltype(m_mutex)> guard(m_mutex);

		if (!m_target_to_connection_hdl_map.count(target))
			return false;

		for (auto it :
		     m_target_to_connection_hdl_map[target]->m_con_hdl_map) {
			targets.push_back(it.second);
		}
	}

	for (auto target : targets) {
		DispatchClientMessage(source, target, msg);
	}

	return true;
}

bool StreamElementsWebsocketApiServer::DispatchTargetClientMessage(
	std::string source, std::string target,
	CefRefPtr<CefProcessMessage> msg)
{
	std::vector<std::shared_ptr<ClientInfo>> targets;

	{
		std::shared_lock<decltype(m_mutex)> guard(m_mutex);

		if (!m_target_to_connection_hdl_map.count(target))
			return false;

		for (auto it : m_target_to_connection_hdl_map[target]->m_con_hdl_map) {
			targets.push_back(it.second);
		}
	}

	for (auto it : targets) {
		DispatchClientMessage(source, it, msg);
	}

	return true;
}
