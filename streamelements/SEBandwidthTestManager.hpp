#pragma once

#include "SEBandwidthTestClient.hpp"

#include "../cef-headers.hpp"

#include <mutex>
#include <string>

class SEBandwidthTestManager
{
public:
	SEBandwidthTestManager();
	virtual ~SEBandwidthTestManager();

	bool BeginBandwidthTest(CefRefPtr<CefValue> settingsValue, CefRefPtr<CefValue> serversValue, std::string target);
	CefRefPtr<CefDictionaryValue> EndBandwidthTest(CefRefPtr<CefValue> options);
	CefRefPtr<CefDictionaryValue> GetBandwidthTestStatus();

private:
	std::mutex m_mutex;

	bool m_isTestInProgress;
	SEBandwidthTestClient* m_client = new SEBandwidthTestClient();

	std::vector<SEBandwidthTestClient::Server> m_last_test_servers;
	std::vector<SEBandwidthTestClient::Result> m_last_test_results;
};
