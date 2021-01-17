#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsRemoteIconLoader.hpp"
#include "StreamElementsPleaseWaitWindow.hpp"
#include "Version.hpp"
#include "wide-string.hpp"

#define GLOBAL_ENV_CONFIG_FILE_NAME "obs-studio/streamelements-env.ini"

#if CHROME_VERSION_BUILD >= 3729
#include <include/cef_api_hash.h>
#endif

#include <cstdint>
#include <codecvt>
#include <vector>
#include <regex>
#include <unordered_map>

#include <curl/curl.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>

#include <QUrl>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QApplication>
#include <QProcess>
#include <regex>

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
#endif

/* ========================================================= */

static const char *ENV_PRODUCT_NAME = "OBS.Live";

/* ========================================================= */

// convert wstring to UTF-8 string
static std::string wstring_to_utf8(const std::wstring &str)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;
	return myconv.to_bytes(str);
}

std::string clean_guid_string(std::string input)
{
	return std::regex_replace(input, std::regex("-"), "");
}

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

void QtPostTask(std::function<void()> task)
{
	struct local_context {
		std::function<void()> task;
	};

	local_context *context = new local_context();
	context->task = task;

	QtPostTask(
		[](void *const data) {
			local_context *context = (local_context *)data;

			context->task();

			delete context;
		},
		context);
}

void QtPostTask(void (*func)(void *), void *const data)
{
	QTimer *t = new QTimer();
	t->moveToThread(qApp->thread());
	t->setSingleShot(true);
	QObject::connect(t, &QTimer::timeout, [=]() {
		t->deleteLater();

		func(data);
	});
	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
				  Q_ARG(int, 0));
}

void QtExecSync(std::function<void()> task)
{
	struct local_context {
		std::function<void()> task;
	};

	local_context *context = new local_context();
	context->task = task;

	QtExecSync(
		[](void *data) {
			local_context *context = (local_context *)data;

			context->task();

			delete context;
		},
		context);
}

void QtExecSync(void (*func)(void *), void *const data)
{
	if (QThread::currentThread() == qApp->thread()) {
		func(data);
	} else {
		os_event_t *completeEvent;

		os_event_init(&completeEvent, OS_EVENT_TYPE_AUTO);

		QTimer *t = new QTimer();
		t->moveToThread(qApp->thread());
		t->setSingleShot(true);
		QObject::connect(t, &QTimer::timeout, [=]() {
			t->deleteLater();

			func(data);

			os_event_signal(completeEvent);
		});
		QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
					  Q_ARG(int, 0));

		QApplication::sendPostedEvents();

		os_event_wait(completeEvent);
		os_event_destroy(completeEvent);
	}
}

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area)
{
	switch (area) {
	case Qt::LeftDockWidgetArea:
		return "left";
	case Qt::RightDockWidgetArea:
		return "right";
	case Qt::TopDockWidgetArea:
		return "top";
	case Qt::BottomDockWidgetArea:
		return "bottom";
	case Qt::NoDockWidgetArea:
	default:
		return "floating";
	}
}

std::string GetCommandLineOptionValue(const std::string key)
{
	QStringList args = QCoreApplication::instance()->arguments();

	std::string search = "--" + key + "=";

	for (int i = 0; i < args.size(); ++i) {
		std::string arg = args.at(i).toStdString();

		if (arg.substr(0, search.size()) == search) {
			return arg.substr(search.size());
		}
	}

	return std::string();
}

std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
}

/* ========================================================= */

#ifdef WIN32
static uint64_t FromFileTime(const FILETIME &ft)
{
	ULARGE_INTEGER uli = {0};
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}
#endif

void SerializeSystemTimes(CefRefPtr<CefValue> &output)
{
	SYNC_ACCESS();

	output->SetNull();

#ifdef WIN32
	FILETIME idleTime;
	FILETIME kernelTime;
	FILETIME userTime;

	if (::GetSystemTimes(&idleTime, &kernelTime, &userTime)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		static bool hasSavedStartingValues = false;
		static uint64_t savedIdleTime;
		static uint64_t savedKernelTime;
		static uint64_t savedUserTime;

		if (!hasSavedStartingValues) {
			savedIdleTime = FromFileTime(idleTime);
			savedKernelTime = FromFileTime(kernelTime);
			savedUserTime = FromFileTime(userTime);

			hasSavedStartingValues = true;
		}

		const uint64_t SECOND_PART = 10000000L;
		const uint64_t MS_PART = SECOND_PART / 1000L;

		uint64_t idleInt = FromFileTime(idleTime) - savedIdleTime;
		uint64_t kernelInt = FromFileTime(kernelTime) - savedKernelTime;
		uint64_t userInt = FromFileTime(userTime) - savedUserTime;

		uint64_t idleMs = idleInt / MS_PART;
		uint64_t kernelMs = kernelInt / MS_PART;
		uint64_t userMs = userInt / MS_PART;

		uint64_t idleSec = idleMs / (uint64_t)1000;
		uint64_t kernelSec = kernelMs / (uint64_t)1000;
		uint64_t userSec = userMs / (uint64_t)1000;

		uint64_t idleMod = idleMs % (uint64_t)1000;
		uint64_t kernelMod = kernelMs % (uint64_t)1000;
		uint64_t userMod = userMs % (uint64_t)1000;

		double idleRat = idleSec + ((double)idleMod / 1000.0);
		double kernelRat = kernelSec + ((double)kernelMod / 1000.0);
		double userRat = userSec + ((double)userMod / 1000.0);

		// https://msdn.microsoft.com/en-us/84f674e7-536b-4ae0-b523-6a17cb0a1c17
		// lpKernelTime [out, optional]
		// A pointer to a FILETIME structure that receives the amount of time that
		// the system has spent executing in Kernel mode (including all threads in
		// all processes, on all processors)
		//
		// >>> This time value also includes the amount of time the system has been idle.
		//

		d->SetDouble("idleSeconds", idleRat);
		d->SetDouble("kernelSeconds", kernelRat - idleRat);
		d->SetDouble("userSeconds", userRat);
		d->SetDouble("totalSeconds", kernelRat + userRat);
		d->SetDouble("busySeconds", kernelRat + userRat - idleRat);
	}
#else
    mach_port_t mach_port = mach_host_self();
    host_cpu_load_info_data_t cpu_load_info;

    mach_msg_type_number_t cpu_load_info_count = HOST_CPU_LOAD_INFO_COUNT;
    if (host_statistics((host_t)mach_port, HOST_CPU_LOAD_INFO, (host_info_t)&cpu_load_info, &cpu_load_info_count) == KERN_SUCCESS) {
        CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
        output->SetDictionary(d);

        d->SetDouble("idleSeconds", (double)(cpu_load_info.cpu_ticks[CPU_STATE_IDLE]) / (double)CLOCKS_PER_SEC);
        d->SetDouble("kernelSeconds", (double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM]) / (double)CLOCKS_PER_SEC);
        d->SetDouble("userSeconds", (double)(cpu_load_info.cpu_ticks[CPU_STATE_USER] + cpu_load_info.cpu_ticks[CPU_STATE_NICE]) / (double)CLOCKS_PER_SEC);
        d->SetDouble("totalSeconds", (double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] + cpu_load_info.cpu_ticks[CPU_STATE_USER] + cpu_load_info.cpu_ticks[CPU_STATE_IDLE] + cpu_load_info.cpu_ticks[CPU_STATE_NICE]) / (double)CLOCKS_PER_SEC);
        d->SetDouble("busySeconds", (double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] + cpu_load_info.cpu_ticks[CPU_STATE_USER] + cpu_load_info.cpu_ticks[CPU_STATE_NICE]) / (double)CLOCKS_PER_SEC);
    }
#endif
}

void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output)
{
	output->SetNull();

    const uint64_t DIV = 1048576;

#ifdef _WIN32
	MEMORYSTATUSEX mem;

	mem.dwLength = sizeof(mem);

	if (GlobalMemoryStatusEx(&mem)) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage", mem.dwMemoryLoad);
		d->SetInt("totalPhysicalMemory", mem.ullTotalPhys / DIV);
		d->SetInt("freePhysicalMemory", mem.ullAvailPhys / DIV);
		d->SetInt("totalVirtualMemory", mem.ullTotalVirtual / DIV);
		d->SetInt("freeVirtualMemory", mem.ullAvailVirtual / DIV);
		d->SetInt("freeExtendedVirtualMemory",
			  mem.ullAvailExtendedVirtual / DIV);
		d->SetInt("totalPageFileSize", mem.ullTotalPageFile / DIV);
		d->SetInt("freePageFileSize", mem.ullAvailPageFile / DIV);
	}
