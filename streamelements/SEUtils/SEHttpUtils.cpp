#include "SEHttpUtils.hpp"

#include "SEAppEnvUtils.hpp"
#include "SEStringUtils.hpp"

#ifdef WIN32
//
// WinHttpGetIEProxyConfigForCurrentUser, for the proxy settings, with the
// link directive that used to sit beside this body in SEUtils.cpp.
//
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")
#endif

// std::transform and std::copy/back_inserter, which the single SEUtils
// translation unit had been getting transitively.
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

static const size_t MAX_HTTP_STRING_RESPONSE_LENGTH = 1024 * 1024 * 100;

struct http_callback_context {
	http_client_callback_t callback;
	void *userdata;
};
static size_t http_write_callback(char *ptr, size_t size, size_t nmemb,
				  void *userdata)
{
	http_callback_context *context = (http_callback_context *)userdata;

	bool result = true;
	if (context->callback) {
		result = context->callback(ptr, size * nmemb, context->userdata,
					   nullptr, 0);
	}

	if (result) {
		return size * nmemb;
	} else {
		return 0;
	}
};
bool HttpGet(std::string method, const char *url,
	     http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	std::transform(method.begin(), method.end(), method.begin(), ::toupper);

	if (curl) {
		if (method == "GET") {
			// NOP
		} else if (method == "DELETE" || method == "OPTIONS") {
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,
					 method.c_str());
		}

		SetGlobalCURLOptions(curl, url);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		http_callback_context context;
		context.callback = callback;
		context.userdata = userdata;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				 http_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

		curl_slist *headers = NULL;
		for (auto h : request_headers) {
			headers = curl_slist_append(
				headers, (h.first + ": " + h.second).c_str());
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		char *errorbuf = new char[CURL_ERROR_SIZE];
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

		CURLcode res = curl_easy_perform(curl);

		curl_slist_free_all(headers);

		if (CURLE_OK == res) {
			result = true;
		}

		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (callback) {
			if (!result) {
				callback(nullptr, 0, 0, errorbuf,
					 (int)http_code);
			} else {
				callback(nullptr, 0, 0, nullptr,
					 (int)http_code);
			}
		}

		delete[] errorbuf;

		curl_easy_cleanup(curl);
	}

	return result;
}
bool HttpGet(const char *url, http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata)
{
	return HttpGet("GET", url, request_headers, callback, userdata);
}
bool HttpPost(std::string method, const char *url,
	      http_client_headers_t request_headers, void *buffer,
	      size_t buffer_len, http_client_callback_t callback,
	      void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	if (curl) {
		SetGlobalCURLOptions(curl, url);

		std::transform(method.begin(), method.end(), method.begin(),
			       ::toupper);

		if (method == "POST") {
			curl_easy_setopt(curl, CURLOPT_POST, 1L);
		} else if (method == "PUT") {
			curl_easy_setopt(curl, CURLOPT_UPLOAD,
					 1L); // implies PUT
		} else if (method == "PATCH") {
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST,
					 method.c_str());
		} else {
			// Unknown method
			curl_easy_cleanup(curl);

			return false;
		}

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 512L * 1024L);

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		http_callback_context context;
		context.callback = callback;
		context.userdata = userdata;

		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
				 http_write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);

		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)buffer_len);
		curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, buffer);

		curl_slist *headers = NULL;
		for (auto h : request_headers) {
			headers = curl_slist_append(
				headers, (h.first + ": " + h.second).c_str());
		}

		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

		char *errorbuf = new char[CURL_ERROR_SIZE];
		curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorbuf);

		CURLcode res = curl_easy_perform(curl);

		curl_slist_free_all(headers);

		if (CURLE_OK == res) {
			result = true;
		}

		long http_code = 0;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

		if (callback) {
			if (!result) {
				callback(nullptr, 0, 0, errorbuf,
					 (int)http_code);
			} else {
				callback(nullptr, 0, 0, nullptr,
					 (int)http_code);
			}
		}

		delete[] errorbuf;

		curl_easy_cleanup(curl);
	}

	return result;
}
bool HttpPost(const char *url, http_client_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
	      void *userdata)
{
	return HttpPost("POST", url, request_headers, buffer, buffer_len,
			callback, userdata);
}
bool HttpGetString(std::string method, const char *url,
		   http_client_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata)
{
	std::vector<char> buffer;
	std::string error = "";
	int http_status_code = 0;

	auto cb = [&](void *data, size_t datalen, void *userdata,
		      char *error_msg, int http_code) -> bool {
		if (http_code != 0) {
			http_status_code = http_code;
		}

		if (error_msg) {
			error = error_msg;

			return false;
		}

		char *in = (char *)data;

		std::copy(in, in + datalen, std::back_inserter(buffer));

		return buffer.size() < MAX_HTTP_STRING_RESPONSE_LENGTH;
	};

	bool success = HttpGet(method, url, request_headers, cb, nullptr);

	buffer.push_back(0);

	callback((char *)&buffer[0], userdata,
		 error.size() ? (char *)error.c_str() : nullptr,
		 http_status_code);

	return success;
}
bool HttpGetString(const char *url, http_client_headers_t request_headers,
		   http_client_string_callback_t callback, void *userdata)
{
	return HttpGetString("GET", url, request_headers, callback, userdata);
}
bool HttpGetBuffer(const char *url, http_client_headers_t request_headers,
		   http_client_buffer_callback_t callback, void *userdata)
{
	std::vector<char> buffer;
	std::string error = "";
	int http_status_code = 0;

	auto cb = [&](void *data, size_t datalen, void *userdata,
		      char *error_msg, int http_code) -> bool {
		if (http_code != 0) {
			http_status_code = http_code;
		}

		if (error_msg) {
			error = error_msg;

			return false;
		}

		char *in = (char *)data;

		std::copy(in, in + datalen, std::back_inserter(buffer));

		return buffer.size() < MAX_HTTP_STRING_RESPONSE_LENGTH;
	};

	bool success = HttpGet(url, request_headers, cb, nullptr);

	buffer.push_back(0);

	callback((void *)&buffer[0], buffer.size(), userdata,
		 error.size() ? (char *)error.c_str() : nullptr,
		 http_status_code);

	return success;
}
bool HttpPostString(std::string method, const char *url,
		    http_client_headers_t request_headers, const char *postData,
		    http_client_string_callback_t callback, void *userdata)
{
	std::vector<char> buffer;
	std::string error = "";
	int http_status_code = 0;

	auto cb = [&](void *data, size_t datalen, void *userdata,
		      char *error_msg, int http_code) -> bool {
		if (http_code != 0) {
			http_status_code = http_code;
		}

		if (error_msg) {
			error = error_msg;

			return false;
		}

		char *in = (char *)data;

		std::copy(in, in + datalen, std::back_inserter(buffer));

		return buffer.size() < MAX_HTTP_STRING_RESPONSE_LENGTH;
	};

	bool success = HttpPost(method, url, request_headers, (void *)postData,
				strlen(postData), cb, nullptr);

	buffer.push_back(0);

	callback((char *)&buffer[0], userdata,
		 error.size() ? (char *)error.c_str() : nullptr,
		 http_status_code);

	return success;
}
bool HttpPostString(const char *url, http_client_headers_t request_headers,
		    const char *postData,
		    http_client_string_callback_t callback, void *userdata)
{
	return HttpPostString("POST", url, request_headers, postData, callback,
			      userdata);
}
std::shared_ptr<CancelableTask>
HttpGetAsync(std::string url, async_http_request_callback_t callback)
{
	return CancelableTask::Execute(
		[=](std::shared_ptr<CancelableTask> task) {
			http_client_headers_t headers;

			auto cb = [=](void *data, size_t datalen,
				      void *userdata, char *error_msg,
				      int http_code) {
				if (!task->IsCancelled()) {
					bool success = http_code >= 200 &&
						       http_code < 400;

					callback(success, data, datalen);
				}
			};

			HttpGetBuffer(url.c_str(), headers, cb, nullptr);
		});
}

