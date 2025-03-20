#pragma once

#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define ASIO_STANDALONE
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <functional>
#include <thread>
#include <string>
#include <map>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>

#include "cef-headers.hpp"

class StreamElementsWebsocketApiServer {
public:
	typedef websocketpp::server<websocketpp::config::asio>
		server_t;

	typedef websocketpp::config::core::message_type message_t;

	typedef websocketpp::connection_hdl connection_hdl_t;

	typedef std::function<void(std::string source, CefRefPtr<CefProcessMessage> msg)> message_handler_t;

public:
	StreamElementsWebsocketApiServer();
	~StreamElementsWebsocketApiServer();

	uint16_t GetPort() const { return m_port; }

	bool DispatchJSEvent(std::string source, std::string target, std::string event,
			     std::string json);
	bool DispatchJSEvent(std::string source, std::string event,
			     std::string json);
	bool DispatchClientMessage(std::string source, std::string target,
			     CefRefPtr<CefProcessMessage> msg);
	bool DispatchClientMessage(std::string source, std::string target,
			     std::string type, CefRefPtr<CefValue> payload);

	bool RegisterMessageHandler(std::string target,
				    message_handler_t handler);
	bool UnregisterMessageHandler(std::string target,
				    message_handler_t handler);

private:
	void ParseIncomingMessage(connection_hdl_t con_hdl, std::string payload);
	void ParseIncomingRegisterMessage(connection_hdl_t con_hdl,
					  CefRefPtr<CefDictionaryValue> root);
	void ParseIncomingDispatchMessage(connection_hdl_t con_hdl,
					  CefRefPtr<CefDictionaryValue> root);

private:
	std::shared_mutex m_mutex;
	std::shared_mutex m_dispatch_handlers_map_mutex;

	uint16_t m_port = 27952;
	server_t m_endpoint;
	std::thread m_thread;

	std::map<connection_hdl_t, std::string, std::owner_less<connection_hdl_t>>
		m_connection_map;
	std::map<std::string, message_handler_t> m_dispatch_handlers_map;
};