#else
    mach_port_t mach_port = mach_host_self();
    vm_statistics_data_t vm_stats;

    mach_msg_type_number_t vm_info_count = HOST_VM_INFO_COUNT;
    vm_size_t page_size;
    if (host_statistics((host_t)mach_port, HOST_VM_INFO, (host_info_t)&vm_stats, &vm_info_count) == KERN_SUCCESS &&
        host_page_size(mach_port, &page_size) == KERN_SUCCESS) {
        int64_t free_memory = (int64_t)vm_stats.free_count * (int64_t)page_size;
        int64_t used_memory = ((int64_t)vm_stats.active_count + (int64_t)vm_stats.inactive_count + (int64_t)vm_stats.wire_count) * (int64_t)page_size;
        int64_t total_memory = free_memory + used_memory;

        CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
        output->SetDictionary(d);

        d->SetString("units", "MB");
        d->SetInt("memoryUsedPercentage", used_memory * 100L / total_memory);
        d->SetInt("totalVirtualMemory", total_memory / DIV);
        d->SetInt("freeVirtualMemory", free_memory / DIV);
        d->SetInt("freeExtendedVirtualMemory", 0);
        d->SetInt("totalPageFileSize", 0);
        d->SetInt("freePageFileSize", 0);
        
        {
            int mib[2] = { CTL_HW, HW_MEMSIZE };
            int64_t physical_memory;
            size_t length = sizeof(physical_memory);
            sysctl(mib, 2, &physical_memory, &length, NULL, 0);

            d->SetInt("totalPhysicalMemory", physical_memory / DIV);
            d->SetInt("freePhysicalMemory", free_memory / DIV); // inaccurate
        }
    }
#endif
}

static CefString getRegStr(HKEY parent, const WCHAR *subkey, const WCHAR *key)
{
	CefString result;

#ifdef WIN32
	DWORD dataSize = 0;

	if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key, RRF_RT_ANY,
					    NULL, NULL, &dataSize)) {
		WCHAR *buffer = new WCHAR[dataSize];

		if (ERROR_SUCCESS == ::RegGetValueW(parent, subkey, key,
						    RRF_RT_ANY, NULL, buffer,
						    &dataSize)) {
			result = buffer;
		}

		delete[] buffer;
	}
#endif

	return result;
};

static DWORD getRegDWORD(HKEY parent, const WCHAR *subkey, const WCHAR *key)
{
	DWORD result = 0;

#ifdef WIN32
	DWORD dataSize = sizeof(DWORD);

	::RegGetValueW(parent, subkey, key, RRF_RT_DWORD, NULL, &result,
		       &dataSize);
#endif

	return result;
}

#ifdef _WIN32
void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output)
{
	output->SetNull();

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
	output->SetDictionary(d);

    d->SetString("platform", "windows");

	SYSTEM_INFO info;

	::GetNativeSystemInfo(&info);

	switch (info.wProcessorArchitecture) {
	case PROCESSOR_ARCHITECTURE_INTEL:
		d->SetString("cpuArch", "x86");
		break;
	case PROCESSOR_ARCHITECTURE_IA64:
		d->SetString("cpuArch", "IA64");
		break;
	case PROCESSOR_ARCHITECTURE_AMD64:
		d->SetString("cpuArch", "x64");
		break;
	case PROCESSOR_ARCHITECTURE_ARM:
		d->SetString("cpuArch", "ARM");
		break;
	case PROCESSOR_ARCHITECTURE_ARM64:
		d->SetString("cpuArch", "ARM64");
		break;

	default:
	case PROCESSOR_ARCHITECTURE_UNKNOWN:
		d->SetString("cpuArch", "Unknown");
		break;
	}

	d->SetInt("cpuCount", info.dwNumberOfProcessors);
	d->SetInt("cpuLevel", info.wProcessorLevel);

	{
		CefRefPtr<CefListValue> cpuList = CefListValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS ==
		    ::RegOpenKeyA(
			    HKEY_LOCAL_MACHINE,
			    "HARDWARE\\DESCRIPTION\\System\\CentralProcessor",
			    &hRoot)) {
			WCHAR cpuKeyBuffer[2048];

			for (DWORD index = 0;
			     ERROR_SUCCESS ==
			     ::RegEnumKeyW(hRoot, index, cpuKeyBuffer,
					   sizeof(cpuKeyBuffer));
			     ++index) {
				CefRefPtr<CefDictionaryValue> p =
					CefDictionaryValue::Create();

				p->SetString("name",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"ProcessorNameString"));
				p->SetString("vendor",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"VendorIdentifier"));
				p->SetInt("speedMHz",
					  getRegDWORD(hRoot, cpuKeyBuffer,
						      L"~MHz"));
				p->SetString("identifier",
					     getRegStr(hRoot, cpuKeyBuffer,
						       L"Identifier"));

				cpuList->SetDictionary(cpuList->GetSize(), p);
			}

			::RegCloseKey(hRoot);
		}

		d->SetList("cpuHardware", cpuList);
	}

	{
		CefRefPtr<CefDictionaryValue> bios =
			CefDictionaryValue::Create();

		HKEY hRoot;
		if (ERROR_SUCCESS ==
		    ::RegOpenKeyW(HKEY_LOCAL_MACHINE,
				  L"HARDWARE\\DESCRIPTION\\System", &hRoot)) {
			HKEY hBios;
			if (ERROR_SUCCESS ==
			    ::RegOpenKeyW(hRoot, L"BIOS", &hBios)) {
				WCHAR subKeyBuffer[2048];
				DWORD bufSize = sizeof(subKeyBuffer) /
						sizeof(subKeyBuffer[0]);

				DWORD valueIndex = 0;
				DWORD valueType = 0;

				LSTATUS callStatus = ::RegEnumValueW(
					hBios, valueIndex, subKeyBuffer,
					&bufSize, NULL, &valueType, NULL, NULL);
				while (ERROR_NO_MORE_ITEMS != callStatus) {
					switch (valueType) {
					case REG_DWORD_BIG_ENDIAN:
					case REG_DWORD_LITTLE_ENDIAN:
						bios->SetInt(
							subKeyBuffer,
							getRegDWORD(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;

					case REG_QWORD:
						bios->SetInt(
							subKeyBuffer,
							getRegDWORD(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;

					case REG_SZ:
					case REG_EXPAND_SZ:
					case REG_MULTI_SZ:
						bios->SetString(
							subKeyBuffer,
							getRegStr(
								hRoot, L"BIOS",
								subKeyBuffer));
						break;
					}

					++valueIndex;

					bufSize = sizeof(subKeyBuffer) /
						  sizeof(subKeyBuffer[0]);
					callStatus = ::RegEnumValueW(
						hBios, valueIndex, subKeyBuffer,
						&bufSize, NULL, &valueType,
						NULL, NULL);
				}

				::RegCloseKey(hBios);
			}

			::RegCloseKey(hRoot);
		}

		d->SetDictionary("bios", bios);
	}
    
    d->SetString("os", "Windows");
}
#endif

/* ========================================================= */

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &output)
{
	// Response codec collection (array)
	CefRefPtr<CefListValue> list = CefListValue::Create();

	// Response codec collection is our root object
	output->SetList(list);

	// Iterate over all input sources
	bool continue_iteration = true;
	for (size_t idx = 0; continue_iteration; ++idx) {
		// Filled by obs_enum_input_types() call below
		const char *sourceId;

		// Get next input source type, obs_enum_input_types() returns true as long as
		// there is data at the specified index
		continue_iteration = obs_enum_input_types(idx, &sourceId);

		if (continue_iteration) {
			// Get source caps
			uint32_t sourceCaps =
				obs_get_source_output_flags(sourceId);

			// If source has video
			if ((sourceCaps & OBS_SOURCE_VIDEO) ==
			    OBS_SOURCE_VIDEO) {
				// Create source response dictionary
				CefRefPtr<CefDictionaryValue> dic =
					CefDictionaryValue::Create();

				// Set codec dictionary properties
				dic->SetString("id", sourceId);
				dic->SetString(
					"name",
					obs_source_get_display_name(sourceId));
				dic->SetBool("hasVideo",
					     (sourceCaps & OBS_SOURCE_VIDEO) ==
						     OBS_SOURCE_VIDEO);
				dic->SetBool("hasAudio",
					     (sourceCaps & OBS_SOURCE_AUDIO) ==
						     OBS_SOURCE_AUDIO);

				// Compare sourceId to known video capture devices
				dic->SetBool(
					"isVideoCaptureDevice",
					strcmp(sourceId, "dshow_input") == 0 ||
						strcmp(sourceId,
						       "decklink-input") == 0);

				// Compare sourceId to known game capture source
				dic->SetBool("isGameCaptureDevice",
					     strcmp(sourceId, "game_capture") ==
						     0);

				// Compare sourceId to known browser source
				dic->SetBool("isBrowserSource",
					     strcmp(sourceId,
						    "browser_source") == 0);

				// Append dictionary to response list
				list->SetDictionary(list->GetSize(), dic);
			}
		}
	}
}

std::string SerializeAppStyleSheet()
{
	std::string result = qApp->styleSheet().toStdString();

	if (result.compare(0, 8, "file:///") == 0) {
		QUrl url(result.c_str());

		if (url.isLocalFile()) {
			result = result.substr(8);

			QFile file(result.c_str());

			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QTextStream in(&file);

				result = in.readAll().toStdString();
			}
		}
	}

	return result;
}

std::string GetAppStyleSheetSelectorContent(std::string selector)
{
	std::string result;

	std::string css = SerializeAppStyleSheet();

	std::replace(css.begin(), css.end(), '\n', ' ');

	css = std::regex_replace(css, std::regex("/\\*.*?\\*/"), "");

	std::regex selector_regex("[^\\s]*" + selector + "[\\s]*\\{(.*?)\\}");
	std::smatch selector_match;

	if (std::regex_search(css, selector_match, selector_regex)) {
		result = std::string(selector_match[1].first,
				     selector_match[1].second);
	}

	return result;
}

std::string GetCurrentThemeName()
{
	std::string result;

	config_t *globalConfig =
		obs_frontend_get_global_config(); // does not increase refcount

	const char *themeName =
		config_get_string(globalConfig, "General", "CurrentTheme");
	if (!themeName) {
		/* Use deprecated "Theme" value if available */
		themeName = config_get_string(globalConfig, "General", "Theme");
		if (!themeName) {
			themeName = "Default";
		}

		result = themeName;
	}

	std::string appStyle = qApp->styleSheet().toStdString();

	if (appStyle.substr(0, 7) == "file://") {
		QUrl url(appStyle.c_str());

		if (url.isLocalFile()) {
			result = url.fileName().split('.')[0].toStdString();
		}
	}

	return result;
}

std::string GetCefVersionString()
{
	char buf[64];

	sprintf(buf, "cef.%d.%d.chrome.%d.%d.%d.%d", cef_version_info(0),
		cef_version_info(1), cef_version_info(2), cef_version_info(3),
		cef_version_info(4), cef_version_info(5));

	return std::string(buf);
}

std::string GetCefPlatformApiHash()
{
	return cef_api_hash(0);
}

std::string GetCefUniversalApiHash()
{
	return cef_api_hash(1);
}

std::string GetStreamElementsPluginVersionString()
{
	char version_buf[64];
	sprintf(version_buf, "%d.%d.%d.%d",
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 1000000000000L) /
		      10000000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 10000000000L) /
		      100000000L),
		(int)((STREAMELEMENTS_PLUGIN_VERSION % 100000000L) / 1000000L),
		(int)(STREAMELEMENTS_PLUGIN_VERSION % 1000000L));

	return version_buf;
}

