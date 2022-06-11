#pragma once

#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define ASIO_STANDALONE
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <functional>

class StreamElementsWebsocketApiServer {
public:
	typedef websocketpp::server<websocketpp::config::asio>
		server_t;

public:
	StreamElementsWebsocketApiServer() {
		// Set logging settings
		m_endpoint.set_error_channels(websocketpp::log::elevel::all);
		m_endpoint.set_access_channels(
			websocketpp::log::alevel::all ^
			websocketpp::log::alevel::frame_payload);

		m_endpoint.init_asio();

		//m_endpoint.set_message_handler();

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

		// m_endpoint.run();
	}

	~StreamElementsWebsocketApiServer() { m_endpoint.stop(); }

private:
	uint16_t m_port = 27952;
	server_t m_endpoint;
};
