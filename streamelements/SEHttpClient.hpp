#pragma once

#include "ISEHttpClient.hpp"

#include "cef-headers.hpp"

class SEHttpClient : public ISEHttpClient {
public:
	SEHttpClient() {}
	~SEHttpClient() {}

public:
	void DeserializeHttpRequestText(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> output) override;
};
