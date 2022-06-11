#pragma once

#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define ASIO_STANDALONE
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <functional>
#include <thread>

class StreamElementsWebsocketApiServer {
public:
	typedef websocketpp::server<websocketpp::config::asio>
		server_t;

	typedef websocketpp::config::core::message_type message_t;

public:
	StreamElementsWebsocketApiServer();
	~StreamElementsWebsocketApiServer();

private:
	uint16_t m_port = 27952;
	server_t m_endpoint;
	std::thread m_thread;
};
