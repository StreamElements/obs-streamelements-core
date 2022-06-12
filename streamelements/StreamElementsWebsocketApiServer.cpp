#include "StreamElementsWebsocketApiServer.hpp"

StreamElementsWebsocketApiServer::StreamElementsWebsocketApiServer()
{
	// TODO: Remove
	RegisterMessageHandler(
		"node-client",
		[this](std::string source, CefRefPtr<CefProcessMessage> msg) {
			auto val = CefValue::Create();
			auto d = CefDictionaryValue::Create();
			d->SetString("hello", "there");
			val->SetDictionary(d);

			DispatchClientMessage("system", source, "test", val);
		});

	// Set logging settings
	m_endpoint.set_error_channels(websocketpp::log::elevel::all);
	m_endpoint.set_access_channels(websocketpp::log::alevel::all ^
				       websocketpp::log::alevel::frame_payload);

	m_endpoint.init_asio();

	m_endpoint.set_open_handler(
		[this](websocketpp::connection_hdl con_hdl) {
			std::lock_guard<decltype(m_mutex)> guard(m_mutex);

			// Connect
			auto connection = m_endpoint.get_con_from_hdl(con_hdl);

			//connection->send("test on open");
		});

	m_endpoint.set_close_handler(
		[this](websocketpp::connection_hdl con_hdl) {
			std::lock_guard<decltype(m_mutex)> guard(m_mutex);

			// Disconnect
			if (!m_connection_map.count(con_hdl))
				return;

			std::string id = m_connection_map[con_hdl];
			m_connection_map.erase(con_hdl);

			m_client_connection_map.erase(id);

			if (m_client_connection_map.count(id))
				m_client_connection_map.erase(id);
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

	// Find first available port
	do {
		ec.clear();

		m_endpoint.listen(m_port, ec);

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
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

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
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (root->GetType("payload") != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> payload = root->GetDictionary("payload");

	if (payload->GetType("id") != VTYPE_STRING)
		return;

	std::string id = payload->GetString("id");

	if (!m_dispatch_handlers_map.count(id))
		return;

	m_client_connection_map[id] = con_hdl;
	m_connection_map[con_hdl] = id;

	auto response = CefValue::Create();
	auto responseDict = CefDictionaryValue::Create();
	responseDict->SetString("id", id);
	response->SetDictionary(responseDict);

	DispatchClientMessage("system", id, "register:response", response);
}

bool StreamElementsWebsocketApiServer::DispatchClientMessage(
	std::string source, std::string target, CefRefPtr<CefProcessMessage> msg)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	auto payload = CefDictionaryValue::Create();

	payload->SetString("name", msg->GetName());
	payload->SetList("args", msg->GetArgumentList()->Copy());

	auto val = CefValue::Create();
	val->SetDictionary(payload);

	DispatchClientMessage(source, target, "dispatch", val);
}

bool StreamElementsWebsocketApiServer::DispatchClientMessage(
	std::string source, std::string target,
		std::string type, CefRefPtr<CefValue> payload)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!m_client_connection_map.count(target))
		return false;

	auto root = CefDictionaryValue::Create();

	root->SetString("type", type);
	root->SetString("source", source);
	root->SetString("target", target);

	root->SetValue("payload", payload);

	auto rootVal = CefValue::Create();

	rootVal->SetDictionary(root);

	std::string json = CefWriteJSON(rootVal, JSON_WRITER_DEFAULT);

	auto con_hdl = m_client_connection_map[target];
	auto connection = m_endpoint.get_con_from_hdl(con_hdl);

	if (!connection)
		return false;

	return !connection->send(json);
}

bool StreamElementsWebsocketApiServer::RegisterMessageHandler(
	std::string target, message_handler_t handler)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_dispatch_handlers_map.count(target))
		return false;

	m_dispatch_handlers_map[target] = handler;

	return true;
}

bool StreamElementsWebsocketApiServer::UnregisterMessageHandler(
	std::string target, message_handler_t handler)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

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

	// Check if handlers are registered
	if (!m_dispatch_handlers_map.count(source))
		return;

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

	m_dispatch_handlers_map[source](source, msg);
}
