#include "SELocalFilesystemHttpServer.hpp" 
#include "SEUtils.hpp"

#include <obs.h>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>
#include <regex>

static size_t LocalReadFile(void *buffer, size_t len, FILE *wf)
{
	return fread(buffer, 1, len, wf);
}

SELocalFilesystemHttpServer::
SELocalFilesystemHttpServer() {
	auto request_handler = [](const HttpServer::request_t &req,
				      HttpServer::response_t &res) {
		QUrl url;
		QUrlQuery query;

		for (auto kv : req.params) {
			query.addQueryItem(kv.first.c_str(), kv.second.c_str());
		}

		url.setScheme("http");
		url.setHost("localhost");
		url.setPath(req.path.c_str());
		url.setQuery(query);

		std::string urlString = url.url().toStdString();
		std::string path;

		if (!SELocalFilesystemHttpServer::
			    VerifySessionSignedAbsolutePathURL(urlString,
							       path)) {
			blog(LOG_WARNING,
			     "SELocalFilesystemHttpServer: invalid request signature: %s",
			     urlString.c_str());

			res.status = 429;
			res.reason = "Invalid Request Signature";
			res.set_content(
				"{ \"success\": false, \"message\": \"Invalid Request Signature\" }",
				"application/json");

			// Without this the handler carried on: it opened the
			// file, attached a content provider for it, and reset
			// the status to 200 further down. The bytes did not
			// actually escape -- set_content had already claimed
			// the body, so the provider's were discarded -- but
			// that is cpp-httplib's precedence protecting us, not
			// the check. The rejection also reported 200, logged
			// "serving file from path", and opened a descriptor
			// whose cleanup lambda only runs if the provider is
			// used, leaking one handle per rejected request.
			return;
		}

		blog(LOG_INFO,
			"SELocalFilesystemHttpServer: serving file from path: %s",
			path.c_str());

		#ifdef _WIN32
		std::wstring wpath = utf8_to_wstring(path);
		int handle = _wopen(wpath.c_str(), O_BINARY | O_RDONLY);
		#else
		int handle = ::open(path.c_str(), O_RDONLY);
		#endif

		if (handle < 0) {
			blog(LOG_WARNING,
				"SELocalFilesystemHttpServer: file not found: %s",
				path.c_str());

			res.status = 404;
			res.reason = "Not Found";
			res.set_content(
				"{ \"success\": false, \"message\": \"File not found\" }",
				"application/json");

			return;
		}

		int buflen = 32768;
		char *buffer = new char[buflen];

		httplib::ContentProvider content_provider =
			[handle,buffer,buflen](size_t offset, size_t length,
				httplib::DataSink &sink)
		{
			UNUSED_PARAMETER(offset);
			
			int req_read = std::min(buflen, (int)length);
			int bytes_read = read(handle, buffer, req_read);

			if (bytes_read > 0) {
				sink.write(buffer, bytes_read);
			} else {
				sink.done();
			}

			return true;
		};

		res.set_content_provider(
			os_get_file_size(path.c_str()),
			"application/octet-stream",
			content_provider,
			[handle,buffer]() -> void {
				delete[] buffer;
				::close(handle);
			});

		res.status = 200;
		res.reason = "OK";
	};

	m_server = std::make_shared<HttpServer>(request_handler);

	// Bind to any port on localhost
	if (m_server->Start() <= 0) {
		blog(LOG_ERROR,
		     "Could not start SELocalFilesystemHttpServer: HttpServer");
	}
}

SELocalFilesystemHttpServer::~SELocalFilesystemHttpServer()
{
	m_server->Stop();
}

std::string SELocalFilesystemHttpServer::CreateSessionSignedAbsolutePathURL(
	std::wstring path)
{
	// static, because constructing a std::wregex compiles the pattern and
	// this is called once per asset. getAllRazerWyvrnEvents signs ~23,700
	// URLs in one call; at two fresh regexes each that was ~47,000 pattern
	// compilations and the dominant cost of the whole request.
	static const std::wregex kHash(L"#");
	static const std::wregex kAmp(L"&");

	path = std::regex_replace(path, kHash, L"%23");
	path = std::regex_replace(path, kAmp, L"%26");

	QUrl parts(GetBaseUrl().c_str());

	parts.setPath(
		QString::fromStdWString((std::wstring(L"/") + path).c_str()));

	std::string message = parts.path().toStdString();

	message = message.erase(0, 1);

	return parts.url().toStdString() + std::string("?digest=") +
	       CreateSessionMessageSignature(message);
}

bool SELocalFilesystemHttpServer::VerifySessionSignedAbsolutePathURL(
	std::string url, std::string &path)
{
	QUrl parts(url.c_str());

	path = parts.path().toStdString();

	path = path.erase(0, 1);

	if (!parts.hasQuery())
		return false;

	QUrlQuery query(parts.query());

	if (!query.hasQueryItem("digest"))
		return false;

	std::string signature = query.queryItemValue("digest").toStdString();

	//
	// A copy, because VerifySessionMessageSignature takes its message by
	// non-const reference and `path` is this function's out-parameter.
	//
	std::string message = path;

	return VerifySessionMessageSignature(message, signature);
}