std::string GetStreamElementsApiVersionString()
{
	char version_buf[64];

	sprintf(version_buf, "%d.%d", HOST_API_VERSION_MAJOR,
		HOST_API_VERSION_MINOR);

	return version_buf;
}

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

bool HttpGet(const char *url, http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	if (curl) {
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

bool HttpPost(const char *url, http_client_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
	      void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	if (curl) {
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

		curl_easy_setopt(curl, CURLOPT_POST, 1L);
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

static const size_t MAX_HTTP_STRING_RESPONSE_LENGTH = 1024 * 1024 * 100;

bool HttpGetString(const char *url, http_client_headers_t request_headers,
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

	bool success = HttpGet(url, request_headers, cb, nullptr);

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

	bool success = HttpPost(url, request_headers, (void *)postData,
				strlen(postData), cb, nullptr);

	buffer.push_back(0);

	callback((char *)&buffer[0], userdata,
		 error.size() ? (char *)error.c_str() : nullptr,
		 http_status_code);

	return success;
}

/* ========================================================= */

static std::string GetEnvironmentConfigRegKeyPath(const char *productName)
{
#ifdef _WIN64
	std::string REG_KEY_PATH = "SOFTWARE\\WOW6432Node\\StreamElements";
#else
	std::string REG_KEY_PATH = "SOFTWARE\\StreamElements";
#endif

	if (productName && productName[0]) {
		REG_KEY_PATH += "\\";
		REG_KEY_PATH += productName;
	}

	return REG_KEY_PATH;
}

static std::string ReadEnvironmentConfigString(const char *regValueName,
					       const char *productName)
{
	std::string result = "";

#ifdef WIN32
	std::string REG_KEY_PATH = GetEnvironmentConfigRegKeyPath(productName);

	DWORD bufLen = 16384;
	char *buffer = new char[bufLen];

	LSTATUS lResult = RegGetValueA(HKEY_LOCAL_MACHINE, REG_KEY_PATH.c_str(),
				       regValueName, RRF_RT_REG_SZ, NULL,
				       buffer, &bufLen);

	if (ERROR_SUCCESS == lResult) {
		result = buffer;
	}

	delete[] buffer;
#else
	config_t *config;

	char *filePath = os_get_config_path_ptr(GLOBAL_ENV_CONFIG_FILE_NAME);
	config_open(&config, filePath, CONFIG_OPEN_ALWAYS);
	bfree(filePath);

	const char* str = config_get_string(config, productName ? productName : "Global", regValueName);

	if (str) {
		result = str;
	}

	config_close(config);
#endif

	return result;
}

bool WriteEnvironmentConfigString(const char *regValueName,
				  const char *regValue, const char *productName)
{
	bool result = false;

#ifdef WIN32
	std::string REG_KEY_PATH = GetEnvironmentConfigRegKeyPath(productName);

	LSTATUS lResult = RegSetKeyValueA(HKEY_LOCAL_MACHINE,
					  REG_KEY_PATH.c_str(), regValueName,
					  REG_SZ, regValue, strlen(regValue));

	if (lResult != ERROR_SUCCESS) {
		result = WriteEnvironmentConfigStrings(
			{{productName ? productName : "", regValueName,
			  regValue}});
	} else {
		result = true;
	}
#else
	config_t *config;

	char *filePath = os_get_config_path_ptr(GLOBAL_ENV_CONFIG_FILE_NAME);
	config_open(&config, filePath, CONFIG_OPEN_ALWAYS);
	bfree(filePath);

	config_set_string(config,
				      productName ? productName : "Global",
				      regValueName, regValue);

	config_save_safe(config, "tmp", "bak");

	config_close(config);

	result = true;
#endif
	return result;
}

#ifdef WIN32
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
static std::wstring GetCurrentDllFolderPathW()
{
	std::wstring result = L"";

	WCHAR path[MAX_PATH];
	HMODULE hModule;

	if (GetModuleHandleExW(
		    GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
			    GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
		    (LPWSTR)&GetCurrentDllFolderPathW, &hModule)) {
		GetModuleFileNameW(hModule, path, sizeof(path));
		PathRemoveFileSpecW(path);

		result = std::wstring(path);

		if (!result.empty() && result[result.size() - 1] != '\\')
			result += L"\\";
	}

	return result;
}
#endif

bool WriteEnvironmentConfigStrings(streamelements_env_update_requests requests)
{
#ifndef WIN32
	for (auto req : requests) {
		if (!WriteEnvironmentConfigString(req.key.c_str(), req.value.c_str(),
						  req.product.c_str())) {
			return false;
		}
	}

	return true;
#else
	std::vector<std::string> args;

	for (auto req : requests) {
		if (req.product.size()) {
			args.push_back(req.product + "/" + req.key + "=" +
				       req.value);
		} else {
			args.push_back(req.key + "=" + req.value);
		}
	}

	STARTUPINFOW startInf;
	memset(&startInf, 0, sizeof startInf);
	startInf.cb = sizeof(startInf);

	PROCESS_INFORMATION procInf;
	memset(&procInf, 0, sizeof procInf);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	std::wstring wArgs;
	for (auto arg : args) {
		wArgs += L"\"" + myconv.from_bytes(arg) + L"\" ";
	}
	if (!wArgs.empty()) {
		// Remove trailing space
		wArgs = wArgs.substr(0, wArgs.size() - 1);
	}

	std::wstring wExePath = GetCurrentDllFolderPathW() +
				L"obs-streamelements-set-machine-config.exe";

	HINSTANCE hInst = ShellExecuteW(NULL, L"runas", wExePath.c_str(),
					wArgs.c_str(), NULL, SW_SHOW);

	BOOL bResult = hInst > (HINSTANCE)32;

	return bResult;
#endif
}

std::string ReadProductEnvironmentConfigurationString(const char *key)
{
	return ReadEnvironmentConfigString(key, ENV_PRODUCT_NAME);
}

bool WriteProductEnvironmentConfigurationString(const char *key,
						const char *value)
{
	return WriteEnvironmentConfigString(key, value, ENV_PRODUCT_NAME);
}

bool WriteProductEnvironmentConfigurationStrings(
	streamelements_env_update_requests requests)
{
	for (int i = 0; i < requests.size(); ++i) {
		requests[i].product = ENV_PRODUCT_NAME;
	}

	return WriteEnvironmentConfigStrings(requests);
}

/* ========================================================= */

#ifndef WIN32
#include <uuid/uuid.h>
#endif

std::string CreateGloballyUniqueIdString()
{
	std::string result;
#ifdef WIN32
	const int GUID_STRING_LENGTH = 39;

	GUID guid;
	CoCreateGuid(&guid);

	OLECHAR guidStr[GUID_STRING_LENGTH];
	StringFromGUID2(guid, guidStr, GUID_STRING_LENGTH);

	guidStr[GUID_STRING_LENGTH - 2] = 0;
	result = wstring_to_utf8(guidStr + 1);
#else
	uuid_t uuid;

	uuid_generate_time(uuid);

	char buf[128];
	uuid_unparse(uuid, buf);

	result = buf;
#endif

	return result;
}

#ifdef WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
std::string CreateCryptoSecureRandomNumberString()
{
	std::string result = "0";

	BCRYPT_ALG_HANDLE hAlgo;

	if (0 == BCryptOpenAlgorithmProvider(&hAlgo, BCRYPT_RNG_ALGORITHM, NULL,
					     0)) {
		uint64_t buffer;

		if (0 == BCryptGenRandom(hAlgo, (PUCHAR)&buffer, sizeof(buffer),
					 0)) {
			char buf[sizeof(buffer) * 2 + 1];
			sprintf_s(buf, sizeof(buf), "%llX", buffer);

			result = buf;

			std::cout << buffer << std::endl;
		}

		BCryptCloseAlgorithmProvider(hAlgo, 0);
	}

	return result;
}
#endif

#ifdef WIN32
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
// MacOS version implemented in StreamElementsUtils.mm
std::string GetComputerSystemUniqueId()
{
	const char *REG_VALUE_NAME = "MachineUniqueIdentifier";

	std::string result =
		ReadEnvironmentConfigString(REG_VALUE_NAME, nullptr);
	std::string prevResult = result;

	if (result.size()) {
		// Discard invalid values
		if (result ==
			    "WUID/03000200-0400-0500-0006-000700080009" || // Known duplicate
		    result ==
			    "WUID/00000000-0000-0000-0000-000000000000" || // Null value
		    result ==
			    "WUID/FFFFFFFF-FFFF-FFFF-FFFF-FFFFFFFFFFFF" || // Invalid value
		    result ==
			    "WUID/00412F4E-0000-0000-0000-0000FFFFFFFF") { // Set by russian MS Office crack
			result = "";
		}
	}

	if (!result.size()) {
		// Get unique ID from WMI

		HRESULT hr = CoInitialize(NULL);

		// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/initializing-com-for-a-wmi-application
		if (SUCCEEDED(hr)) {
			bool uinitializeCom = hr == S_OK;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/setting-the-default-process-security-level-using-c-
			CoInitializeSecurity(
				NULL, // security descriptor
				-1,   // use this simple setting
				NULL, // use this simple setting
				NULL, // reserved
				RPC_C_AUTHN_LEVEL_DEFAULT, // authentication level
				RPC_C_IMP_LEVEL_IMPERSONATE, // impersonation level
				NULL,      // use this simple setting
				EOAC_NONE, // no special capabilities
				NULL);     // reserved

			IWbemLocator *pLocator;

			// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
			hr = CoCreateInstance(CLSID_WbemLocator, 0,
					      CLSCTX_INPROC_SERVER,
					      IID_IWbemLocator,
					      (LPVOID *)&pLocator);

			if (SUCCEEDED(hr)) {
				IWbemServices *pSvc = 0;

				// https://docs.microsoft.com/en-us/windows/desktop/wmisdk/creating-a-connection-to-a-wmi-namespace
				hr = pLocator->ConnectServer(
					BSTR(L"root\\cimv2"), //namespace
					NULL,                 // User name
					NULL,                 // User password
					0,                    // Locale
					NULL,                 // Security flags
					0,                    // Authority
					0,                    // Context object
					&pSvc); // IWbemServices proxy

				if (SUCCEEDED(hr)) {
					hr = CoSetProxyBlanket(
						pSvc, RPC_C_AUTHN_WINNT,
						RPC_C_AUTHZ_NONE, NULL,
						RPC_C_AUTHN_LEVEL_CALL,
						RPC_C_IMP_LEVEL_IMPERSONATE,
						NULL, EOAC_NONE);

					if (SUCCEEDED(hr)) {
						IEnumWbemClassObject
							*pEnumerator = NULL;

						hr = pSvc->ExecQuery(
							(BSTR)L"WQL",
							(BSTR)L"select * from Win32_ComputerSystemProduct",
							WBEM_FLAG_FORWARD_ONLY,
							NULL, &pEnumerator);

						if (SUCCEEDED(hr)) {
							IWbemClassObject *pObj =
								NULL;

							ULONG resultCount;
							hr = pEnumerator->Next(
								WBEM_INFINITE,
								1, &pObj,
								&resultCount);

							if (SUCCEEDED(hr)) {
								VARIANT value;

								hr = pObj->Get(
									L"UUID",
									0,
									&value,
									NULL,
									NULL);

								if (SUCCEEDED(
									    hr)) {
									if (value.vt !=
									    VT_NULL) {
										result =
											std::string(
												"SWID/") +
											clean_guid_string(wstring_to_utf8(
												std::wstring(
													value.bstrVal)));
										result +=
											"-";
										result += clean_guid_string(
											CreateGloballyUniqueIdString());
										result +=
											"-";
										result +=
											CreateCryptoSecureRandomNumberString();
									}
									VariantClear(
										&value);
								}
							}

							pEnumerator->Release();
						}
					}

					pSvc->Release();
				}

				pLocator->Release();
			}

			if (uinitializeCom) {
				CoUninitialize();
			}
		}
	}

	if (!result.size()) {
		// Failed retrieving UUID, generate our own
		result = std::string("SEID/") +
			 clean_guid_string(CreateGloballyUniqueIdString());
		result += "-";
		result += CreateCryptoSecureRandomNumberString();
	}

	if (result.size() && result != prevResult) {
		// Save for future use
		WriteEnvironmentConfigString(REG_VALUE_NAME, result.c_str(),
					     nullptr);
	}

	return result;
}
#endif

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result)
{
	std::string s = input;

	while (s.size()) {
		std::string left;

		size_t offset = s.find('&');
		if (offset != std::string::npos) {
			left = s.substr(0, offset);
			s = s.substr(offset + 1);
		} else {
			left = s;
			s = "";
		}

		std::string right = "";
		offset = left.find('=');

		if (offset != std::string::npos) {
			right = left.substr(offset + 1);
			left = left.substr(0, offset);
		}

		result[left] = right;
	}

	return true;
}

std::string CreateSHA256Digest(std::string &input)
{
	std::vector<unsigned char> hash(picosha2::k_digest_size);
	picosha2::hash256(input.begin(), input.end(), hash.begin(), hash.end());

	return picosha2::bytes_to_hex_string(hash.begin(), hash.end());
}

static std::mutex s_session_message_signature_mutex;
static std::string s_session_message_signature_random = "";

std::string CreateSessionMessageSignature(std::string &message)
{
	if (!s_session_message_signature_random.size()) {
		std::lock_guard<std::mutex> guard(
			s_session_message_signature_mutex);
		if (!s_session_message_signature_random.size()) {
			s_session_message_signature_random =
				CreateCryptoSecureRandomNumberString();
		}
	}

	std::string digest_input = message + s_session_message_signature_random;

	return CreateSHA256Digest(digest_input);
}

bool VerifySessionMessageSignature(std::string &message, std::string &signature)
{
	std::string digest_input = message + s_session_message_signature_random;

	std::string digest = CreateSHA256Digest(digest_input);

	return digest == signature;
}

std::string CreateSessionSignedAbsolutePathURL(std::wstring path)
{
	CefURLParts parts;

	path = std::regex_replace(path, std::wregex(L"#"), L"%23");
	path = std::regex_replace(path, std::wregex(L"&"), L"%26");

	CefString(&parts.scheme) = "https";
	CefString(&parts.host) = "absolute";
	CefString(&parts.path) = std::wstring(L"/") + path;

	CefString url;
	CefCreateURL(parts, url);
	CefParseURL(url, parts);

	std::string message = CefString(&parts.path).ToString();

	message =
		CefURIDecode(message, true, cef_uri_unescape_rule_t::UU_SPACES);
	message = CefURIDecode(
		message, true,
		cef_uri_unescape_rule_t::
			UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

	message = message.erase(0, 1);

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return url.ToString() + std::string("?digest=") +
	       CreateSessionMessageSignature(message);
}

bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path)
{
	CefURLParts parts;

	if (!CefParseURL(CefString(url), parts)) {
		return false;
	} else {
		path = CefString(&parts.path).ToString();

		path = CefURIDecode(path, true,
				    cef_uri_unescape_rule_t::UU_SPACES);
		path = CefURIDecode(
			path, true,
			cef_uri_unescape_rule_t::
				UU_URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS);

		path = path.erase(0, 1);

		std::map<std::string, std::string> queryArgs;
		ParseQueryString(CefString(&parts.query), queryArgs);

		if (!queryArgs.count("digest")) {
			return false;
		} else {
			std::string signature = queryArgs["digest"];

			std::string message = path;

			//message = std::regex_replace(message, std::regex("#"), "%23");
			//message = std::regex_replace(message, std::regex("&"), "%26");

			return VerifySessionMessageSignature(message,
							     signature);
		}
	}
}

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window)
{
    if (!window) return false;
    
#ifdef WIN32
	DWORD exStyle = GetWindowLong((HWND)window->winId(), GWL_EXSTYLE);
	return (exStyle & WS_EX_TOPMOST) != 0;
#else
	return (window->windowFlags() & Qt::WindowStaysOnTopHint) != 0;
#endif
}

void SetAlwaysOnTop(QWidget *window, bool enable)
{
    if (!window) return;

#ifdef WIN32
	HWND hwnd = (HWND)window->winId();
	SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
	Qt::WindowFlags flags = window->windowFlags();

	if (enable)
		flags |= Qt::WindowStaysOnTopHint;
	else
		flags &= ~Qt::WindowStaysOnTopHint;

	window->setWindowFlags(flags);
	window->show();
#endif
}

double GetObsGlobalFramesPerSecond()
{
	config_t *basicConfig =
		obs_frontend_get_profile_config(); // does not increase refcount

	switch (config_get_uint(basicConfig, "Video", "FPSType")) {
	case 0: // Common
		return (double)atoi(
			config_get_string(basicConfig, "Video", "FPSCommon"));
		break;

	case 1: // Integer
		return (double)config_get_uint(basicConfig, "Video", "FPSInt");
		break;

	case 2: // Fractional
		return (double)config_get_uint(basicConfig, "Video", "FPSNum") /
		       (double)config_get_uint(basicConfig, "Video", "FPSDen");
		break;
	}
}

void AdviseHostUserInterfaceStateChanged()
{
	static std::mutex mutex;

	static QTimer *t = nullptr;

	std::lock_guard<std::mutex> guard(mutex);

	if (t == nullptr) {
		t = new QTimer();

		t->moveToThread(qApp->thread());
		t->setSingleShot(true);

		QObject::connect(t, &QTimer::timeout, [&]() {
			std::lock_guard<std::mutex> guard(mutex);

			t->deleteLater();
			t = nullptr;

			// Advise guest code of user interface state changes
			StreamElementsCefClient::DispatchJSEvent(
				"hostUserInterfaceStateChanged", "null");
		});
	}

	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
				  Q_ARG(int, 250));
}

void AdviseHostHotkeyBindingsChanged()
{
	static std::mutex mutex;

	static QTimer *t = nullptr;

	std::lock_guard<std::mutex> guard(mutex);

	if (t == nullptr) {
		t = new QTimer();

		t->moveToThread(qApp->thread());
		t->setSingleShot(true);

		QObject::connect(t, &QTimer::timeout, [&]() {
			std::lock_guard<std::mutex> guard(mutex);

			t->deleteLater();
			t = nullptr;

			// Advise guest code of user interface state changes
			StreamElementsCefClient::DispatchJSEvent(
				"hostHotkeyBindingsChanged", "null");
		});
	}

	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
				  Q_ARG(int, 250));
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

#if ENABLE_DECRYPT_COOKIES
#include "deps/sqlite/sqlite3.h"
#include <windows.h>
#include <wincrypt.h>
#include <dpapi.h>
#pragma comment(lib, "crypt32.lib")
void StreamElementsDecryptCefCookiesFile(const char *path_utf8)
{
#ifdef _WIN32
	sqlite3 *db;

	if (SQLITE_OK !=
	    sqlite3_open_v2(path_utf8, &db, SQLITE_OPEN_READWRITE, nullptr)) {
		blog(LOG_ERROR,
		     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': %s",
		     path_utf8, sqlite3_errmsg(db));

		sqlite3_close_v2(db);

		return;
	}

	std::vector<std::string> update_statements;

	const char *sql =
		"select * from cookies where value = '' and encrypted_value is NOT NULL";

	sqlite3_stmt *stmt;

	if (SQLITE_OK == sqlite3_prepare(db, sql, -1, &stmt, nullptr)) {
		std::unordered_map<std::string, int> columnNameMap;

		/* Map column names to their indexes */
		for (int colIndex = 0;; ++colIndex) {
			const char *col_name =
				sqlite3_column_name(stmt, colIndex);
			if (!col_name)
				break;

			columnNameMap[col_name] = colIndex;
		}

		auto getStringValue = [&](const char *colName) -> std::string {
			const int col_index = columnNameMap[colName];
			const unsigned char *val =
				sqlite3_column_text(stmt, col_index);

			if (!!val) {
				const int size =
					sqlite3_column_bytes(stmt, col_index);

				return std::string((const char *)val,
						   (size_t)size);
			}

			return std::string();
		};

		auto getIntValue = [&](const char *colName) -> int {
			const int col_index = columnNameMap[colName];
			return sqlite3_column_int(stmt, col_index);
		};

		if (columnNameMap.count("name") &&
		    columnNameMap.count("encrypted_value") &&
		    columnNameMap.count("host_key") &&
		    columnNameMap.count("path") &&
		    columnNameMap.count("priority")) {
			while (SQLITE_ROW == sqlite3_step(stmt)) {
				const int encrypted_value_index =
					columnNameMap["encrypted_value"];

				const void *encrypted_blob =
					sqlite3_column_blob(
						stmt, encrypted_value_index);
				const int encrypted_size = sqlite3_column_bytes(
					stmt, encrypted_value_index);

				if (!encrypted_size)
					continue;

				std::string host_key =
					getStringValue("host_key");
				std::string path = getStringValue("path");
				std::string name = getStringValue("name");
				int priority = getIntValue("priority");

				DATA_BLOB in;
				in.cbData = encrypted_size;
				in.pbData = (BYTE *)encrypted_blob;

				DATA_BLOB out;

				if (!CryptUnprotectData(&in, NULL, NULL, NULL,
							NULL, 0, &out))
					continue;

				std::string decrypted_value(
					(const char *)out.pbData,
					(size_t)out.cbData);
				::LocalFree(out.pbData);

				char *update_stmt = sqlite3_mprintf(
					"update cookies set value = %Q, encrypted_value = NULL where value = '' and host_key = %Q and name = %Q and priority = %d",
					decrypted_value.c_str(),
					host_key.c_str(), name.c_str(),
					priority);

				update_statements.push_back(update_stmt);

				sqlite3_free(update_stmt);
			}
		} else {
			blog(LOG_ERROR,
			     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s' cookies table is missing required columns",
			     path_utf8);
		}

		sqlite3_finalize(stmt);
	} else {
		blog(LOG_ERROR,
		     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': %s",
		     path_utf8, sqlite3_errmsg(db));
	}

	int done_count = 0;
	int error_count = 0;

	for (auto item : update_statements) {
		char *zErrMsg;

		if (SQLITE_OK != sqlite3_exec(db, item.c_str(), nullptr,
					      nullptr, &zErrMsg)) {
			blog(LOG_INFO,
			     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': sqlite3_exec(): %s: %s",
			     path_utf8, zErrMsg, item.c_str());

			sqlite3_free(zErrMsg);

			++error_count;
		} else {
			++done_count;
		}
	}

	sqlite3_close_v2(db);

	blog(LOG_INFO,
	     "obs-browser: StreamElementsDecryptCefCookiesFile: '%s': %d succeeded, %d failed",
	     path_utf8, done_count, error_count);
#endif
}

void StreamElementsDecryptCefCookiesStoragePath(const char *path_utf8)
{
	std::string file_path = path_utf8;

	file_path += "/Cookies";

	StreamElementsDecryptCefCookiesFile(file_path.c_str());
}
#endif /* ENABLE_DECRYPT_COOKIES */

std::string GetIdFromPointer(const void *ptr)
{
	char buf[32];

	sprintf_s(buf, "ptr(%p)", ptr);

	return buf;
}

const void *GetPointerFromId(const char *id)
{
	void *ptr;

	if (sscanf(id, "ptr(%p)", &ptr) > 0) {
		return ptr;
	} else {
		return nullptr;
	}
}

bool GetTemporaryFilePath(std::string prefixString, std::string &result)
{
#ifdef WIN32
	const size_t BUF_LEN = 2048;
	wchar_t *pathBuffer = new wchar_t[BUF_LEN];

	if (!::GetTempPathW(BUF_LEN, pathBuffer)) {
		delete[] pathBuffer;

		return false;
	}

	std::wstring wtempBufPath(pathBuffer);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	if (0 == ::GetTempFileNameW(wtempBufPath.c_str(),
				    myconv.from_bytes(prefixString).c_str(), 0,
				    pathBuffer)) {
		delete[] pathBuffer;

		return false;
	}

	wtempBufPath = pathBuffer;

	result = myconv.to_bytes(wtempBufPath);

	delete[] pathBuffer;

	return true;
#else
	static int serial = 0;

	++serial;

	result = "/tmp/";
	char pid_str[32];
	sprintf(pid_str, "%d", getpid());
	result += prefixString;
	result += ".";
	result += pid_str;
	result += ".";
	char serial_str[32];
	sprintf(serial_str, "%d", serial);
	result += serial_str;

	return true;
#endif
}

std::string GetUniqueFileNameFromPath(std::string path, size_t maxLength)
{
	std::string guid = clean_guid_string(CreateGloballyUniqueIdString());
	const char *ext_cstr = os_get_path_extension(path.c_str());
	std::string ext = ext_cstr ? ext_cstr : "";
	std::string result = std::regex_replace(
		path.substr(0, path.size() - ext.size()) + "_" + guid + ext,
		std::regex("[\\/: ]"), "_");

	if (maxLength > 0 && maxLength < result.size()) {
		return result.substr(result.size() - maxLength);
	} else {
		return result;
	}
}

std::string GetFolderPathFromFilePath(std::string filePath)
{
	std::string path(filePath);

	std::transform(path.begin(), path.end(), path.begin(), [](char ch) {
		if (ch == '\\')
			return '/';
		else
			return ch;
	});

	size_t pos = path.find_last_of('/');

	if (pos > 0)
		return path.substr(0, pos);
	else
		return ".";
}

bool ReadListOfObsSceneCollections(std::map<std::string, std::string> &output)
{
	char *basePathPtr = os_get_config_path_ptr("obs-studio/basic/scenes");
	std::string basePath = basePathPtr;
	bfree(basePathPtr);

	os_dir_t *dir = os_opendir(basePath.c_str());

	if (!dir)
		return false;

	struct os_dirent *entry;

	while ((entry = os_readdir(dir)) != NULL) {
		if (entry->directory || *entry->d_name == '.')
			continue;

		std::string fileName = entry->d_name;

		std::smatch match;

		if (!std::regex_search(fileName, match,
				       std::regex("^(.+?)\\.json$")))
			continue;

		std::string id = match[1].str();

		std::string filePath = basePath + "/" + fileName;

		char *content = os_quick_read_utf8_file(filePath.c_str());

		if (content) {
			CefRefPtr<CefValue> root =
				CefParseJSON(CefString(content),
					     JSON_PARSER_ALLOW_TRAILING_COMMAS);

			if (root.get() && root->GetType() == VTYPE_DICTIONARY) {
				CefRefPtr<CefDictionaryValue> d =
					root->GetDictionary();

				if (d->HasKey("name") &&
				    d->GetType("name") == VTYPE_STRING) {
					std::string name = d->GetString("name");

					output[id] = name;
				}
			}

			bfree(content);
		}
	}

	os_closedir(dir);

	return true;
}

bool ReadListOfObsProfiles(std::map<std::string, std::string> &output)
{
	char *basePathPtr = os_get_config_path_ptr("obs-studio/basic/profiles");
	std::string basePath = basePathPtr;
	bfree(basePathPtr);

	os_dir_t *dir = os_opendir(basePath.c_str());

	if (!dir)
		return false;

	struct os_dirent *entry;

	while ((entry = os_readdir(dir)) != NULL) {
		if (!entry->directory || *entry->d_name == '.')
			continue;

		std::string id = entry->d_name;

		std::string filePath = basePath + "/" + id + "/basic.ini";

		config_t *ini;

		if (config_open(&ini, filePath.c_str(), CONFIG_OPEN_EXISTING) ==
		    CONFIG_SUCCESS) {
			const char *value =
				config_get_string(ini, "General", "Name");

			if (value) {
				std::string name = value;

				output[id] = name;
			}

			config_close(ini);
		}
	}

	os_closedir(dir);

	return true;
}

static class LocalCefURLRequestClient : public CefURLRequestClient {
public:
	LocalCefURLRequestClient(cef_http_request_callback_t callback)
		: m_callback(callback)
	{
	}

	virtual ~LocalCefURLRequestClient() {}

public:
	virtual bool
	GetAuthCredentials(bool isProxy, const CefString &host, int port,
			   const CefString &realm, const CefString &scheme,
			   CefRefPtr<CefAuthCallback> callback) override
	{
		return false;
	}

	virtual void OnDownloadData(CefRefPtr<CefURLRequest> request,
				    const void *data,
				    size_t data_length) override;

	virtual void OnUploadProgress(CefRefPtr<CefURLRequest> request,
				      int64 current, int64 total) override
	{
	}

	virtual void OnDownloadProgress(CefRefPtr<CefURLRequest> request,
					int64 current, int64 total)
	{
	}

	virtual void
	OnRequestComplete(CefRefPtr<CefURLRequest> request) override;

private:
	std::vector<char> m_buffer;
	cef_http_request_callback_t m_callback;

public:
	IMPLEMENT_REFCOUNTING(LocalCefURLRequestClient);
};

void LocalCefURLRequestClient::OnDownloadData(CefRefPtr<CefURLRequest> request,
					      const void *data,
					      size_t data_length)
{
	m_buffer.insert(m_buffer.end(), (char *)data,
			(char *)data + data_length);
}

void LocalCefURLRequestClient::OnRequestComplete(
	CefRefPtr<CefURLRequest> request)
{
	if (request->GetRequestStatus() == UR_CANCELED)
		return;

	if (request->GetRequestStatus() == UR_SUCCESS) {
		// Success

		m_callback(true, m_buffer.data(), m_buffer.size());
	} else {
		// Failure
		m_callback(false, nullptr, 0);
	}
}

CefRefPtr<CefCancelableTask> QueueCefCancelableTask(std::function<void()> task)
{
	CefRefPtr<CefCancelableTask> result = new CefCancelableTask(task);

	CefPostTask(TID_UI, result);

	return result;
}

CefRefPtr<CefCancelableTask>
CefHttpGetAsync(const char *url,
		std::function<void(CefRefPtr<CefURLRequest>)> init_callback,
		cef_http_request_callback_t callback)
{
	CefRefPtr<CefRequest> request = CefRequest::Create();

	request->SetURL(url);
	request->SetMethod("GET");

	CefRefPtr<LocalCefURLRequestClient> client =
		new LocalCefURLRequestClient(callback);

	extern bool QueueCEFTask(std::function<void()> task);

	return QueueCefCancelableTask([=]() -> void {
		CefRefPtr<CefURLRequest> cefRequest = CefURLRequest::Create(
			request, client,
			StreamElementsGlobalStateManager::GetInstance()
				->GetCookieManager()
				->GetCefRequestContext());

		init_callback(cefRequest);
	});
}

static class QRemoteIconMenu : public QMenu {
public:
	QRemoteIconMenu(const char *iconUrl, QPixmap *defaultPixmap = nullptr)
		: QMenu(),
		  loader(StreamElementsRemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		QObject::connect(this, &QObject::destroyed,
				 [this]() { loader->Cancel(); });
	}

private:
	CefRefPtr<StreamElementsRemoteIconLoader> loader;
};

static class QRemoteIconAction : public QAction {
public:
	QRemoteIconAction(const char *iconUrl, QPixmap *defaultPixmap = nullptr)
		: QAction(),
		  loader(StreamElementsRemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		QObject::connect(this, &QObject::destroyed,
				 [this]() { loader->Cancel(); });
	}

private:
	CefRefPtr<StreamElementsRemoteIconLoader> loader;
};

static class QRemoteIconPushButton : public QPushButton {
public:
	QRemoteIconPushButton(const char *iconUrl,
			      QPixmap *defaultPixmap = nullptr)
		: QPushButton(),
		  loader(StreamElementsRemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		setMouseTracking(true);

		setStyleSheet("background: none; padding: 0;");

		setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Minimum);

		CefRefPtr<StreamElementsRemoteIconLoader> loaderCopy = loader;

		loaderCopy->AddRef();
		QObject::connect(this, &QObject::destroyed, [loaderCopy]() {
			loaderCopy->Cancel();
			loaderCopy->Release();
		});
	}

	virtual QSize minimumSizeHint() const override { return QSize(16, 16); }
	virtual QSize sizeHint() const override
	{
		if (text().size()) {
			return QPushButton::sizeHint();
		} else {
			return QSize(16, 16);
		}
	}

private:
	CefRefPtr<StreamElementsRemoteIconLoader> loader;
};

