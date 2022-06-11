#include "StreamElementsWebsocketApiServer.hpp"

StreamElementsWebsocketApiServer::StreamElementsWebsocketApiServer()
{
	// Set logging settings
	m_endpoint.set_error_channels(websocketpp::log::elevel::all);
	m_endpoint.set_access_channels(websocketpp::log::alevel::all ^
				       websocketpp::log::alevel::frame_payload);

	m_endpoint.init_asio();

	m_endpoint.set_message_handler([this](websocketpp::connection_hdl con_hdl,
					      std::shared_ptr<message_t> msg) {
		auto connection = m_endpoint.get_con_from_hdl(con_hdl);

		std::string data = msg->get_payload();

		::MessageBoxA(0, data.c_str(), "Payload", 0);

		connection->send("test");
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
