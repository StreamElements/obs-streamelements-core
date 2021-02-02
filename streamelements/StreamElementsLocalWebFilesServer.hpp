#pragma once

#include <string>
#include <map>
#include "cef-headers.hpp"

#include "StreamElementsFileSystemMapper.hpp"

class StreamElementsLocalWebFilesServer
{
public:
	StreamElementsLocalWebFilesServer(std::string rootFolder);
	~StreamElementsLocalWebFilesServer();

	///
	// Check if host is mapped to a local folder.
	//
	bool HasHost(std::string host);

	///
	// Map host & request path to local file absolute_path
	//
	bool MapRequestPath(std::string host, std::string relative, std::string& absolute_path);

	///
	// Map CEF request to a CefResourceHandler.
	//
	// This is the method which should be called to map a
	// CefRequest to local files CefResourceHandler
	// implementation.
	//
	CefRefPtr<CefResourceHandler> GetCefResourceHandler(
		CefRefPtr<CefBrowser> browser,
		CefRefPtr<CefFrame> frame,
		CefRefPtr<CefRequest> request);

	///
	// Create a CEF request for serving a pre-defined string & content type
	// off a specified URL.
	//
	CefRefPtr<CefRequest> CreateCefRequestForString(std::string data,
							std::string contentType,
							std::string url);

private:
	std::string m_rootFolder;
	std::map<std::string, std::shared_ptr<StreamElementsFileSystemMapper>> m_hostsMap;

	const std::string m_sessionId;
	const std::string m_HEADER_INTERCEPT_CONTENT_TYPE;
	const std::string m_HEADER_INTERCEPT_BASE64_CONTENT;
};