bool DeserializeDocksMenu(QMenu& menu)
{
	StreamElementsGlobalStateManager::GetInstance()
		->GetWidgetManager()
		->EnterCriticalSection();

	std::vector<std::string> widgetIds;
	StreamElementsGlobalStateManager::GetInstance()
		->GetWidgetManager()
		->GetDockBrowserWidgetIdentifiers(widgetIds);

	std::vector<StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *>
		widgets;
	for (auto id : widgetIds) {
		auto info = StreamElementsGlobalStateManager::GetInstance()
				    ->GetWidgetManager()
				    ->GetDockBrowserWidgetInfo(id.c_str());

		if (info) {
			widgets.push_back(info);
		}
	}

	std::sort(
		widgets.begin(), widgets.end(),
		[](StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *a,
		   StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo
			   *b) { return a->m_title < b->m_title; });

	StreamElementsGlobalStateManager::GetInstance()
		->GetWidgetManager()
		->LeaveCriticalSection();

	for (auto widget : widgets) {
		// widget->m_visible
		QAction *widget_action =
			new QAction(QString(widget->m_title.c_str()));
		menu.addAction(widget_action);

		std::string id = widget->m_id;
		bool isVisible = widget->m_visible;

		widget_action->setCheckable(true);
		widget_action->setChecked(isVisible);

		QObject::connect(widget_action, &QAction::triggered, [id, isVisible, widget_action] {
			QDockWidget *dock =
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->GetDockWidget(id.c_str());

			if (dock) {
				if (isVisible) {
					// Hide
					StreamElementsGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackDockWidgetEvent(
							dock, "Hide",
							json11::Json::object{
								{"actionSource",
								 "Menu"}});
				} else {
					// Show
					StreamElementsGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackDockWidgetEvent(
							dock, "Show",
							json11::Json::object{
								{"actionSource",
								 "Menu"}});
				}

				dock->setVisible(!isVisible);

				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()->Update();
			}
		});
	}

	for (auto widget : widgets) {
		delete widget;
	}

	return true;
}

