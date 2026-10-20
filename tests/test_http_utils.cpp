//
// Behavioural tests for the libcurl wrappers, against a cpp-httplib server
// on a loopback port.
//
// This is the one test in the suite that is not self-contained: it links
// libcurl and opens a socket, where everything else here builds with a
// compiler and nothing more. tests/CMakeLists.txt therefore registers it
// only when curl is found, so the rest of the suite keeps the property that
// it builds anywhere in seconds.
//
// What is worth pinning is not curl's behaviour but the contract these
// wrappers put on top of it, which is sharper than the signatures suggest
// in four places: a 404 is reported as failure and swallows the error body,
// an unrecognised method is silently sent as GET, the buffer flavour's
// length counts a terminator the body does not have, and the raw callback
// is invoked once more after the last byte with a null data pointer.
//
// Platform note: SetGlobalCURLOptions has a Windows body in SEHttpUtils.cpp
// and a macOS one in SEUtils.mm, so this file supplies it on anything that
// is not Windows. That half is reasoned, not run.
//

#include "streamelements/SEUtils/SEHttpUtils.hpp"

//
// cpp-httplib directly rather than the HttpServer wrapper beside it:
// that wrapper's .cpp includes <QApplication>, and the whole point of
// this file is that nothing in it needs Qt.
//
#include "streamelements/deps/cpp-httplib/httplib.h"

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

#include <chrono>
#include <cstdio>
#include <cstring>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

//
// The link seams. GetCommandLineOptionValue reads the Qt application's
// argument list, which is not linked here, and returning nothing is the same
// answer a run with no --streamelements-http-proxy gives.
//
std::string GetCommandLineOptionValue(const std::string key)
{
	(void)key;

	return std::string();
}

#ifndef WIN32
//
// The Windows body is in the production .cpp compiled beside this test; the
// macOS one is in SEUtils.mm, which needs Objective-C. A no-op is the right
// answer for a loopback request either way.
//
void SetGlobalCURLOptions(CURL *curl, const char *url)
{
	(void)curl;
	(void)url;
}
#endif

/* ------------------------------------------------------------- the server */

static std::string s_baseUrl;

static std::string url_for(const char *path)
{
	return s_baseUrl + path;
}

//
// What the handler saw, for the assertions that are about the request rather
// than the response.
//
static std::string s_lastMethod;

static void handle(const httplib::Request &req,
		   httplib::Response &res)
{
	s_lastMethod = req.method;

	if (req.path == "/hello") {
		res.status = 200;
		res.set_content("hello world", "text/plain");
	} else if (req.path == "/echo-header") {
		res.status = 200;
		res.set_content(req.get_header_value("X-Probe"), "text/plain");
	} else if (req.path == "/echo-header-count") {
		char buf[32];
		std::snprintf(buf, sizeof(buf), "%d",
			      (int)req.get_header_value_count("X-Probe"));
		res.status = 200;
		res.set_content(buf, "text/plain");
	} else if (req.path == "/echo-method") {
		res.status = 200;
		res.set_content(req.method, "text/plain");
	} else if (req.path == "/echo-body") {
		res.status = 200;
		res.set_content(req.body, "text/plain");
	} else if (req.path == "/missing") {
		res.status = 404;
		res.set_content("the error body", "text/plain");
	} else {
		res.status = 500;
		res.set_content("unexpected path", "text/plain");
	}
}

/* ------------------------------------------------------------------ tests */

struct string_result {
	std::string body;
	bool hadError = false;
	int httpCode = 0;
};

static bool get_string(const std::string &method, const std::string &url,
		       string_result &out,
		       http_client_headers_t headers = {})
{
	return HttpGetString(
		method, url.c_str(), headers,
		[&out](char *data, void *, char *error_msg, int http_code) {
			out.body = data ? data : "";
			out.hadError = error_msg != nullptr;
			out.httpCode = http_code;
		},
		nullptr);
}

static void test_a_200_gives_the_body_and_the_status()
{
	string_result r;

	check(get_string("GET", url_for("/hello"), r),
	      "HttpGetString reported failure for a 200");

	check(r.body == "hello world", "the response body is wrong");
	check(r.httpCode == 200, "the reported status code is not 200");
	check(!r.hadError, "a 200 reported an error message");
}

