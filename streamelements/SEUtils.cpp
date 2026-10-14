#include <string.h>

#include "SELocalFilesystemHttpServer.hpp"
#include "SEUtils.hpp"
#include "SEConfig.hpp"
#include "SEGlobalStateManager.hpp"
#include "SERemoteIconLoader.hpp"
#include "SEPleaseWaitWindow.hpp"
#include "Version.hpp"
#include "wide-string.hpp"
#include "deps/utf8.h"

#define GLOBAL_ENV_CONFIG_FILE_NAME "obs-studio/streamelements-env.ini"

#include <cstdint>
#include <atomic>
#include <chrono>
#include <vector>
#include <regex>
#include <unordered_map>
#include <filesystem>
#include <regex>

#include <curl/curl.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <obs.hpp>

#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QApplication>
#include <QProcess>
#include <QLatin1String>
#include <QStringList>

#include "deps/picosha2/picosha2.h"

#ifndef WIN32
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#ifndef WIN32
#define sprintf_s sprintf

#ifndef WCHAR
typedef wchar_t WCHAR;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef HKEY
typedef void *HKEY;
#endif

#include <sys/syscall.h>
#endif

/* ========================================================= */


/* ========================================================= */


/* ========================================================= */


/* ========================================================= */


/* ========================================================= */


/* ========================================================= */

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

/* ========================================================= */


/* ========================================================= */








/* ========================================================= */



/* ========================================================= */




/* ========================================================= */






#ifdef _WIN32
#endif

/* ========================================================= */















/* ========================================================= */

#ifdef WIN32
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")
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
#endif












/* ========================================================= */




#ifdef WIN32
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif





/* ========================================================= */

#ifndef WIN32
#include <uuid/uuid.h>
#endif


#ifdef WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

#ifdef WIN32
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
#endif


std::string CreateSessionSignedAbsolutePathURL(std::wstring path)
{
	// static, because constructing a std::wregex compiles the pattern and
	// this is called once per asset. getAllRazerWyvrnEvents signs ~23,700
	// URLs in one call; at two fresh regexes each that was ~47,000 pattern
	// compilations and the dominant cost of the whole request.
	static const std::wregex kHash(L"#");
	static const std::wregex kAmp(L"&");

	path = std::regex_replace(path, kHash, L"%23");
	path = std::regex_replace(path, kAmp, L"%26");

	QUrl parts(SEGlobalStateManager::GetInstance()
			   ->GetLocalFilesystemHttpServer()
			   ->GetBaseUrl()
			   .c_str());

	parts.setPath(
		QString::fromStdWString((std::wstring(L"/") + path).c_str()));

	std::string message = parts.path().toStdString();

	message = message.erase(0, 1);

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return parts.url().toStdString() + std::string("?digest=") +
	       CreateSessionMessageSignature(message);
}

bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path)
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

	std::string message = path;

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return VerifySessionMessageSignature(message, signature);
}

/* ========================================================= */




void AdviseHostUserInterfaceStateChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {
		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			os_atomic_set_bool(&isBusy, false);

			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostUserInterfaceStateChanged",
					      "null");
		});
	}
}

void AdviseHostHotkeyBindingsChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {

		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostHotkeyBindingsChanged",
					      "null");

			os_atomic_set_bool(&isBusy, false);
		});
	}
}

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId)
{
	std::regex url_regex(
		"^https://streamelements.com/overlay/([^/]+)/([^/]+)$");
	std::smatch match;

	if (std::regex_match(url, match, url_regex)) {
		overlayId = match[1].str();
		accountId = match[2].str();

		return true;
	}

	return false;
}

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId)
{
	return std::string("https://streamelements.com/overlay/") + overlayId +
	       std::string("/editor");
}
























/* ========================================================= */






#ifdef WIN32
#endif


void DispatchClientMessage(std::string target, CefRefPtr<CefProcessMessage> msg)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetClientMessage("system", target, msg);
}

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", event, eventArgsJson);
}

void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetJSEvent("system", target, event,
					 eventArgsJson);
}

















