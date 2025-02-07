#pragma once

#include "deps/server/HttpServer.hpp"

#include <memory>

class StreamElementsLocalFilesystemHttpServer {
public:
	StreamElementsLocalFilesystemHttpServer();
	~StreamElementsLocalFilesystemHttpServer();

	int GetPort() { return m_server->GetPortNumber(); }
	std::string GetBaseUrl()
	{
		char portbuf[8];
		snprintf(portbuf, sizeof(portbuf), "%d", GetPort());

		std::string result = "http://localhost:";
		result += portbuf;

		return result;
	}

private:
	std::shared_ptr<HttpServer> m_server;
};
