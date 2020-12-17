#pragma once

#include <string>
#include <mutex>
#include <thread>
#include <functional>

#include "../cpp-httplib/httplib.h"

class HttpServer
{
public:
	typedef httplib::Request request_t;
	typedef httplib::Response response_t;
	typedef std::function<void(const request_t &, response_t &)> request_handler_t;

public:
	HttpServer(request_handler_t requestHandler);
	~HttpServer();

	int Start(int bindToPort = 0,
		  std::string bindToIpAddress = "127.0.0.1");
	void Stop();

	const int GetPortNumber() const { return m_portNumber; }
	const std::string GetIpAddress() const { return m_ipAddress; }

private:
	std::recursive_mutex m_mutex;
	std::thread m_thread;
	bool m_running = false;

	request_handler_t m_requestHandler;

	httplib::Server m_server;

	int m_portNumber = 0;
	std::string m_ipAddress = "127.0.0.1";
};
