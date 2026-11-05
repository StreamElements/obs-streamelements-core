#pragma once

#include <memory>
#include <string>

//
// A browser client the host API can answer: which container it belongs to,
// which instance of that container, and the connection it arrived on.
//
// This was SEWebsocketApiServer::ClientInfo, nested inside a header that
// pulls in websocketpp, asio, openssl and zlib. It appears in nine
// signatures on the API handler infrastructure -- which is otherwise
// dependency-free -- so the nesting alone decided that the infrastructure
// could not be compiled anywhere the transport is not available, tests/
// included.
//
// The connection handle is std::weak_ptr<void>, which is exactly what
// websocketpp::connection_hdl is. SEWebsocketApiServer.cpp static_asserts
// that, so the two cannot drift apart quietly if websocketpp's smart-pointer
// configuration ever changes under us.
//
class SEApiClientInfo {
public:
	std::string m_target;
	std::string m_unique_id;
	std::weak_ptr<void> m_con_hdl;

	SEApiClientInfo(std::string target, std::string unique_id,
			std::weak_ptr<void> con_hdl = std::weak_ptr<void>())
		: m_target(target), m_unique_id(unique_id), m_con_hdl(con_hdl)
	{
	}

	//
	// Moved verbatim, non-const reference and dropped connection handle
	// included. A copy constructor taking a mutable reference also
	// suppresses the implicit one, so every copy of a client loses the
	// connection it came in on -- which is harmless only because nothing
	// copies one: clients travel as shared_ptr everywhere. Left as it was
	// rather than quietly corrected, because there is no caller to verify
	// a correction against.
	//
	SEApiClientInfo(SEApiClientInfo &other)
		: m_target(other.m_target), m_unique_id(other.m_unique_id)
	{
	}

	bool operator==(const SEApiClientInfo &b)
	{
		return m_target == b.m_target && m_unique_id == b.m_unique_id;
	}

	bool operator!=(const SEApiClientInfo &b)
	{
		return m_target != b.m_target || m_unique_id != b.m_unique_id;
	}
};