bool DeserializeMenu(CefRefPtr<CefValue> input, QMenu &menu,
		     std::function<void()> defaultAction,
		     std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_LIST)
		return false;

	auto getIconUrl =
		[](CefRefPtr<CefDictionaryValue> parent) -> std::string {
		if (!parent->HasKey("icon") ||
		    parent->GetType("icon") != VTYPE_DICTIONARY)
			return "";

		CefRefPtr<CefDictionaryValue> d = parent->GetDictionary("icon");

		if (!d->HasKey("url") || d->GetType("url") != VTYPE_STRING)
			return "";

		std::string url = d->GetString("url").ToString();

		return url;
	};

	CefRefPtr<CefListValue> list = input->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_DICTIONARY)
			return false;

		CefRefPtr<CefDictionaryValue> d = list->GetDictionary(index);

		if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
			return false;

		std::string type = d->GetString("type");

		if (type == "separator") {
			menu.addSeparator();
		} else if (type == "command") {
			bool enabled = true;

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				return false;

			if (enabled) {
				if (!d->HasKey("invoke") ||
				    d->GetType("invoke") != VTYPE_STRING)
					return false;
			}

			std::string title = d->GetString("title");

			std::string iconUrl = getIconUrl(d);

			QAction *auxAction = new QRemoteIconAction(
				iconUrl.size() ? iconUrl.c_str() : nullptr);

			auxAction->setText(title.c_str());
			auxAction->setEnabled(enabled);

			menu.addAction(auxAction);

			CefRefPtr<CefValue> action = CefValue::Create();
			action->SetDictionary(d->Copy(false));

			auxAction->connect(
				auxAction, &QAction::triggered,
				[action, defaultAction, defaultContextMenu]() {
					DeserializeAndInvokeAction(
						action, defaultAction,
						defaultContextMenu);

					return true;
				});
		} else if (type == "container") {
			bool enabled = true;

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				return false;

			std::string iconUrl = getIconUrl(d);

			if (d->HasKey("items") &&
			    d->GetType("items") == VTYPE_LIST) {
				QMenu *submenu = new QRemoteIconMenu(
					iconUrl.size() ? iconUrl.c_str()
						       : nullptr);

				submenu->setTitle(d->GetString("title")
							  .ToString()
							  .c_str());

				submenu->setEnabled(enabled);

				menu.addMenu(submenu);

				if (!DeserializeMenu(d->GetValue("items"),
						     *submenu))
					return false;
			} else if (d->HasKey("itemsSource") && d->GetType("itemsSource") == VTYPE_STRING) {
				std::string itemsSource =
					d->GetString("itemsSource");

				if (itemsSource == ":dockingWidgets") {
					QMenu *submenu = new QRemoteIconMenu(
						iconUrl.size() ? iconUrl.c_str()
							       : nullptr);

					submenu->setTitle(d->GetString("title")
								  .ToString()
								  .c_str());

					submenu->setEnabled(enabled);

					menu.addMenu(submenu);

					if (!DeserializeDocksMenu(*submenu))
						return false;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else {
			return false;
		}
	}

	return true;
}

QWidget *
DeserializeAuxiliaryControlWidget(CefRefPtr<CefValue> input,
				  std::function<void()> defaultAction,
				  std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
		return nullptr;

	std::string type = d->GetString("type");

	std::string iconUrl = "";

	if (d->HasKey("icon") && d->GetType("icon") == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> m_icon = d->GetDictionary("icon");

		if (m_icon->HasKey("url") &&
		    m_icon->GetType("url") == VTYPE_STRING) {
			iconUrl = m_icon->GetString("url");
		}
	}

	std::string styleSheet =
		"QToolTip { color: #eeeeee; background-color: #000000; } ";

	if (d->HasKey("color") && d->GetType("color") == VTYPE_STRING) {
		styleSheet += "QPushButton { color: ";
		styleSheet += d->GetString("color").ToString() + ";";
		styleSheet += " }";
	}

	if (type == "container") {
		if (!d->HasKey("items") || d->GetType("items") != VTYPE_LIST)
			return nullptr;

		CefRefPtr<CefValue> items = d->GetValue("items")->Copy();

		QRemoteIconPushButton *control =
			new QRemoteIconPushButton(iconUrl.c_str());

		//control->setContentsMargins(0, 0, 0, 0);
		//control->setMinimumSize(16, 16);
		control->setSizePolicy(QSizePolicy::Minimum,
				       QSizePolicy::Minimum);

		auto HandleClick = [items, control](bool /*checked*/) {
			QMenu menu;

			if (!DeserializeMenu(items, menu))
				return;

			// menu.exec(control->mapToGlobal(QPoint(0, 0)));
			menu.exec(QCursor::pos());
		};

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());
		}

		if (d->HasKey("tooltip") &&
		    d->GetType("tooltip") == VTYPE_STRING) {
			std::string tooltip =
				d->GetString("tooltip").ToString();

			control->setToolTip(tooltip.c_str());
		}

		control->setStyleSheet(styleSheet.c_str());

		QObject::connect(control, &QPushButton::clicked, HandleClick);

		return control;
	} else if (type == "command") {
		if (!d->HasKey("invoke") ||
		    d->GetType("invoke") != VTYPE_STRING)
			return nullptr;

		CefRefPtr<CefValue> action = input->Copy();

		auto HandleClick = [action, defaultAction,
				    defaultContextMenu](bool /*checked*/) {
			DeserializeAndInvokeAction(action, defaultAction,
						   defaultContextMenu);
		};

		QRemoteIconPushButton *control =
			new QRemoteIconPushButton(iconUrl.c_str());

		//control->setContentsMargins(0, 0, 0, 0);
		control->setMinimumSize(16, 16);
		control->setSizePolicy(QSizePolicy::Minimum,
				       QSizePolicy::Minimum);

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());
		}

		if (d->HasKey("tooltip") &&
		    d->GetType("tooltip") == VTYPE_STRING) {
			std::string tooltip =
				d->GetString("tooltip").ToString();

			control->setToolTip(tooltip.c_str());
		}

		control->setStyleSheet(styleSheet.c_str());

		QObject::connect(control, &QPushButton::clicked, HandleClick);

		return control;
	} else if (type == "separator") {
		QWidget *control = new QWidget();
		control->setContentsMargins(0, 0, 0, 0);
		control->setStyleSheet("background: none");
		control->setFixedSize(2, 2);

		return control;
	}

	return nullptr;
}

QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
				     QPixmap *defaultPixmap)
{
	std::string iconUrl = "";

	if (input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (d->HasKey("url") && d->GetType("url") == VTYPE_STRING) {
			iconUrl = d->GetString("url").ToString();
		}
	}

	return new QRemoteIconPushButton(iconUrl.c_str(), defaultPixmap);
}

bool DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				std::function<void()> defaultAction,
				std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("invoke") || d->GetType("invoke") != VTYPE_STRING)
		return false;

	std::string invoke = d->GetString("invoke");

	CefRefPtr<CefListValue> invokeArgs = CefListValue::Create();

	if (d->HasKey("invokeArgs") && d->GetType("invokeArgs") == VTYPE_LIST)
		invokeArgs = d->GetList("invokeArgs")->Copy();

	if (invoke == ":defaultAction") {
		defaultAction();

		return true;
	} else if (invoke == ":defaultContextMenu") {
		defaultContextMenu();

		return true;
	} else if (invoke == ":none") {
		// No action
		return true;
	} else {
		return StreamElementsApiMessageHandler::InvokeHandler::
			GetInstance()
				->InvokeApiCallAsync(invoke, invokeArgs,
						     [](CefRefPtr<CefValue>) {
						     });
	}
}

void ObsSceneEnumAllItems(obs_scene_t *scene,
			  std::function<bool(obs_sceneitem_t *)> func)
{
	struct local_context {
		std::vector<obs_sceneitem_t *> items;
	};

	local_context pass1_context;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
		   void *param) {
			local_context *context = (local_context *)param;

			obs_sceneitem_addref(sceneitem);

			context->items.push_back(sceneitem);

			return true;
		},
		&pass1_context);

	local_context pass2_context;

	for (auto item : pass1_context.items) {
		pass2_context.items.push_back(item);

		if (obs_sceneitem_is_group(item)) {
			obs_sceneitem_group_enum_items(
				item,
				[](obs_scene_t *scene,
				   obs_sceneitem_t *sceneitem, void *param) {
					local_context *context =
						(local_context *)param;

					obs_sceneitem_addref(sceneitem);

					context->items.push_back(sceneitem);

					return true;
				},
				&pass2_context);
		}
	}

	bool keepCalling = true;

	for (auto item : pass2_context.items) {
		if (keepCalling) {
			keepCalling = func(item);
		}

		obs_sceneitem_release(item);
	}
}

