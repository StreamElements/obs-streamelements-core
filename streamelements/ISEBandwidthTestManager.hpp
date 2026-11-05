#pragma once

#include "cef-headers.hpp"

#include <string>

//
// What the host API needs from the bandwidth test. The test runs on its own
// thread inside the concrete class and announces progress through the host
// event dispatcher, so there is nothing to poll here beyond the status.
//
class ISEBandwidthTestManager {
public:
	virtual ~ISEBandwidthTestManager() {}

	virtual bool BeginBandwidthTest(CefRefPtr<CefValue> settingsValue,
					CefRefPtr<CefValue> serversValue,
					std::string target) = 0;

	virtual CefRefPtr<CefDictionaryValue>
	EndBandwidthTest(CefRefPtr<CefValue> options) = 0;

	virtual CefRefPtr<CefDictionaryValue> GetBandwidthTestStatus() = 0;
};
