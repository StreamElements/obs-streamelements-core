#pragma once

#include "deps/server/HttpServer.hpp"

#include <string>
#include <map>
#include <mutex>

#include "cef-headers.hpp"

class StreamElementsHttpServerManager
{
public:
	StreamElementsHttpServerManager(HttpServer::request_handler_t handler);
	~StreamElementsHttpServerManager();

	void DeserializeHttpServer(CefRefPtr<CefValue> input,
				   CefRefPtr<CefValue> &output);

	void SerializeHttpServers(CefRefPtr<CefValue> &output);

	void RemoveHttpServersByIds(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);

private:
	std::recursive_mutex m_mutex;
	std::map<std::string, std::shared_ptr<HttpServer>> m_servers;
	HttpServer::request_handler_t m_handler;
};
