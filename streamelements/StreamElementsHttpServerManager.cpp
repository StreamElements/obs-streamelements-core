#include "StreamElementsHttpServerManager.hpp"
#include "StreamElementsUtils.hpp"

StreamElementsHttpServerManager::StreamElementsHttpServerManager(
	HttpServer::request_handler_t handler)
	: m_handler(handler)
{
}

StreamElementsHttpServerManager::~StreamElementsHttpServerManager()
{
	m_servers.clear();
}

void StreamElementsHttpServerManager::DeserializeHttpServer(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	int portNumber = 0;
	std::string ipAddress = "127.0.0.1";

	if (d->HasKey("portNumber") && d->GetType("portNumber") == VTYPE_INT) {
		portNumber = d->GetInt("portNumber");
	}

	if (d->HasKey("ipAddress") && d->GetType("ipAddress") == VTYPE_STRING) {
		ipAddress = d->GetString("ipAddress").ToString();
	}

	std::string id = CreateGloballyUniqueIdString();
	std::shared_ptr<HttpServer> server = std::make_shared<HttpServer>(m_handler);

	portNumber = server->Start(portNumber, ipAddress);

	if (portNumber > 0) {
		m_servers[id] = server;

		CefRefPtr<CefDictionaryValue> result =
			CefDictionaryValue::Create();

		result->SetString("id", id.c_str());
		result->SetInt("portNumber", portNumber);
		result->SetString("ipAddress", ipAddress.c_str());

		output->SetDictionary(result);
	}
}

void StreamElementsHttpServerManager::SerializeHttpServers(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefListValue> list = CefListValue::Create();

	for (auto kv : m_servers) {
		auto id = kv.first;
		auto server = kv.second;

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", id.c_str());
		d->SetString("ipAddress", server->GetIpAddress().c_str());
		d->SetInt("portNumber", server->GetPortNumber());

		list->SetDictionary(list->GetSize(), d);
	}

	output->SetList(list);
}

void StreamElementsHttpServerManager::RemoveHttpServersByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (input->GetType() == VTYPE_STRING) {
		std::string id = input->GetString().ToString();

		if (m_servers.count(id)) {
			m_servers.erase(id);
		}
	} else if (input->GetType() == VTYPE_LIST) {
		CefRefPtr<CefListValue> list = input->GetList();

		for (size_t index = 0; index < list->GetSize(); ++index) {
			if (list->GetType(index) == VTYPE_STRING) {
				std::string id = list->GetString(index);

				if (m_servers.count(id)) {
					m_servers.erase(id);
				}
			}
		}
	}

	output->SetBool(true);
}
