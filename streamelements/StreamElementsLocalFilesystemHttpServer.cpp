#include "StreamElementsLocalFilesystemHttpServer.hpp" 
#include "StreamElementsUtils.hpp"

#include <obs.h>
#include <QUrl>
#include <QUrlQuery>

#include <algorithm>

static size_t LocalReadFile(void *buffer, size_t len, FILE *wf)
{
	return fread(buffer, 1, len, wf);
}

StreamElementsLocalFilesystemHttpServer::
StreamElementsLocalFilesystemHttpServer() {
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

		if (!VerifySessionSignedAbsolutePathURL(urlString, path)) {
			blog(LOG_WARNING,
			     "StreamElementsLocalFilesystemHttpServer: invalid request signature: %s",
			     urlString.c_str());

			res.status = 429;
			res.reason = "Invalid Request Signature";
			res.set_content(
				"{ \"success\": false, \"message\": \"Invalid Request Signature\" }",
				"application/json");
		}

		blog(LOG_INFO,
			"StreamElementsLocalFilesystemHttpServer: serving file from path: %s",
			path.c_str());

		#ifdef _WIN32
		std::wstring wpath = utf8_to_wstring(path);
		int handle = _wopen(wpath.c_str(), O_BINARY | O_RDONLY);
		#else
		int handle = ::open(path.c_str(), O_RDONLY);
		#endif

		if (handle < 0) {
			blog(LOG_WARNING,
				"StreamElementsLocalFilesystemHttpServer: file not found: %s",
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
		     "Could not start StreamElementsLocalFilesystemHttpServer: HttpServer");
	}
}

StreamElementsLocalFilesystemHttpServer::~StreamElementsLocalFilesystemHttpServer()
{
	m_server->Stop();
}
