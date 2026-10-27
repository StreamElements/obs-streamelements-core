#pragma once

#define _WEBSOCKETPP_CPP11_TYPE_TRAITS_
#define ASIO_STANDALONE
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>

#include <functional>
#include <thread>
#include <string>
#include <map>
#include <set>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <shared_mutex>

#include "cef-headers.hpp"
#include "SEApiMessageSink.hpp"

class SEWebsocketApiServer : public ISEApiMessageSink {
public:
	//
	// Was a nested class; it is SEApiClientInfo now, in a header that
	// needs none of websocketpp. The alias keeps the other files that
	// spell SEWebsocketApiServer::ClientInfo compiling, and reads
	// correctly besides -- a client of this server is what it is.
	//
	typedef SEApiClientInfo ClientInfo;

	typedef ISEApiMessageSink::message_handler_t message_handler_t;

public:
	typedef websocketpp::server<websocketpp::config::asio>
		server_t;

	typedef websocketpp::config::core::message_type message_t;

	typedef websocketpp::connection_hdl connection_hdl_t;

public:
	SEWebsocketApiServer();
	~SEWebsocketApiServer();

	uint16_t GetPort() const override { return m_port; }

	bool DispatchJSEvent(std::string source,
			     std::shared_ptr<ClientInfo> clientInfo,
			     std::string event, std::string json) override;
	bool DispatchJSEvent(std::string source, std::string event,
			     std::string json) override;
	bool DispatchTargetJSEvent(std::string source, std::string target,
				   std::string event, std::string json);
	bool DispatchClientMessage(std::string source,
				   std::shared_ptr<ClientInfo> clientInfo,
				   CefRefPtr<CefProcessMessage> msg) override;
	bool DispatchClientMessage(std::string source,
				   std::shared_ptr<ClientInfo> clientInfo,
			     std::string type, CefRefPtr<CefValue> payload);
	bool DispatchTargetClientMessage(std::string source, std::string target,
					 CefRefPtr<CefProcessMessage> msg);

	bool RegisterMessageHandler(std::string target,
				    message_handler_t handler) override;
	bool UnregisterMessageHandler(std::string target,
				      message_handler_t handler) override;

private:
	void ParseIncomingMessage(connection_hdl_t con_hdl, std::string payload);
	void ParseIncomingRegisterMessage(connection_hdl_t con_hdl,
					  CefRefPtr<CefDictionaryValue> root);
	void ParseIncomingDispatchMessage(connection_hdl_t con_hdl,
					  CefRefPtr<CefDictionaryValue> root);

	std::shared_ptr<ClientInfo> AddConnection(std::string target,
						  std::string unique_id,
						  connection_hdl_t con_hdl);
	void RemoveConnection(connection_hdl_t con_hdl);

private:
	std::shared_mutex m_mutex;
	std::shared_mutex m_dispatch_handlers_map_mutex;

	uint16_t m_port = 27952;
	server_t m_endpoint;
	std::thread m_thread;

	std::map<connection_hdl_t, std::shared_ptr<ClientInfo>, std::owner_less<connection_hdl_t>>
		m_connection_map;

	class connection_hdl_container_t {
	public:
		std::map<connection_hdl_t, std::shared_ptr<ClientInfo>,
			 std::owner_less<connection_hdl_t>>
			m_con_hdl_map;
	};

	std::map < std::string, std::shared_ptr<connection_hdl_container_t>>
		m_target_to_connection_hdl_map;

	std::map<std::string, message_handler_t> m_dispatch_handlers_map;
};
