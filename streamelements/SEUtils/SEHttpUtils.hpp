#pragma once

//
// The libcurl wrappers: synchronous GET and POST in buffer and string
// flavours, plus one cancelable asynchronous GET.
//
// SetGlobalCURLOptions is declared here but still defined per platform where
// it always was -- the Windows half in SEUtils.cpp, the macOS half in
// SEUtils.mm, because the proxy settings come from different places.
//

#include "SECancelableTask.hpp"

#include <curl/curl.h>

#include <functional>
#include <map>
#include <memory>
#include <string>

void SetGlobalCURLOptions(CURL *curl, const char *url);

typedef std::function<bool(void *data, size_t datalen, void *userdata,
			   char *error_msg, int http_code)>
	http_client_callback_t;
typedef std::function<void(char *data, void *userdata, char *error_msg,
			   int http_code)>
	http_client_string_callback_t;
typedef std::function<void(void *data, size_t datalen, void *userdata,
			   char *error_msg, int http_code)>
	http_client_buffer_callback_t;
typedef std::multimap<std::string, std::string> http_client_headers_t;

bool HttpGet(const char *url, http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata);

bool HttpGet(std::string method, const char *url,
	     http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata);

bool HttpPost(std::string method, const char *url,
	      http_client_headers_t request_headers, void *buffer,
	      size_t buffer_len, http_client_callback_t callback,
	      void *userdata);

bool HttpPost(const char *url, http_client_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
	      void *userdata);

bool HttpGetString(std::string method, const char *url,
		   http_client_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata);

bool HttpGetString(const char *url, http_client_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata);

bool HttpGetBuffer(const char *url, http_client_headers_t request_headers,
		   http_client_buffer_callback_t callback, void *userdata);

bool HttpPostString(std::string method, const char *url,
		    http_client_headers_t request_headers, const char *postData,
		    http_client_string_callback_t callback, void *userdata);

bool HttpPostString(const char *url, http_client_headers_t request_headers,
		    const char *postData,
		    http_client_string_callback_t callback, void *userdata);

typedef std::function<void(bool success, void *, size_t)>
	async_http_request_callback_t;

std::shared_ptr<CancelableTask>
HttpGetAsync(std::string url, async_http_request_callback_t callback);
