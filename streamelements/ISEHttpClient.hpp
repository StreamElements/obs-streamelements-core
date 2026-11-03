#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the HTTP client: the one call behind
// httpRequestText.
//
// Note that output is taken BY VALUE and mutated through the handle,
// not by reference. That is the production signature and it is kept
// exactly: a stand-in that took it by value and then assigned to it
// would write into a local, leaving the handler to answer whatever the
// dispatcher pre-set -- which is the shape
// check_every_stand_in_can_answer_the_handler_that_calls_it exists to
// catch.
//
class ISEHttpClient {
public:
	virtual ~ISEHttpClient() {}

	//
	// Performs the request described by input and writes the response
	// through output. Synchronous, and it holds the API lock for the whole
	// request -- which is how a url it could not use once wedged the
	// entire host API.
	//
	virtual void DeserializeHttpRequestText(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> output) = 0;
};