void ObsSceneEnumAllItems(obs_source_t *source,
			  std::function<bool(obs_sceneitem_t *)> func)
{
	if (!source)
		return;

	obs_scene_t *scene =
		obs_scene_from_source(source); // does not increment refcount

	ObsSceneEnumAllItems(scene, func);
}

void ObsCurrentSceneEnumAllItems(std::function<bool(obs_sceneitem_t *)> func)
{
	obs_source_t *sceneSource = obs_frontend_get_current_scene();

	if (!sceneSource)
		return;

	ObsSceneEnumAllItems(sceneSource, func);

	obs_source_release(sceneSource);
}

bool IsCefValueEqual(CefRefPtr<CefValue> a, CefRefPtr<CefValue> b)
{
	std::string json1 = CefWriteJSON(a, JSON_WRITER_DEFAULT);
	std::string json2 = CefWriteJSON(a, JSON_WRITER_DEFAULT);

	return json1 == json2;
}

void ObsEnumAllScenes(std::function < bool(obs_source_t * scene)> func)
{
	struct local_context {
		std::vector<obs_source_t *> list;
	};

	local_context context;

	obs_enum_scenes(
		[](void *data, obs_source_t *scene) -> bool {
			local_context *context = (local_context *)data;

			
			if (!obs_source_is_group(scene)) {
				obs_source_addref(scene);

				context->list.push_back(scene);
			}

			return true;
		},
		&context);

	for (auto scene : context.list) {
		if (!func(scene))
			break;
	}

	for (auto scene : context.list) {
		obs_source_release(scene);
	}
}