static void test_request_headers_are_sent()
{
	string_result r;
	http_client_headers_t headers;
	headers.insert({"X-Probe", "first"});

	check(get_string("GET", url_for("/echo-header"), r, headers),
	      "the header probe request failed");

	check(r.body == "first",
	      "the request header did not reach the server");

	//
	// http_client_headers_t is a multimap and every entry becomes its own
	// curl_slist line, so a repeated key is sent twice rather than being
	// collapsed or overwritten.
	//
	string_result counted;
	http_client_headers_t twice;
	twice.insert({"X-Probe", "first"});
	twice.insert({"X-Probe", "second"});

	check(get_string("GET", url_for("/echo-header-count"), counted, twice),
	      "the repeated-header request failed");

	check(counted.body == "2",
	      "a repeated request header was not sent twice -- the multimap "
	      "is being collapsed somewhere");
}

static void test_a_404_is_reported_as_failure()
{
	string_result r;

	//
	// CURLOPT_FAILONERROR is set, so curl aborts on a 4xx and the wrapper
	// returns false. The status still arrives, and the error body does
	// not: a caller that only checks the return value cannot tell a 404
	// from a connection failure, and one that wants to read the server's
	// explanation will never see it.
	//
	check(!get_string("GET", url_for("/missing"), r),
	      "a 404 was reported as success");

	check(r.httpCode == 404, "the 404 status did not reach the callback");
	check(r.hadError, "a 404 did not produce an error message");
	check(r.body.empty(),
	      "a 404 delivered the error body -- FAILONERROR is no longer "
	      "set, which changes what every caller sees");
}

static void test_the_method_reaches_the_server()
{
	string_result deleted;

	check(get_string("DELETE", url_for("/echo-method"), deleted),
	      "a DELETE to /echo-method failed");

	check(deleted.body == "DELETE",
	      "DELETE did not reach the server as DELETE");

	//
	// Only GET, DELETE and OPTIONS are recognised. Anything else falls
	// through the if-chain without setting CURLOPT_CUSTOMREQUEST, so it
	// goes out as a GET with nothing to say the method was dropped.
	//
	string_result put;

	get_string("PUT", url_for("/echo-method"), put);

	check(put.body == "GET",
	      "an unrecognised method is no longer silently sent as GET -- if "
	      "that was fixed, this expectation should be updated rather than "
	      "deleted");
}

static void test_post_string_sends_the_body()
{
	string_result r;

	bool ok = HttpPostString(
		url_for("/echo-body").c_str(), {}, "name=value&x=1",
		[&r](char *data, void *, char *error_msg, int http_code) {
			r.body = data ? data : "";
			r.hadError = error_msg != nullptr;
			r.httpCode = http_code;
		},
		nullptr);

	check(ok, "HttpPostString reported failure for a 200");
	check(r.body == "name=value&x=1",
	      "the posted body did not reach the server intact");
	check(s_lastMethod == "POST", "the request did not arrive as a POST");
}

static void test_the_buffer_length_includes_a_terminator()
{
	size_t reported = 0;
	std::string content;

	bool ok = HttpGetBuffer(
		url_for("/hello").c_str(), {},
		[&](void *data, size_t datalen, void *, char *, int) {
			reported = datalen;
			content.assign((const char *)data,
				       datalen ? datalen - 1 : 0);
		},
		nullptr);

	check(ok, "HttpGetBuffer reported failure for a 200");

	//
	// The wrapper appends a NUL so the buffer can be read as a C string,
	// and then reports the size with that NUL counted. "hello world" is
	// eleven bytes and datalen is twelve, so anything passing datalen
	// straight to a binary consumer passes one byte too many.
	//
	check(reported == std::strlen("hello world") + 1,
	      "HttpGetBuffer's datalen is no longer the body length plus one "
	      "for the appended terminator");

	check(content == "hello world",
	      "the buffer contents are wrong once the terminator is dropped");
}

static void test_the_raw_callback_protocol()
{
	std::vector<std::string> chunks;
	int dataCallbackCodes = 0;
	int finalCalls = 0;
	int finalCode = 0;

	bool ok = HttpGet(
		url_for("/hello").c_str(), {},
		[&](void *data, size_t datalen, void *, char *error_msg,
		    int http_code) -> bool {
			(void)error_msg;

			if (data) {
				chunks.push_back(
					std::string((const char *)data,
						    datalen));

				if (http_code != 0)
					++dataCallbackCodes;
			} else {
				++finalCalls;
				finalCode = http_code;
			}

			return true;
		},
		nullptr);

	check(ok, "HttpGet reported failure for a 200");

	std::string joined;

	for (const std::string &c : chunks)
		joined += c;

	check(joined == "hello world",
	      "the chunks the raw callback received do not reassemble into the "
	      "body");

	//
	// The status arrives on its own, after the body: curl cannot report it
	// through the write callback, so the wrapper calls the callback once
	// more with a null data pointer. A callback that dereferences data
	// unconditionally crashes there, which is why both string wrappers
	// guard on it.
	//
	check(finalCalls == 1,
	      "the raw callback was not invoked exactly once after the body "
	      "with a null data pointer");

	check(finalCode == 200,
	      "the status code did not arrive on the final callback");

	check(dataCallbackCodes == 0,
	      "a data callback carried a non-zero http_code -- the string "
	      "wrappers latch the status on any non-zero value, so this would "
	      "change which code they report");
}

