#include <string.h>

#include "StreamElementsLocalFilesystemHttpServer.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsRemoteIconLoader.hpp"
#include "StreamElementsPleaseWaitWindow.hpp"
#include "Version.hpp"
#include "wide-string.hpp"
#include "deps/utf8.h"

#define GLOBAL_ENV_CONFIG_FILE_NAME "obs-studio/streamelements-env.ini"

#include <cstdint>
#include <vector>
#include <regex>
#include <unordered_map>
#include <filesystem>

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

#include <sys/syscall.h>
#endif

/* ========================================================= */

static const char *ENV_PRODUCT_NAME = "OBS.Live";

/* ========================================================= */

static config_t *obs_fe_user_config()
{
	auto config = StreamElementsConfig::GetInstance();

	if (config) {
		return config->GetObsUserConfig();
	}

	return nullptr;
}

/* ========================================================= */

static inline const char *safe_str(const char *s)
{
	if (s == NULL)
		return "(NULL)";
	else
		return s;
}

/* ========================================================= */

bool IsTraceLogLevel() {
	static bool has_result = false;
	static bool result = false;

	if (!has_result) {
		std::string search = "--setrace";

		QStringList args = QCoreApplication::instance()->arguments();

		for (int i = 0; i < args.size() && !has_result; ++i) {
			std::string arg = args.at(i).toStdString();

			if (arg.substr(0, search.size()) == search) {
				result = true;
				break;
			}
		}
	}

	has_result = true;

	return result;
}

/* ========================================================= */

// convert wstring to UTF-8 string
std::string wstring_to_utf8(const std::wstring wstr)
{
	std::string utf8line;

	if (wstr.empty())
		return utf8line;

#ifdef _MSC_VER
	utf8::utf16to8(wstr.begin(), wstr.end(), std::back_inserter(utf8line));
#else
	utf8::utf32to8(wstr.begin(), wstr.end(), std::back_inserter(utf8line));
#endif
	return utf8line;
}

std::wstring utf8_to_wstring(const std::string str)
{
	std::wstring wide_line;

	if (str.empty())
		return wide_line;

#ifdef _MSC_VER
	utf8::utf8to16(str.begin(), str.end(), std::back_inserter(wide_line));
#else
	utf8::utf8to32(str.begin(), str.end(), std::back_inserter(wide_line));
#endif
	return wide_line;
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

StreamElementsApiContext_t s_apiContext;
std::shared_mutex s_apiContextMutex;

void GetApiContext(std::function<void(StreamElementsApiContext_t*)> callback)
{
	std::shared_lock lock(s_apiContextMutex);

	callback(&s_apiContext);
}

std::shared_ptr<StreamElementsApiContextItem> PushApiContext(CefString method, CefRefPtr<CefListValue> args)
{
	std::unique_lock lock(s_apiContextMutex);

	auto item =
		std::make_shared<StreamElementsApiContextItem>(method, args,
#ifdef _WIN32
							       GetCurrentThreadId()
#else
							       uint32_t(syscall(SYS_thread_selfid))
#endif
							       );

	s_apiContext.push_back(item);

	return item;
}

void RemoveApiContext(std::shared_ptr<StreamElementsApiContextItem> item)
{
	std::unique_lock lock(s_apiContextMutex);

	s_apiContext.remove(item);
}

/* ========================================================= */

static StreamElementsAsyncCallContextStack_t s_asyncCallContextStack;
static std::shared_mutex s_asyncCallContextStackMutex;

void GetAsyncCallContextStack(std::function<void(const StreamElementsAsyncCallContextStack_t *)> callback)
{
	std::shared_lock lock(s_asyncCallContextStackMutex);

	callback(&s_asyncCallContextStack);
}

std::shared_ptr<StreamElementsAsyncCallContextItem>
AsyncCallContextPush(std::string file, int line, bool running)
{
	std::unique_lock lock(s_asyncCallContextStackMutex);

	auto item = std::make_shared<StreamElementsAsyncCallContextItem>(
		file, line, running);

	s_asyncCallContextStack.push_back(item);

	return item;
}

void AsyncCallContextRemove(
	std::shared_ptr<StreamElementsAsyncCallContextItem> item)
{
	std::unique_lock lock(s_asyncCallContextStackMutex);

	s_asyncCallContextStack.remove(item);
}

std::future<void> __QtDelayTask_Impl(std::function<void()> task, int delayMs,
				     const char *file, const int line)
{
	auto promise = std::make_shared<std::promise<void>>();

	auto t = new QTimer();

	t->moveToThread(qApp->thread());
	t->setSingleShot(true);

	auto item = AsyncCallContextPush(file, line, false);

	QObject::connect(t, &QTimer::timeout, [=]() {
		t->deleteLater();

		item->running = true;

		task();

		AsyncCallContextRemove(item);

		promise->set_value();
	});

	QMetaObject::invokeMethod(t, "start", Qt::QueuedConnection,
				  Q_ARG(int, delayMs));

	return promise->get_future();
}

std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line)
{
	std::shared_ptr<std::promise<void>> promise =
		std::make_shared<std::promise<void>>();

	auto item = AsyncCallContextPush(file, line, false);

	auto executor = [=]() {
		task();

		item->running = true;

		AsyncCallContextRemove(item);

		promise->set_value();
	};

	QMetaObject::invokeMethod(qApp, executor, Qt::QueuedConnection);

	return promise->get_future();
}

