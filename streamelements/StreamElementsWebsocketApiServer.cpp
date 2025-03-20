#include "StreamElementsWebsocketApiServer.hpp"

#include <asio.hpp>
#include <asio/ts/buffer.hpp>
#include <asio/ts/internet.hpp>

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
			std::unique_lock<decltype(m_mutex)> guard(m_mutex);

			// Disconnect
			if (!m_connection_map.count(con_hdl))
				return;

			std::string id = m_connection_map[con_hdl];
			m_connection_map.erase(con_hdl);
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

	{
		std::shared_lock<decltype(m_dispatch_handlers_map_mutex)>
			lock(m_dispatch_handlers_map_mutex);

		if (!m_dispatch_handlers_map.count(id))
			return;
	}

	{
		std::unique_lock<decltype(m_mutex)> guard(m_mutex);

		m_connection_map[con_hdl] = id;
	}

	auto response = CefValue::Create();
	auto responseDict = CefDictionaryValue::Create();
	responseDict->SetString("id", id);
	response->SetDictionary(responseDict);

	DispatchClientMessage("system", id, "register:response", response);
}

bool StreamElementsWebsocketApiServer::DispatchClientMessage(
	std::string source, std::string target, CefRefPtr<CefProcessMessage> msg)
{
	auto payload = CefDictionaryValue::Create();

	payload->SetString("name", msg->GetName());
	payload->SetList("args", msg->GetArgumentList()->Copy());

	auto val = CefValue::Create();
	val->SetDictionary(payload);

	return DispatchClientMessage(source, target, "dispatch", val);
}

bool StreamElementsWebsocketApiServer::DispatchClientMessage(
	std::string source, std::string target,
		std::string type, CefRefPtr<CefValue> payload)
{
	auto root = CefDictionaryValue::Create();

	root->SetString("type", type);
	root->SetString("source", source);
	root->SetString("target", target);

	root->SetValue("payload", payload);

	auto rootVal = CefValue::Create();

	rootVal->SetDictionary(root);

	std::string json = CefWriteJSON(rootVal, JSON_WRITER_DEFAULT);

	std::shared_lock<decltype(m_mutex)> guard(m_mutex);

	bool result = false;

	for (auto kv : m_connection_map) {
		if (target != kv.second)
			continue;

		auto con_hdl = kv.first;

		auto connection = m_endpoint.get_con_from_hdl(con_hdl);

		if (!connection)
			continue;

		result |= !connection->send(json);
	}

	return result;
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
	std::string source = m_connection_map[con_hdl];

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
	if (!m_dispatch_handlers_map.count(source))
		return;

	m_dispatch_handlers_map[source](source, msg);
}

bool StreamElementsWebsocketApiServer::DispatchJSEvent(std::string source,
						       std::string target,
						       std::string event,
						       std::string json)
{
	auto msg = CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, json);

	return DispatchClientMessage(source, target, msg);
}

bool StreamElementsWebsocketApiServer::DispatchJSEvent(std::string source, std::string event,
						       std::string json)
{
	auto msg = CefProcessMessage::Create("DispatchJSEvent");
	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	args->SetString(0, event);
	args->SetString(1, json);

	std::vector<std::string> targets;

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
