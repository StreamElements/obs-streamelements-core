#pragma once

#include "deps/server/HttpServer.hpp"

#include <memory>
#include <string>

class SELocalFilesystemHttpServer {
public:
	SELocalFilesystemHttpServer();
	~SELocalFilesystemHttpServer();

	int GetPort() { return m_server->GetPortNumber(); }
	std::string GetBaseUrl()
	{
		char portbuf[8];
		snprintf(portbuf, sizeof(portbuf), "%d", GetPort());

		std::string result = "http://localhost:";
		result += portbuf;

		return result;
	}

	//
	// A URL into this server for an absolute local path, signed so that
	// the path cannot be forged. It was a free function in SEUtils that
	// fetched this server from the singleton for GetBaseUrl() and
	// nothing else.
	//
	std::string CreateSessionSignedAbsolutePathURL(std::wstring path);

	//
	// The verifying half. Static, because it needs no base url -- only
	// the session key -- and because its first caller is the
	// capture-less request handler lambda in this class's own
	// constructor, which has no object to call a member on.
	//
	static bool VerifySessionSignedAbsolutePathURL(std::string url,
						       std::string &path);

private:
	std::shared_ptr<HttpServer> m_server;
};