std::future<void> __QtExecSync_Impl(std::function<void()> task,
				    std::string file, int line)
{
	if (QThread::currentThread() == qApp->thread()) {
		task();

		std::promise<void> promise;
		promise.set_value();
		return promise.get_future();
	} else {
		std::future<void> result = __QtPostTask_Impl(task, file, line);

		result.wait();

		return result;
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

bool DeserializeObsSourceFilters(obs_source_t* source, CefRefPtr<CefValue> filtersValue)
{
	if (!filtersValue.get())
		return false;

	if (filtersValue->GetType() != VTYPE_LIST)
		return false;

	auto filtersList = filtersValue->GetList();

	std::map<int, OBSSourceAutoRelease> filtersMap;
	int maxOrder = -1;
	std::vector<int> orderIndexes;

	for (size_t i = 0; i < filtersList->GetSize(); ++i) {
		if (filtersList->GetType(i) != VTYPE_DICTIONARY)
			return false;

		auto d = filtersList->GetDictionary(i);

		if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
			return false;

		std::string sourceType = d->GetString("class");
		std::string sourceName =
			d->HasKey("name") && d->GetType("name") == VTYPE_STRING
				? d->GetString("name")
				: obs_source_get_display_name(
					  sourceType.c_str());

		int order = d->HasKey("order") &&
					    d->GetType("order") == VTYPE_INT
				    ? order = d->GetInt("order")
				    : filtersList->GetSize() - i - 1;

		if (order < 0)
			return false;

		if (order > maxOrder)
			maxOrder = order;

		if (filtersMap.count(order))
			return false; // duplicate "order" value

		orderIndexes.push_back(order);

		OBSDataAutoRelease settings = obs_data_create();

		if (d->HasKey("settings") &&
		    d->GetType("settings") == VTYPE_DICTIONARY) {
			if (!DeserializeObsData(d->GetValue("settings"),
						settings))
				return false;
		}

		filtersMap[order] = obs_source_create_private(
			sourceType.c_str(), sourceName.c_str(), settings);

		if (!filtersMap[order])
			return false;
	}

	// Sort order indexes ascending
	std::sort(orderIndexes.begin(), orderIndexes.end());

	// Remove existing filters if any
	obs_source_enum_filters(
		source,
		[](obs_source_t *source, obs_source_t *filter, void *params) {
			obs_source_filter_remove(source, filter);
		},
		nullptr);

	// Add new filters
	for (auto i : orderIndexes) {
		obs_source_filter_add(source, filtersMap[i]);
	}

	// Set new filters order
	for (auto i : orderIndexes) {
		obs_source_filter_set_index(source, filtersMap[i], i);
	}

	return true;
}

CefRefPtr<CefListValue> SerializeObsSourceFilters(obs_source_t *source,
						  bool serializeProperties)
{
	if (!source) return CefListValue::Create();

	struct local_filters_context_t {
		CefRefPtr<CefListValue> items = CefListValue::Create();
		std::vector<OBSSourceAutoRelease> filters;
	};

	local_filters_context_t context;
	obs_source_enum_filters(
		source,
		[](obs_source_t *source, obs_source_t *filter,
		   void *params) -> void {
			auto context =
				static_cast<local_filters_context_t *>(params);

			context->filters.push_back(obs_source_get_ref(filter));
		},
		&context);

	for (size_t i = 0; i < context.filters.size(); ++i) {
		auto d = CefDictionaryValue::Create();

		SerializeObsSource(context.filters[i], d, true,
				   serializeProperties);

		context.items->SetDictionary(context.items->GetSize(), d);
	}

	return context.items;
}

void SerializeObsSource(obs_source_t *source, CefRefPtr<CefDictionaryValue> dic,
			bool isExistingSource, bool serializeProperties)
{
	if (!source)
		return;

	if (!dic.get())
		return;

	obs_source_type sourceType = obs_source_get_type(source);
	std::string sourceId = safe_str(obs_source_get_id(source));
	std::string unversioned_id = safe_str(obs_source_get_unversioned_id(source));

	uint32_t sourceCaps = obs_get_source_output_flags(sourceId.c_str());

	// Set codec dictionary properties
	dic->SetString("class", sourceId);

	dic->SetString("unversionedClass", unversioned_id);
	dic->SetBool("isDeprecated", (sourceCaps & OBS_SOURCE_DEPRECATED) != 0);

	if (isExistingSource) {
		dic->SetString("id", GetIdFromPointer(source));
		dic->SetString("name", safe_str(obs_source_get_name(source)));
	} else {
		dic->SetString("id", sourceId);
		dic->SetString("name",
			       safe_str(obs_source_get_display_name(sourceId.c_str())));
	}

	dic->SetString("className",
		       safe_str(obs_source_get_display_name(sourceId.c_str())));

	dic->SetBool("hasVideo",
		     (sourceCaps & OBS_SOURCE_VIDEO) == OBS_SOURCE_VIDEO);
	dic->SetBool("hasAudio",
		     (sourceCaps & OBS_SOURCE_AUDIO) == OBS_SOURCE_AUDIO);

	// Compare sourceId to known video capture devices
	dic->SetBool("isVideoCaptureDevice",
		     sourceId == "dshow_input" || sourceId == "decklink-input");

	// Compare sourceId to known game capture source
	dic->SetBool("isGameCaptureDevice", sourceId == "game_capture");

	// Compare sourceId to known browser source
	dic->SetBool("isBrowserSource", sourceId == "browser_source");

	dic->SetBool("isSceneSource", obs_source_is_scene(source));

	dic->SetBool("isGroupSource", obs_source_is_group(source));

	dic->SetBool("isFilterSource", sourceType == OBS_SOURCE_TYPE_FILTER);
	dic->SetBool("isInputSource", sourceType == OBS_SOURCE_TYPE_INPUT);
	dic->SetBool("isTransitionSource",
		     sourceType == OBS_SOURCE_TYPE_TRANSITION);
	dic->SetBool("isSceneSource", sourceType == OBS_SOURCE_TYPE_SCENE);

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	if (isExistingSource) {
		dic->SetValue("settings", SerializeObsData(settings));
	}

	OBSDataAutoRelease defaultSettings =
		obs_get_source_defaults(sourceId.c_str());

	dic->SetValue("defaultSettings", SerializeObsData(defaultSettings));

	if (serializeProperties) {
		auto properties = obs_source_properties(source);

		if (properties) {
			obs_properties_apply_settings(properties, settings);

			auto propertiesVal = CefValue::Create();
			SerializeObsProperties(properties, propertiesVal);

			dic->SetValue("properties", propertiesVal);

			obs_properties_destroy(properties);
		} else {
			dic->SetList("properties", CefListValue::Create());
		}
	}

	if (!isExistingSource)
		return;

	if (sourceType == OBS_SOURCE_TYPE_FILTER) {
		auto parentSource = obs_filter_get_parent(source);

		if (parentSource) {
			dic->SetInt("order", obs_source_filter_get_index(
						     parentSource, source));
		}
	}

	if (sourceType == OBS_SOURCE_TYPE_FILTER)
		return;

	dic->SetList("filters",
		     SerializeObsSourceFilters(source, serializeProperties));
}

void SerializeObsSourceProperties(CefRefPtr<CefValue> input, CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return;

	std::string id = d->GetString("class");

	OBSDataAutoRelease settings = obs_data_create();

	if (d->HasKey("settings")) {
		if (!DeserializeObsData(d->GetValue("settings"), settings))
			return;
	}

	OBSSourceAutoRelease source =
		obs_source_create_private(id.c_str(), id.c_str(), settings);

	if (!source)
		return;

	auto root = CefDictionaryValue::Create();

	SerializeObsSource(source, root, false, true);

	output->SetDictionary(root);
}

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &output,
					uint32_t requireAnyOfOutputFlagsMask,
	std::vector<obs_source_type> requiredSourceTypes,
	bool serializeProperties)
{
	// Response codec collection (array)
	CefRefPtr<CefListValue> list = CefListValue::Create();

	// Response codec collection is our root object
	output->SetList(list);

	auto add = [&](const char *sourceId,
		       const char *unversioned_id) -> void {
		// Get source caps
		uint32_t sourceCaps = obs_get_source_output_flags(sourceId);

		// Check if the source is disabled, if so - skip it
		if ((sourceCaps & OBS_SOURCE_CAP_DISABLED) != 0)
			return;

		// If source has video / audio / any of the required caps
		if ((sourceCaps & requireAnyOfOutputFlagsMask) == 0L)
			return;

		// We need all of this create-release dance since some 3rd party sources do not support obs_get_source_properties :(
		OBSDataAutoRelease settings = obs_data_create();
		OBSSourceAutoRelease source = obs_source_create_private(
			sourceId, CreateGloballyUniqueIdString().c_str(),
			settings);

		if (!source)
			return;

		obs_source_type sourceType = obs_source_get_type(source);

		if (!unversioned_id) {
			unversioned_id = obs_source_get_unversioned_id(source);
		}

		bool hasRequiredType = false;

		for (auto requiredType : requiredSourceTypes) {
			if (requiredType == sourceType) {
				hasRequiredType = true;

				break;
			}
		}

		if (!hasRequiredType)
			return;

		// Create source response dictionary
		CefRefPtr<CefDictionaryValue> dic =
			CefDictionaryValue::Create();

		SerializeObsSource(source, dic, false, serializeProperties);

		// Append dictionary to response list
		list->SetDictionary(list->GetSize(), dic);
	};

	// Iterate over all required source types
	for (auto requiredType : requiredSourceTypes) {
		if (requiredType == OBS_SOURCE_TYPE_SCENE &&
			(requireAnyOfOutputFlagsMask & OBS_SOURCE_VIDEO) ==
				OBS_SOURCE_VIDEO) {
			add("scene", "scene");

			break;
		}

		// Iterate over all required source type
		for (size_t idx = 0;; ++idx) {
			// Filled by obs_enum_input_types() call below
			const char *sourceId;

			if (requiredType ==
				OBS_SOURCE_TYPE_INPUT) {
				const char *unversioned_id;

				if (!obs_enum_input_types2(idx, &sourceId,
							   &unversioned_id))
					break;

				add(sourceId, unversioned_id);
			} else if (requiredType ==
					OBS_SOURCE_TYPE_FILTER) {
				if (!obs_enum_filter_types(idx, &sourceId))
					break;

				add(sourceId, nullptr);
			} else if (requiredType ==
					OBS_SOURCE_TYPE_TRANSITION) {
				if (!obs_enum_transition_types(idx, &sourceId))
					break;

				add(sourceId, nullptr);
			} else {
				break;
			}
		}
	}
}

void SerializeExistingInputSources(
	CefRefPtr<CefValue> &output, uint32_t requireAnyOfOutputFlagsMask,
	uint32_t requireOutputFlagsMask,
	std::vector<obs_source_type> requireSourceTypes,
	bool serializeProperties)
{
	struct local_context_t {
		CefRefPtr<CefListValue> list;
		uint32_t requireAnyOfOutputFlagsMask;
		uint32_t requireOutputFlagsMask;
		std::vector<obs_source_type> requireSourceTypes;
		bool serializeProperties;
	};

	local_context_t local_context;

	local_context.list = CefListValue::Create();
	local_context.requireAnyOfOutputFlagsMask = requireAnyOfOutputFlagsMask;
	local_context.requireOutputFlagsMask = requireOutputFlagsMask;
	local_context.requireSourceTypes = requireSourceTypes;
	local_context.serializeProperties = serializeProperties;

	auto process = [](void *param, obs_source_t *source) -> bool {
		auto context = (local_context_t *)param;

		if (obs_source_removed(source)) {
			// Skip sources which have been marked removed
			return true;
		}

		// Get source caps
		uint32_t sourceCaps = obs_source_get_output_flags(source);

		obs_source_type sourceType = obs_source_get_type(source);

		bool hasRequiredSourceType = false;
		for (auto requireType : context->requireSourceTypes) {
			if (requireType == sourceType) {
				hasRequiredSourceType = true;
				break;
			}
		}

		// If source has video
		if ((sourceCaps & context->requireOutputFlagsMask) ==
			    context->requireOutputFlagsMask &&
		    (sourceCaps & context->requireAnyOfOutputFlagsMask) != 0L &&
		    hasRequiredSourceType) {
			// Create source response dictionary
			CefRefPtr<CefDictionaryValue> dic =
				CefDictionaryValue::Create();

			SerializeObsSource(source, dic, true,
					   context->serializeProperties);

			// Append dictionary to response list
			context->list->SetDictionary(context->list->GetSize(),
						     dic);
		} else {
			blog(LOG_INFO, "Skipping source '%s' ('%s')",
			     obs_source_get_id(source),
			     obs_source_get_unversioned_id(source));
		}

		return true;
	};

	for (auto sourceType : local_context.requireSourceTypes) {
		switch (sourceType) {
		case OBS_SOURCE_TYPE_SCENE:
			obs_enum_scenes(process, &local_context);
			break;
		default:
			break;
		}
	}

	// Iterate over all sources
	obs_enum_sources(process, &local_context);

	// Response codec collection is our root object
	output->SetList(local_context.list);
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
	std::string result = "Default";

	config_t *globalConfig =
		obs_fe_user_config(); // does not increase refcount

	if (globalConfig) {
		const char *themeName = config_get_string(
			globalConfig, "General", "CurrentTheme");
		if (!themeName) {
			/* Use deprecated "Theme" value if available */
			themeName = config_get_string(globalConfig, "General",
						      "Theme");
			if (!themeName) {
				themeName = "Default";
			}

			result = themeName;
		}
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
	char buf[64] = "unknown";

#ifdef _WIN32
	void* libcef = os_dlopen("libcef");
	if (libcef) {
		typedef int (*cef_version_info_func_ptr_t)(int entry);

		cef_version_info_func_ptr_t cef_version_info_func_ptr =
			(cef_version_info_func_ptr_t)os_dlsym(
				libcef, "cef_version_info");

		if (cef_version_info_func_ptr) {
			sprintf(buf, "cef.%d.%d.chrome.%d.%d.%d.%d",
				cef_version_info_func_ptr(0),
				cef_version_info_func_ptr(1),
				cef_version_info_func_ptr(2),
				cef_version_info_func_ptr(3),
				cef_version_info_func_ptr(4),
				cef_version_info_func_ptr(5));
		}

		os_dlclose(libcef);
	}
#endif

	return std::string(buf);
}

std::string GetCefPlatformApiHash()
{
	char buf[128] = "unknown";

#ifdef _WIN32
	void *libcef = os_dlopen("libcef");
	if (libcef) {
		typedef const char *(*cef_api_hash_func_ptr_t)(int entry);

		cef_api_hash_func_ptr_t cef_api_hash_func_ptr =
			(cef_api_hash_func_ptr_t)os_dlsym(
				libcef, "cef_api_hash");

		if (cef_api_hash_func_ptr) {
			sprintf(buf, "%s", cef_api_hash_func_ptr(0));

			return std::string(buf);
		}

		os_dlclose(libcef);
	}
#endif

	return std::string(buf);
}

std::string GetCefUniversalApiHash()
{
	char buf[128] = "unknown";

#ifdef _WIN32
	void *libcef = os_dlopen("libcef");
	if (libcef) {
		typedef const char *(*cef_api_hash_func_ptr_t)(int entry);

		cef_api_hash_func_ptr_t cef_api_hash_func_ptr =
			(cef_api_hash_func_ptr_t)os_dlsym(libcef,
							  "cef_api_hash");

		if (cef_api_hash_func_ptr) {
			sprintf(buf, "%s", cef_api_hash_func_ptr(1));

			return std::string(buf);
		}

		os_dlclose(libcef);
	}
#endif

	return std::string(buf);
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

bool HttpGet(std::string method, const char *url, http_client_headers_t request_headers,
	     http_client_callback_t callback, void *userdata)
{
	bool result = false;

	CURL *curl = curl_easy_init();

	std::transform(method.begin(), method.end(), method.begin(), ::toupper);

	if (curl) {
		if (method == "GET") {
			// NOP
		} else if (method == "DELETE" || method == "OPTIONS") {
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
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

bool HttpGet(const char* url, http_client_headers_t request_headers,
	http_client_callback_t callback, void* userdata)
{
	return HttpGet("GET", url, request_headers, callback, userdata);
}

bool HttpPost(std::string method, const char *url, http_client_headers_t request_headers,
	      void *buffer, size_t buffer_len, http_client_callback_t callback,
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
			curl_easy_setopt(curl, CURLOPT_UPLOAD, 1L); // implies PUT
		} else if (method == "PATCH") {
			curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
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

bool HttpPost(const char* url, http_client_headers_t request_headers,
	void* buffer, size_t buffer_len, http_client_callback_t callback,
	void* userdata)
{
	return HttpPost("POST", url, request_headers, buffer, buffer_len, callback,
		 userdata);
}

static const size_t MAX_HTTP_STRING_RESPONSE_LENGTH = 1024 * 1024 * 100;

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

bool HttpGetString(const char* url, http_client_headers_t request_headers,
	http_client_string_callback_t callback, void* userdata)
{
	return HttpGetString("GET", url, request_headers, callback, userdata);
}

bool HttpGetBuffer(
	const char *url, http_client_headers_t request_headers,
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

bool HttpPostString(const char* url, http_client_headers_t request_headers,
	const char* postData,
	http_client_string_callback_t callback, void* userdata)
{
	return HttpPostString("POST", url, request_headers, postData, callback,
			      userdata);
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

	std::wstring wArgs;
	for (auto arg : args) {
		wArgs += L"\"" + utf8_to_wstring(arg) + L"\" ";
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
	for (size_t i = 0; i < requests.size(); ++i) {
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
	path = std::regex_replace(path, std::wregex(L"#"), L"%23");
	path = std::regex_replace(path, std::wregex(L"&"), L"%26");

	QUrl parts(StreamElementsGlobalStateManager::GetInstance()
			 ->GetLocalFilesystemHttpServer()
			 ->GetBaseUrl()
			 .c_str());

	parts.setPath(QString::fromStdWString((std::wstring(L"/") + path).c_str()));

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

	return 0;
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
			DispatchClientJSEvent(
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
			DispatchClientJSEvent(
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

	if (0 == ::GetTempFileNameW(wtempBufPath.c_str(),
				    utf8_to_wstring(prefixString).c_str(), 0,
				    pathBuffer)) {
		delete[] pathBuffer;

		return false;
	}

	wtempBufPath = pathBuffer;

	result = wstring_to_utf8(wtempBufPath);

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

std::string GetUniqueFileNameFromPath(std::string fullPath, size_t maxLength)
{
	std::filesystem::path path = std::filesystem::path(utf8_to_wstring(fullPath).c_str());

	auto stem = std::regex_replace(path.stem().wstring(),
				       std::wregex(L"[ ]"), L"_");

	auto ext = std::regex_replace(path.extension().wstring(),
				      std::wregex(L"[ ]"), L"_");

	std::wstring guid = utf8_to_wstring(
		clean_guid_string(CreateGloballyUniqueIdString()));

	std::wstring result = stem + std::wstring(L"_") + guid + ext;

	if (maxLength > 0 && maxLength < result.size()) {
		result = result.substr(result.size() - maxLength);
	}

	return wstring_to_utf8(result);
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

std::shared_ptr<CancelableTask>
HttpGetAsync(std::string url,
		async_http_request_callback_t callback)
{
	return CancelableTask::Execute([=](std::shared_ptr<CancelableTask> task) {
		http_client_headers_t headers;

		auto cb = [=](void* data, size_t datalen, void* userdata, char* error_msg,
			int http_code) {
				if (!task->IsCancelled()) {
					bool success = http_code >= 200 && http_code < 400;

					callback(success, data, datalen);
				}
		};

		HttpGetBuffer(url.c_str(), headers, cb, nullptr);
	});
}

class QRemoteIconMenu : public QMenu {
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

class QRemoteIconAction : public QAction {
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

class QRemoteIconPushButton : public QPushButton {
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

		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

		CefRefPtr<StreamElementsRemoteIconLoader> loaderCopy = loader;

		QObject::connect(this, &QObject::destroyed, [loaderCopy]() {
			loaderCopy->Cancel();
		});
	}

	virtual QSize minimumSizeHint() const override { return sizeHint(); }
	virtual QSize sizeHint() const override
	{
		if (text().size()) {
			QSize textSize = fontMetrics().size(
				Qt::TextShowMnemonic, text());

			QStyleOptionButton opt;
			opt.initFrom(this);
			opt.rect.setSize(textSize);

			QSize size = style()->sizeFromContents(
				QStyle::CT_PushButton, &opt, textSize, this);

			return size;
		} else {
			return QSize(16, 16);
		}
	}

private:
	CefRefPtr<StreamElementsRemoteIconLoader> loader;
};

bool DeserializeDocksMenu(QMenu& menu)
{
	auto widgetManager = StreamElementsGlobalStateManager::GetInstance()
				     ->GetWidgetManager();

	if (!widgetManager)
		return true;

	widgetManager->EnterCriticalSection();

	std::vector<std::string> widgetIds;
	
	widgetManager->GetDockBrowserWidgetIdentifiers(widgetIds);

	std::vector<StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *>
		widgets;

	if (widgetManager) {
		for (auto id : widgetIds) {
			auto info = widgetManager->GetDockBrowserWidgetInfo(
				id.c_str());

			if (info) {
				widgets.push_back(info);
			}
		}
	}

	std::sort(
		widgets.begin(), widgets.end(),
		[](StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo *a,
		   StreamElementsBrowserWidgetManager::DockBrowserWidgetInfo
			   *b) { return a->m_title < b->m_title; });

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
			auto widgetManager =
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager();

			if (!widgetManager)
				return;


			widgetManager->EnterCriticalSection();

			QDockWidget *dock =
				widgetManager->GetDockWidget(id.c_str());

			if (dock) {
				if (isVisible) {
					// Hide
					StreamElementsGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackEvent(
							"se_live_dock_hide_click",
							json11::Json::object{
								{"type",
								 "button_click"},
								{"placement",
								 "menu"}
							},
							json11::Json::array{json11::Json::array{
								"dock_widget_title",
								dock->windowTitle().toStdString()}});
				} else {
					// Show
					StreamElementsGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackEvent(
							"se_live_dock_show_click",
							json11::Json::object{
								{"type",
								 "button_click"},
								{"placement",
								 "menu"}
							},
							json11::Json::array{json11::Json::array{
								"dock_widget_title",
								dock->windowTitle().toStdString()}});
				}

				dock->setVisible(!isVisible);

				StreamElementsGlobalStateManager::GetInstance()
					->GetMenuManager()->Update();
			}

			widgetManager->LeaveCriticalSection();
		});
	}

	for (auto widget : widgets) {
		delete widget;
	}

	widgetManager->LeaveCriticalSection();

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
		"QToolTip { color: palette(text); background-color: palette(dark); } ";

	if (d->HasKey("color") && d->GetType("color") == VTYPE_STRING) {
		styleSheet += "QPushButton { color: ";
		styleSheet += d->GetString("color").ToString() + ";";
		styleSheet += " } ";
	}

	styleSheet += "QPushButton { background-color: palette(button); padding: 1; padding-left: 1em; padding-right: 1em; } ";
	styleSheet +=
		"QPushButton:hover { background-color: palette(midlight); color: palette(dark); } ";
	styleSheet +=
		"QPushButton:pressed { background-color: palette(shadow); color: palette(bright-text); } ";

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
		//control->setMinimumSize(16, 16);
		//control->setSizePolicy(QSizePolicy::Minimum,
		//		       QSizePolicy::Minimum);

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());

			control->setWindowIconText(
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
				obs_source_get_ref(scene);

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

bool IsSafeFileExtension(std::string path)
{
	const char *fileExtension = os_get_path_extension(path.c_str());

	if (!fileExtension) {
		// No file extension, file is safe.
		return true;
	}

	const char *extensions[] = {".dll",   ".exe", ".vst", ".ax",  ".so",
				    ".dylib", ".com", ".msi", ".bat", ".vbs",
				    ".vb",    ".vbe", NULL};

	for (size_t i = 0; extensions[i]; ++i) {
		if (strcasecmp(fileExtension, extensions[i]) == 0)
			return false;
	}

	return true;
}

void DispatchClientMessage(std::string target, CefRefPtr<CefProcessMessage> msg)
{
	if (!StreamElementsGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchClientMessage("system", target, msg);
}

void DispatchClientJSEvent(std::string event, std::string eventArgsJson)
{
	if (!StreamElementsGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", event, eventArgsJson);
}

void DispatchClientJSEvent(std::string target, std::string event, std::string eventArgsJson)
{
	if (!StreamElementsGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", target, event, eventArgsJson);
}

bool SecureJoinPaths(std::string base, std::string subpath, std::string &result)
{
	char *absPath = os_get_abs_path_ptr(base.c_str());
	std::string root = absPath;
	bfree(absPath);

	root += "/";

	absPath = os_get_abs_path_ptr((root + subpath).c_str());
	std::string joined = absPath;
	bfree(absPath);

	if (joined.size() < root.size()) {
		return false;
	}

	if (strncmp(root.c_str(), joined.c_str(), root.size()) != 0) {
		return false;
	}

	result = joined;

	return true;
}

bool SerializeObsProperty(obs_property_t *prop, CefRefPtr<CefValue> &output)
{
	if (!prop)
		return false;

	if (!obs_property_visible(prop))
		return false;

	auto safe_str = [](const char *input) -> std::string {
		if (!input)
			return "";
		else
			return input;
	};

	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();

	obs_property_type type_id = obs_property_get_type(prop);
	std::string dataType = "";
	std::string controlType = "";
	std::string controlMode = "";
	std::string valueFormat = "";
	std::string propDefault = "";

	switch (type_id) {
	case OBS_PROPERTY_BOOL:
		dataType = "bool";
		controlType = "checkbox";
		controlMode = "";
		break;

	case OBS_PROPERTY_INT:
		dataType = "integer";
		controlType = "number";

		switch (obs_property_int_type(prop)) {
		case OBS_NUMBER_SCROLLER:
			controlMode = "scroller";
			break;
		case OBS_NUMBER_SLIDER:
			controlMode = "slider";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_FLOAT:
		dataType = "float";
		controlType = "number";

		switch (obs_property_float_type(prop)) {
		case OBS_NUMBER_SCROLLER:
			controlMode = "scroller";
			break;
		case OBS_NUMBER_SLIDER:
			controlMode = "slider";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_TEXT:
		dataType = "string";
		controlType = "text";
		controlMode = "";

		switch (obs_property_text_type(prop)) {
		case OBS_TEXT_DEFAULT:
			controlMode = "text";
			break;
		case OBS_TEXT_PASSWORD:
			controlMode = "password";
			break;
		case OBS_TEXT_MULTILINE:
			controlMode = "textarea";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_PATH:
		dataType = "string";
		controlType = "path";

		switch (obs_property_path_type(prop)) {
		case OBS_PATH_FILE:
			controlMode = "open";
			break;
		case OBS_PATH_FILE_SAVE:
			controlMode = "save";
			break;
		case OBS_PATH_DIRECTORY:
			controlMode = "folder";
			break;
		default:
			return false;
		}

		valueFormat = safe_str(obs_property_path_filter(prop));
		propDefault = safe_str(obs_property_path_default_path(prop));
		break;

	case OBS_PROPERTY_LIST:
		controlType = "select";

		switch (obs_property_list_type(prop)) {
		case OBS_COMBO_TYPE_EDITABLE:
			controlMode = "dynamic";
			break;
		case OBS_COMBO_TYPE_LIST:
			controlMode = "static";
			break;
		default:
			return false;
		}

		switch (obs_property_list_format(prop)) {
		case OBS_COMBO_FORMAT_INT:
			dataType = "integer";
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			dataType = "float";
			break;
		case OBS_COMBO_FORMAT_STRING:
			dataType = "string";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_COLOR:
		dataType = "string";
		controlType = "text";
		controlMode = "color";
		break;

	case OBS_PROPERTY_BUTTON:
		return false;

	case OBS_PROPERTY_FONT:
		dataType = "font";
		controlType = "font";
		controlMode = "";
		break;

	case OBS_PROPERTY_EDITABLE_LIST:
		dataType = "array";
		controlType = "list";
		controlMode = "dynamic";
		valueFormat = safe_str(obs_property_editable_list_filter(prop));
		propDefault =
			safe_str(obs_property_editable_list_default_path(prop));
		break;

	case OBS_PROPERTY_FRAME_RATE:
		dataType = "frame_rate";
		controlType = "frame_rate";
		controlMode = "";
		break;

	case OBS_PROPERTY_GROUP:
		dataType = "group";
		controlType = "group";

		switch (obs_property_group_type(prop)) {
		case OBS_GROUP_NORMAL:
			controlMode = "normal";
			break;

		case OBS_GROUP_CHECKABLE:
			controlMode = "checkable";
			break;

		default:
			return false;
		}
		break;

	default:
		return false;
	}

	root->SetString("name", safe_str(obs_property_name(prop)));
	root->SetString("label", safe_str(obs_property_description(prop)));
	root->SetString("description",
			safe_str(obs_property_long_description(prop)));
	root->SetString("dataType", dataType);
	root->SetString("controlType", controlType);

	if (controlMode.size())
		root->SetString("controlMode", controlMode);

	if (propDefault.size())
		root->SetString("defaultValue", propDefault);

	if (valueFormat.size())
		root->SetString("valueFormat", valueFormat);

	if (type_id == OBS_PROPERTY_LIST) {
		const size_t count = obs_property_list_item_count(prop);
		const obs_combo_format comboFormat =
			obs_property_list_format(prop);

		CefRefPtr<CefListValue> items = CefListValue::Create();

		for (size_t index = 0; index < count; ++index) {
			CefRefPtr<CefDictionaryValue> item =
				CefDictionaryValue::Create();

			item->SetString("name",
					safe_str(obs_property_list_item_name(
						prop, index)));

			item->SetBool("enabled",
				      !obs_property_list_item_disabled(prop,
								       index));

			switch (comboFormat) {
			case OBS_COMBO_FORMAT_INT:
				item->SetInt("value",
					     obs_property_list_item_int(prop,
									index));
				break;
			case OBS_COMBO_FORMAT_FLOAT:
				item->SetDouble("value",
						obs_property_list_item_float(
							prop, index));
				break;
			case OBS_COMBO_FORMAT_STRING:
				item->SetString(
					"value",
					safe_str(obs_property_list_item_string(
						prop, index)));
				break;
			default:
				continue;
			}

			items->SetDictionary(items->GetSize(), item);
		}

		root->SetList("items", items);
	} else if (type_id == OBS_PROPERTY_EDITABLE_LIST) {
		/* Nothing to do */
	} else if (type_id == OBS_PROPERTY_GROUP) {
		obs_properties_t *props = obs_property_group_content(
			prop); // Does not increment refcount

		if (props) {
			CefRefPtr<CefListValue> items = CefListValue::Create();

			obs_property_t *itemProp = obs_properties_first(props);

			do {
				if (!itemProp)
					break;

				CefRefPtr<CefValue> item = CefValue::Create();

				if (SerializeObsProperty(itemProp, item)) {
					items->SetValue(items->GetSize(), item);
				}
			} while (obs_property_next(&prop));

			root->SetList("items", items);
		}
	}

	output->SetDictionary(root);

	return true;
}

bool SerializeObsProperties(obs_properties_t *props,
			    CefRefPtr<CefValue> &output)
{
	auto list = CefListValue::Create();

	obs_property_t *prop = obs_properties_first(props);

	do {
		if (!prop)
			break;

		CefRefPtr<CefValue> item = CefValue::Create();

		if (SerializeObsProperty(prop, item)) {
			list->SetValue(list->GetSize(), item);
		}
	} while (obs_property_next(&prop));

	output->SetList(list);

	return true;
}


bool DeserializeObsData(CefRefPtr<CefValue> input, obs_data_t *data)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	CefDictionaryValue::KeyList keys;

	if (!d->GetKeys(keys))
		return false;

	for (std::string key : keys) {
		CefRefPtr<CefValue> v = d->GetValue(key);

		if (v->GetType() == VTYPE_STRING)
			obs_data_set_string(data, key.c_str(),
					    v->GetString().ToString().c_str());
		else if (v->GetType() == VTYPE_INT)
			obs_data_set_int(data, key.c_str(), v->GetInt());
		else if (v->GetType() == VTYPE_DOUBLE)
			obs_data_set_double(data, key.c_str(), v->GetDouble());
		else if (v->GetType() == VTYPE_BOOL)
			obs_data_set_bool(data, key.c_str(), v->GetBool());
		else if (v->GetType() == VTYPE_DICTIONARY) {
			obs_data_t *obj = obs_data_create_from_json(
				CefWriteJSON(v, JSON_WRITER_DEFAULT)
					.ToString()
					.c_str());

			obs_data_set_obj(data, key.c_str(), obj);

			obs_data_release(obj);
		} else if (v->GetType() == VTYPE_LIST) {
			CefRefPtr<CefListValue> list = v->GetList();

			obs_data_array_t *array = obs_data_array_create();

			for (size_t index = 0; index < list->GetSize();
			     ++index) {
				obs_data_t *obj = obs_data_create_from_json(
					CefWriteJSON(v, JSON_WRITER_DEFAULT)
						.ToString()
						.c_str());

				obs_data_array_push_back(array, obj);

				obs_data_release(obj);
			}

			obs_data_set_array(data, key.c_str(), array);

			obs_data_array_release(array);
		} else {
			/* Unexpected data type */
			return false;
		}
	}

	return true;
}

CefRefPtr<CefValue> SerializeObsData(obs_data_t *data)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (obs_data_item_t *item = obs_data_first(data); item;
	     obs_data_item_next(&item)) {
		enum obs_data_type type = obs_data_item_gettype(item);
		const char *name = obs_data_item_get_name(item);

		if (!name)
			continue;

		if (type == OBS_DATA_STRING) {
			if (obs_data_item_has_user_value(item) ||
			    obs_data_item_has_default_value(item)) {
				const char *str =
					obs_data_item_get_string(item);

				if (str) {
					d->SetString(name, str);
				} else {
					d->SetNull(name);
				}
			} else {
				d->SetNull(name);
			}
		} else if (type == OBS_DATA_NUMBER) {
			if (obs_data_item_numtype(item) == OBS_DATA_NUM_INT) {
				d->SetInt(name, obs_data_item_get_int(item));
			} else if (obs_data_item_numtype(item) ==
				   OBS_DATA_NUM_DOUBLE) {
				d->SetDouble(name,
					     obs_data_item_get_double(item));
			}
		} else if (type == OBS_DATA_BOOLEAN)
			d->SetBool(name, obs_data_item_get_bool(item));
		else if (type == OBS_DATA_OBJECT) {
			obs_data_t *obj = obs_data_item_get_obj(item);
			if (obj) {
				d->SetValue(name, SerializeObsData(obj));
				obs_data_release(obj);
			}
		} else if (type == OBS_DATA_ARRAY) {
			CefRefPtr<CefListValue> list = CefListValue::Create();

			obs_data_array_t *array = obs_data_item_get_array(item);

			if (array) {
				size_t count = obs_data_array_count(array);

				for (size_t idx = 0; idx < count; idx++) {
					obs_data_t *sub_item =
						obs_data_array_item(array, idx);

					list->SetValue(
						list->GetSize(),
						SerializeObsData(sub_item));

					obs_data_release(sub_item);
				}

				obs_data_array_release(array);
			}
		}
	}

	CefRefPtr<CefValue> result = CefValue::Create();
	result->SetDictionary(d);

	/*
	return CefParseJSON(obs_data_get_json(data),
			JSON_PARSER_ALLOW_TRAILING_COMMAS);
	*/

	return result;
}

CefRefPtr<CefValue>
SerializeObsEncoderProperties(std::string id, obs_data_t *settings)
{
	auto result = CefValue::Create();

	result->SetNull();

	auto props = obs_get_encoder_properties(id.c_str());

	if (props) {
		if (settings)
			obs_properties_apply_settings(props, settings);

		SerializeObsProperties(props, result);

		obs_properties_destroy(props);
	}

	return result;
}

uint32_t GetInt32FromAlignmentId(std::string alignment)
{
	uint32_t result = 0;

	if (std::regex_search(alignment, std::regex("left")))
		result |= OBS_ALIGN_LEFT;

	if (std::regex_search(alignment, std::regex("right")))
		result |= OBS_ALIGN_RIGHT;

	if (std::regex_search(alignment, std::regex("top")))
		result |= OBS_ALIGN_TOP;

	if (std::regex_search(alignment, std::regex("bottom")))
		result |= OBS_ALIGN_BOTTOM;

	return result;
}

std::string GetAlignmentIdFromInt32(uint32_t a)
{
	std::string h = "center";
	std::string v = "center";

	if (a & OBS_ALIGN_LEFT) {
		h = "left";
	} else if (a & OBS_ALIGN_RIGHT) {
		h = "right";
	}

	if (a & OBS_ALIGN_TOP) {
		v = "top";
	} else if (a & OBS_ALIGN_BOTTOM) {
		v = "bottom";
	}

	if (h == v) {
		return "center";
	} else {
		return v + "_" + h;
	}
}

void SerializeObsTransition(std::string videoCompositionId, obs_source_t *t,
			    int durationMilliseconds,
			    CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!t)
		return;

	auto d = CefDictionaryValue::Create();

	std::string id = obs_source_get_id(t);
	d->SetString("class", id);
	d->SetString("videoCompositionId", videoCompositionId);

	auto settings = obs_source_get_settings(t);

	auto props = obs_get_source_properties(id.c_str());
	auto propsValue = CefValue::Create();
	obs_properties_apply_settings(props, settings);
	SerializeObsProperties(props, propsValue);
	obs_properties_destroy(props);

	auto settingsValue = SerializeObsData(settings);

	obs_data_release(settings);

	d->SetValue("properties", propsValue);
	d->SetValue("settings", settingsValue);

	auto defaultsData = obs_get_source_defaults(id.c_str());
	d->SetValue("defaultSettings", SerializeObsData(defaultsData));
	obs_data_release(defaultsData);

	d->SetString("label", obs_source_get_display_name(id.c_str()));

	uint32_t cx, cy;
	obs_transition_get_size(t, &cx, &cy);
	d->SetInt("width", cx);
	d->SetInt("height", cy);

	auto scaleType = obs_transition_get_scale_type(t);
	switch (scaleType) {
	case OBS_TRANSITION_SCALE_MAX_ONLY:
		d->SetString("scaleType", "maxOnly");
		break;
	case OBS_TRANSITION_SCALE_ASPECT:
		d->SetString("scaleType", "aspect");
		break;
	case OBS_TRANSITION_SCALE_STRETCH:
		d->SetString("scaleType", "stretch");
		break;
	default:
		d->SetString("scaleType", "unknown");
		break;
	}

	d->SetString("alignment",
		     GetAlignmentIdFromInt32(obs_transition_get_alignment(t)));

	d->SetInt("durationMilliseconds", durationMilliseconds);

	output->SetDictionary(d);
}

obs_source_t *GetExistingObsTransition(std::string lookupId)
{
	obs_source_t *result = nullptr;

	obs_frontend_source_list l = {};
	obs_frontend_get_transitions(&l);

	for (size_t idx = 0; idx < l.sources.num; ++idx) {
		auto source = l.sources.array[idx];

		std::string id = obs_source_get_id(source);

		if (id == lookupId) {
			result = source;

			obs_source_get_ref(result);

			break;
		}
	}

	obs_frontend_source_list_free(&l);

	return result;
}

bool DeserializeObsTransition(CefRefPtr<CefValue> input, obs_source_t **t,
			      int *durationMilliseconds, bool useExisting)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	auto d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return false;

	std::string id = d->GetString("class");

	if (useExisting) {
		*t = GetExistingObsTransition(id);

		if (!*t)
			return false;
	} else {
		*t = obs_source_create_private(id.c_str(), id.c_str(), nullptr);
	}

	if (d->HasKey("settings")) {
		auto settings = obs_data_create();

		DeserializeObsData(d->GetValue("settings"), settings);

		obs_source_update(*t, settings);

		obs_data_release(settings);
	}

	if (d->HasKey("width") && d->GetType("width") == VTYPE_INT &&
	    d->HasKey("height") && d->GetType("height") == VTYPE_INT) {
		int cx = d->GetInt("width");
		int cy = d->GetInt("height");

		if (cx <= 0 || cy <= 0)
			return false;

		obs_transition_set_size(*t, cx, cy);
	}

	if (d->HasKey("scaleType") && d->GetType("scaleType") == VTYPE_STRING) {
		auto scaleType = d->GetString("scaleType");

		if (scaleType == "maxOnly") {
			obs_transition_set_scale_type(
				*t, OBS_TRANSITION_SCALE_MAX_ONLY);
		} else if (scaleType == "aspect") {
			obs_transition_set_scale_type(
				*t, OBS_TRANSITION_SCALE_ASPECT);
		} else if (scaleType == "stretch") {
			obs_transition_set_scale_type(
				*t, OBS_TRANSITION_SCALE_STRETCH);
		} else {
			return false;
		}
	}

	if (d->HasKey("alignment") && d->GetType("alignment") == VTYPE_STRING) {
		obs_transition_set_alignment(
			*t, GetInt32FromAlignmentId(
				    d->GetString("alignment").c_str()));
	}

	if (d->HasKey("durationMilliseconds") &&
	    d->GetType("durationMilliseconds") == VTYPE_INT) {
		*durationMilliseconds = d->GetInt("durationMilliseconds");
	} else {
		*durationMilliseconds = 300; // OBS FE default
	}

	return true;
}

CefRefPtr<CefDictionaryValue> SerializeObsEncoder(obs_encoder_t *e)
{
	auto result = CefDictionaryValue::Create();

	auto id = obs_encoder_get_id(e);
	result->SetString("class", id);

	auto name = obs_encoder_get_name(e);
	result->SetString("name", name ? name : id);

	auto displayName = obs_encoder_get_display_name(obs_encoder_get_id(e));
	result->SetString("label",
			  displayName ? displayName : (name ? name : id));

	auto codec = obs_encoder_get_codec(e);

	if (codec)
		result->SetString("codec", obs_encoder_get_codec(e));
	else
		result->SetNull("codec");

	if (obs_encoder_get_type(e) == OBS_ENCODER_VIDEO) {
		if (obs_encoder_parent_video(e) ||
		    obs_encoder_scaling_enabled(e)) {
			result->SetInt("width", obs_encoder_get_width(e));
			result->SetInt("height", obs_encoder_get_height(e));
		}
	}

	auto settings = obs_encoder_get_settings(e);

	result->SetValue("settings",
			 CefParseJSON(obs_data_get_json(settings),
				      JSON_PARSER_ALLOW_TRAILING_COMMAS));

	result->SetValue("properties",
			 SerializeObsEncoderProperties(obs_encoder_get_id(e),
						       settings));

	auto defaultSettings = obs_encoder_get_defaults(e);

	if (defaultSettings) {
		result->SetValue("defaultSettings",
				 SerializeObsData(defaultSettings));

		obs_data_release(defaultSettings);
	}

	obs_data_release(settings);

	return result;
}