//
// Rejoined this domain once the platform split could be handled explicitly:
// the Windows body is here, the macOS one is in SEUtils.mm, because the proxy
// settings come from different places.
//
// convert wstring to UTF-8 string
static std::vector<std::string> tokenizeString(const std::string &str,
					       const std::string &delimiters)
{
	std::vector<std::string> tokens;
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos ||
	       std::string::npos !=
		       lastPos) { // Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
	return tokens;
}
void SetGlobalCURLOptions(CURL *curl, const char *url)
{
	// TODO: TBD: MacOS: http://mirror.informatimago.com/next/developer.apple.com/qa/qa2001/qa1234.html
	std::string proxy =
		GetCommandLineOptionValue("streamelements-http-proxy");

	if (!proxy.size()) {
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config;

		if (WinHttpGetIEProxyConfigForCurrentUser(&config)) {
			// http=127.0.0.1:8888;https=127.0.0.1:8888
			if (config.lpszProxy) {
				proxy = wstring_to_utf8(config.lpszProxy);

				std::map<std::string, std::string> schemes;
				for (auto kvstr : tokenizeString(proxy, ";")) {
					std::vector<std::string> kv =
						tokenizeString(kvstr, "=");

					if (kv.size() == 2) {
						std::transform(kv[0].begin(),
							       kv[0].end(),
							       kv[0].begin(),
							       tolower);
						schemes[kv[0]] = kv[1];
					}
				}

				std::string scheme =
					tokenizeString(url, ":")[0];
				std::transform(scheme.begin(), scheme.end(),
					       scheme.begin(), tolower);

				if (schemes.count(scheme)) {
					proxy = schemes[scheme];
				} else if (schemes.count("http")) {
					proxy = schemes["http"];
				} else {
					proxy = "";
				}
			}

			if (config.lpszProxy) {
				GlobalFree((HGLOBAL)config.lpszProxy);
			}

			if (config.lpszProxyBypass) {
				GlobalFree((HGLOBAL)config.lpszProxyBypass);
			}

			if (config.lpszAutoConfigUrl) {
				GlobalFree((HGLOBAL)config.lpszAutoConfigUrl);
			}
		}
	}

	if (proxy.size()) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
	}
}
