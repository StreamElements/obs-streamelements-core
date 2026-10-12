#pragma once

#include "cef-headers.hpp"

class SEHttpClient
{
public:
	SEHttpClient() {}
	~SEHttpClient() {}

public:
	void DeserializeHttpRequestText(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> output);
};
