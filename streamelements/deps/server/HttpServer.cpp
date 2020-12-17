#include "HttpServer.hpp"


HttpServer::HttpServer(request_handler_t requestHandler) : m_requestHandler(requestHandler)
{
	auto reqHandler = [this](const httplib::Request& req,
				 httplib::Response& res) -> void {
		if (req.has_header("Origin")) {
			std::string origin = req.get_header_value("Origin");

			res.set_header("Access-Control-Allow-Origin", origin.c_str());
		} else {
			res.set_header("Access-Control-Allow-Origin", "*");
		}

		res.set_header("Access-Control-Allow-Methods", "*");
		res.set_header("Access-Control-Allow-Credentials", "true");

		m_requestHandler(req, res);
	};

	m_server.Get(".*", reqHandler);
	m_server.Post(".*", reqHandler);
	m_server.Delete(".*", reqHandler);
	m_server.Put(".*", reqHandler);
	m_server.Patch(".*", reqHandler);
	m_server.Options(".*", reqHandler);
}

HttpServer::~HttpServer()
{
	Stop();
}

void HttpServer::Stop()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	m_running = false;

	if (m_server.is_running()) {
		m_server.stop();

		m_thread.join();
	}
}

int HttpServer::Start(int bindToPort /*= 0*/,
		      std::string bindToIpAddress /*= "127.0.0.1"*/)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_running) {
		// Already running
		return 0;
	}

	int port = bindToPort;

	if (port > 0) {
		if (!m_server.bind_to_port(bindToIpAddress.c_str(), port)) {
			port = 0;
		}
	} else {
		port = m_server.bind_to_any_port(bindToIpAddress.c_str());
	}

	if (port <= 0) {
		return 0;
	}

	m_portNumber = port;
	m_ipAddress = bindToIpAddress;

	m_running = true;

	m_thread = std::thread([this]() {
		m_server.listen_after_bind();

		m_running = false;
	});

	return port;
}