static void test_the_async_get()
{
	//
	// Shared rather than captured by reference: CancelableTask::Execute
	// detaches its thread, so if the wait below ever timed out this
	// function would return while the callback still held the capture.
	//
	struct async_result {
		std::promise<void> done;
		std::string body;
		bool success = false;
	};

	auto result = std::make_shared<async_result>();
	auto future = result->done.get_future();

	auto task = HttpGetAsync(
		url_for("/hello"),
		[result](bool success, void *data, size_t len) {
			result->success = success;
			result->body.assign((const char *)data,
					    len ? len - 1 : 0);
			result->done.set_value();
		});

	const bool arrived = future.wait_for(std::chrono::seconds(10)) ==
			     std::future_status::ready;

	check(arrived, "the asynchronous GET never called back");

	if (arrived) {
		//
		// One call, with the whole body: it runs on HttpGetBuffer
		// rather than on the raw callback, so the consumer never sees
		// the chunk-then-status sequence the test above pins -- and
		// the length carries the same extra terminator byte.
		//
		check(result->body == "hello world",
		      "the asynchronous GET delivered the wrong body");

		check(result->success,
		      "the asynchronous GET reported failure for a 200");
	}

	if (task)
		task->Cancel();
}

/* ------------------------------------------------------------------- main */

#ifdef _WIN32
static bool a_system_proxy_is_configured()
{
	WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config = {};

	if (!WinHttpGetIEProxyConfigForCurrentUser(&config))
		return false;

	const bool configured = config.lpszProxy != nullptr;

	if (config.lpszProxy)
		GlobalFree((HGLOBAL)config.lpszProxy);

	if (config.lpszProxyBypass)
		GlobalFree((HGLOBAL)config.lpszProxyBypass);

	if (config.lpszAutoConfigUrl)
		GlobalFree((HGLOBAL)config.lpszAutoConfigUrl);

	return configured;
}
#endif

int main()
{
#ifdef _WIN32
	//
	// A note, not a skip, and not an assertion either.
	//
	// SetGlobalCURLOptions reads the user's proxy configuration and never
	// looks at lpszProxyBypass, so when a proxy is set these loopback
	// requests are sent to it even if the bypass list names localhost --
	// as it does by default on Windows. Nothing below can assert that,
	// because a proxy that forwards to the loopback makes the behaviour
	// identical: the machine this was written on had exactly that, and
	// every check passed through it.
	//
	// The line is here so that an unexplained failure in this file is
	// read correctly. A proxy that refuses loopback destinations fails
	// every request, and the cause is the ignored bypass list rather than
	// anything in the wrappers.
	//
	if (a_system_proxy_is_configured()) {
		std::printf("note: a system proxy is configured, and the "
			    "bypass list is ignored, so these loopback "
			    "requests go through it\n");
	}
#endif

	httplib::Server server;

	//
	// One handler for every verb the tests use, on a catch-all pattern,
	// so the dispatch above decides the response rather than the routing
	// table -- including the 404, which has to come from a matched route
	// to be our 404 rather than the library's.
	//
	server.Get("/.*", handle);
	server.Post("/.*", handle);
	server.Put("/.*", handle);
	server.Delete("/.*", handle);
	server.Options("/.*", handle);

	const int port = server.bind_to_any_port("127.0.0.1");

	if (port <= 0) {
		std::fprintf(stderr, "FAIL: could not bind a loopback port "
				     "for the test server\n");

		return 1;
	}

	char base[64];
	std::snprintf(base, sizeof(base), "http://127.0.0.1:%d", port);
	s_baseUrl = base;

	//
	// The socket is listening as soon as bind_to_any_port returns, so a
	// request issued before this thread gets going simply queues.
	//
	std::thread serving([&server]() { server.listen_after_bind(); });

	test_a_200_gives_the_body_and_the_status();
	test_request_headers_are_sent();
	test_a_404_is_reported_as_failure();
	test_the_method_reaches_the_server();
	test_post_string_sends_the_body();
	test_the_buffer_length_includes_a_terminator();
	test_the_raw_callback_protocol();
	test_the_async_get();

	server.stop();
	serving.join();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);

		return 1;
	}

	std::printf("all http util tests passed\n");

	return 0;
}
