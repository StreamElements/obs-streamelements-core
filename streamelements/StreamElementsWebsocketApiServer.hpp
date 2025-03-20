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

class StreamElementsWebsocketApiServer {
public:
	class ClientInfo {
	public:
		std::string m_target;
		std::string m_unique_id;
		websocketpp::connection_hdl m_con_hdl;

		ClientInfo(std::string target, std::string unique_id,
			websocketpp::connection_hdl con_hdl =
				StreamElementsWebsocketApiServer::connection_hdl_t(
					std::shared_ptr<void>(nullptr)))
			: m_target(target),
			  m_unique_id(unique_id),
			  m_con_hdl(con_hdl)
		{
		}

		ClientInfo(ClientInfo &other)
			: m_target(other.m_target),
			  m_unique_id(other.m_unique_id)
		{
		}

		bool operator==(const ClientInfo& b) {
			return m_target == b.m_target &&
					  m_unique_id == b.m_unique_id;
		}

		bool operator!=(const ClientInfo &b)
		{
			return m_target != b.m_target ||
			       m_unique_id != b.m_unique_id;
		}
	};

public:
	typedef websocketpp::server<websocketpp::config::asio>
		server_t;

	typedef websocketpp::config::core::message_type message_t;

	typedef websocketpp::connection_hdl connection_hdl_t;

	typedef std::function<void(std::shared_ptr<ClientInfo> clientInfo,
				   CefRefPtr<CefProcessMessage> msg)>
		message_handler_t;

public:
	StreamElementsWebsocketApiServer();
	~StreamElementsWebsocketApiServer();

	uint16_t GetPort() const { return m_port; }

	bool DispatchJSEvent(std::string source,
			     std::shared_ptr<ClientInfo> clientInfo,
			     std::string event, std::string json);
	bool DispatchJSEvent(std::string source, std::string event,
			     std::string json);
	bool DispatchTargetJSEvent(std::string source, std::string target,
				   std::string event, std::string json);
	bool DispatchClientMessage(std::string source,
				   std::shared_ptr<ClientInfo> clientInfo,
			     CefRefPtr<CefProcessMessage> msg);
	bool DispatchClientMessage(std::string source,
				   std::shared_ptr<ClientInfo> clientInfo,
			     std::string type, CefRefPtr<CefValue> payload);
	bool DispatchTargetClientMessage(std::string source, std::string target,
					 CefRefPtr<CefProcessMessage> msg);

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