class TimedObsApiTransactionHandle {
private:
	static std::map<std::string, TimedObsApiTransactionHandle *> s_map;
	static std::recursive_mutex s_mutex;

public:
	static std::string Create(int timeoutMilliseconds = 60000)
	{
		std::lock_guard<std::recursive_mutex> guard(s_mutex);

		std::string id = CreateGloballyUniqueIdString();

		s_map[id] = new TimedObsApiTransactionHandle(id, timeoutMilliseconds,
						       [id]() { Destroy(id); });

		if (s_map.size() == 1) {
			StreamElementsPleaseWaitWindow::GetInstance()->Show();

			obs_frontend_defer_save_begin();
		}

		return id;
	}

	static void Destroy(std::string id) {
		std::lock_guard<std::recursive_mutex> guard(s_mutex);

		if (!s_map.count(id))
			return;

		delete s_map[id];

		s_map.erase(id);

		if (s_map.size() == 0) {
			obs_frontend_defer_save_end();

			StreamElementsPleaseWaitWindow::GetInstance()->Hide();
		}
	}

private:
	TimedObsApiTransactionHandle(
		std::string id,
		int timeoutMilliseconds, std::function<void()> onTimer)
		: m_id(id), m_onTimer(onTimer)
	{
		m_timer = new QTimer();
		m_timer->moveToThread(qApp->thread());
		m_timer->setInterval(timeoutMilliseconds);
		m_timer->setSingleShot(true);
		QObject::connect(m_timer, &QTimer::timeout, [this]() {
			m_onTimer();
		});
		QMetaObject::invokeMethod(m_timer, "start",
					  Qt::QueuedConnection,
					  Q_ARG(int, timeoutMilliseconds));
	}

	~TimedObsApiTransactionHandle() {
		QMetaObject::invokeMethod(m_timer, "stop",
					  Qt::QueuedConnection, Q_ARG(int, 0));

		m_timer->deleteLater();
	}

private:
	std::string m_id;
	std::function<void()> m_onTimer;
	QTimer *m_timer;
};

std::map<std::string, TimedObsApiTransactionHandle *> TimedObsApiTransactionHandle::s_map;
std::recursive_mutex TimedObsApiTransactionHandle::s_mutex;

std::string CreateTimedObsApiTransaction(int timeoutMilliseconds) {
	return TimedObsApiTransactionHandle::Create(timeoutMilliseconds);
}

void CompleteTimedObsApiTransaction(std::string id) {
	TimedObsApiTransactionHandle::Destroy(id);
}

/* ========================================================= */

static bool GetBool(CefRefPtr<CefDictionaryValue> input, std::string key,
		    bool defaultValue = false)
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_BOOL)
		return defaultValue;

	return input->GetBool(key);
}

static int GetInt(CefRefPtr<CefDictionaryValue> input, std::string key,
		    int defaultValue = 0)
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_INT)
		return defaultValue;

	return input->GetInt(key);
}

static std::string GetString(CefRefPtr<CefDictionaryValue> input, std::string key,
		  std::string defaultValue = "")
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_STRING)
		return defaultValue;

	return input->GetString(key).ToString();
}

static std::wstring GetWString(CefRefPtr<CefDictionaryValue> input,
			     std::string key, std::wstring defaultValue = L"")
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_STRING)
		return defaultValue;

	return input->GetString(key).ToWString();
}

static cef_event_flags_t DeserializeCefEventModifiers(CefRefPtr<CefValue> input) {
	if (input->GetType() != VTYPE_DICTIONARY)
		return EVENTFLAG_NONE;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	uint32 result = EVENTFLAG_NONE;

	if (GetBool(d, "altKey"))
		result |= EVENTFLAG_ALT_DOWN;
	if (GetBool(d, "ctrlKey"))
		result |= EVENTFLAG_CONTROL_DOWN;
	if (GetBool(d, "metaKey"))
		result |= EVENTFLAG_COMMAND_DOWN;
	if (GetBool(d, "shiftKey"))
		result |= EVENTFLAG_SHIFT_DOWN;
	if (GetBool(d, "capsLock"))
		result |= EVENTFLAG_CAPS_LOCK_ON;
	if (GetBool(d, "numLock"))
		result |= EVENTFLAG_NUM_LOCK_ON;
	if (GetBool(d, "primaryMouseButton"))
		result |= EVENTFLAG_LEFT_MOUSE_BUTTON;
	if (GetBool(d, "secondaryMouseButton"))
		result |= EVENTFLAG_RIGHT_MOUSE_BUTTON;
	if (GetBool(d, "auxiliaryMouseButton"))
		result |= EVENTFLAG_MIDDLE_MOUSE_BUTTON;

	switch (GetInt(d, "location", -1)) {
	case 0x00: // DOM_KEY_LOCATION_STANDARD
		// No special mod
		break;
	case 0x01: // DOM_KEY_LOCATION_LEFT
		result |= EVENTFLAG_IS_LEFT;
		break;
	case 0x02: // DOM_KEY_LOCATION_RIGHT
		result |= EVENTFLAG_IS_RIGHT;
		break;
	case 0x03: // DOM_KEY_LOCATION_NUMPAD
		result |= EVENTFLAG_IS_KEY_PAD;
		break;
	default:
		std::string location = GetString(d, "location");

		if (location == "left") {
			result |= EVENTFLAG_IS_LEFT;
		} else if (location == "right") {
			result |= EVENTFLAG_IS_RIGHT;
		} else if (location == "numPad" || location == "keyPad") {
			result |= EVENTFLAG_IS_KEY_PAD;
		}
		break;
	}

	return (cef_event_flags_t)result;
}

bool DeserializeCefMouseEvent(CefRefPtr<CefValue> input, CefMouseEvent &output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	output.Reset();
	output.modifiers = DeserializeCefEventModifiers(input);
	output.x = GetInt(d, "x");
	output.y = GetInt(d, "y");

	return true;
}

bool DeserializeCefKeyEvent(CefRefPtr<CefValue> input, CefKeyEvent& output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	int native_vkey_code = GetInt(d, "code", -1);
	std::wstring key = GetWString(d, "key");
	int charCode = GetInt(d, "charCode", -1);
	if (charCode < 0 && !key.empty())
		charCode = key[0];

	output.Reset();
	output.modifiers = DeserializeCefEventModifiers(input);
	output.native_key_code = 0;
	output.windows_key_code = native_vkey_code >= 0 ? native_vkey_code : charCode;
	output.character = charCode;

	std::string type = GetString(d, "type");
	if (type == "rawkeydown") {
		output.type = KEYEVENT_RAWKEYDOWN;
	} else if (type == "keydown") {
		output.type = KEYEVENT_KEYDOWN;
	} else if (type == "keyup") {
		output.type = KEYEVENT_KEYUP;
	} else if (type == "keypress") {
		output.type = KEYEVENT_CHAR;
	} else {
		return false;
	}

	return true;
}

bool DeserializeCefMouseButtonType(CefRefPtr<CefValue> input,
				   CefBrowserHost::MouseButtonType &output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	std::string button = GetString(d, "button");

	if (button == "left") {
		output = MBT_LEFT;
	} else if (button == "right") {
		output = MBT_RIGHT;
	} else if (button == "auxiliary" || button == "middle") {
		output = MBT_MIDDLE;
	} else {
		return false;
	}

	return true;
}

bool DeserializeCefMouseEventCount(CefRefPtr<CefValue> input, int& output) {
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	output = GetInt(d, "eventCount", 1);

	if (output < 1)
		output = 1;

	return true;
}

bool DeserializeCefMouseEventType(CefRefPtr<CefValue> input,
				  CefMouseEventType &output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	std::string type = GetString(d, "type");

	if (type == "mousedown") {
		output = Down;
	} else if (type == "mouseup") {
		output = Up;
	} else if (type == "mousemove") {
		output = Move;
	} else if (type == "wheel") {
		output = Wheel;
	}

	return true;
}

bool DeserializeCefMouseWheelEventArgs(CefRefPtr<CefValue> input,
				       CefMouseWheelEventArgs &output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	output.deltaX = GetInt(d, "deltaX", 0);
	output.deltaY = GetInt(d, "deltaY", 0);

	if (output.deltaX < 0)
		output.deltaX = 0;

	if (output.deltaY < 0)
		output.deltaY = 0;

	return output.deltaX > 0 || output.deltaY > 0;
}

#ifdef WIN32
void RestartCurrentApplication()
{
	bool success = false;

	QProcess proc;
	if (proc.startDetached(
		QCoreApplication::instance()->applicationFilePath(),
		QCoreApplication::instance()->arguments()
	)) {
		success = true;

		/* Exit OBS */

		/* This is not the nicest way to terminate our own process,
			* yet, given that we are not looking for a clean shutdown
			* but will rather overwrite settings files, this is
			* acceptable.
			*
			* It is also likely to overcome any shutdown issues OBS
			* might have, and which appear from time to time. We definitely
			* do NOT want those attributed to Cloud Restore.
			*/
		
		::exit(0);
		QApplication::quit();
	}
}
#endif
