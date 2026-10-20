#include "SESystemInfoUtils.hpp"

#include "SEAsyncTaskUtils.hpp"
#include "SECryptoUtils.hpp"
#include "SEEnvConfigUtils.hpp"
#include "SEStringUtils.hpp"

#include "../Version.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <util/config-file.h>
#include <util/platform.h>

#include <cstdint>
#include <iostream>
#include <regex>
#include <string>
#include <vector>

#ifdef WIN32
#include <windows.h>
#include <wbemidl.h>

//
// The link directives came with these bodies: they used to sit in SEUtils.cpp
// beside them, and nothing else pulls these libraries in.
//
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
#else
// The same platform shims the single translation unit carried: these bodies
// are written against the Windows registry and FILETIME vocabulary, and the
// macOS halves live in SEUtils.mm.
#include <mach/host_info.h>
#include <mach/mach_host.h>
#include <mach/mach_init.h>
#include <mach/mach_time.h>
#include <mach/mach_types.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>

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

#ifdef WIN32
//
// Windows-only helper, used only by the branch below.
//
static uint64_t FromFileTime(const FILETIME &ft)
{
	ULARGE_INTEGER uli = {0};
	uli.LowPart = ft.dwLowDateTime;
	uli.HighPart = ft.dwHighDateTime;
	return uli.QuadPart;
}
#endif /* WIN32 */
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
	if (host_statistics((host_t)mach_port, HOST_CPU_LOAD_INFO,
			    (host_info_t)&cpu_load_info,
			    &cpu_load_info_count) == KERN_SUCCESS) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		d->SetDouble("idleSeconds",
			     (double)(cpu_load_info.cpu_ticks[CPU_STATE_IDLE]) /
				     (double)CLOCKS_PER_SEC);
		d->SetDouble(
			"kernelSeconds",
			(double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM]) /
				(double)CLOCKS_PER_SEC);
		d->SetDouble("userSeconds",
			     (double)(cpu_load_info.cpu_ticks[CPU_STATE_USER] +
				      cpu_load_info.cpu_ticks[CPU_STATE_NICE]) /
				     (double)CLOCKS_PER_SEC);
		d->SetDouble(
			"totalSeconds",
			(double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] +
				 cpu_load_info.cpu_ticks[CPU_STATE_USER] +
				 cpu_load_info.cpu_ticks[CPU_STATE_IDLE] +
				 cpu_load_info.cpu_ticks[CPU_STATE_NICE]) /
				(double)CLOCKS_PER_SEC);
		d->SetDouble(
			"busySeconds",
			(double)(cpu_load_info.cpu_ticks[CPU_STATE_SYSTEM] +
				 cpu_load_info.cpu_ticks[CPU_STATE_USER] +
				 cpu_load_info.cpu_ticks[CPU_STATE_NICE]) /
				(double)CLOCKS_PER_SEC);
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

		//
		// DIV has already scaled these to megabytes, so the narrowing
		// to CefDictionaryValue::SetInt's int is safe -- it is explicit
		// because this file no longer sits behind the warning
		// suppression that used to leak over it.
		//
		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage", mem.dwMemoryLoad);
		d->SetInt("totalPhysicalMemory", (int)(mem.ullTotalPhys / DIV));
		d->SetInt("freePhysicalMemory", (int)(mem.ullAvailPhys / DIV));
		d->SetInt("totalVirtualMemory",
			  (int)(mem.ullTotalVirtual / DIV));
		d->SetInt("freeVirtualMemory",
			  (int)(mem.ullAvailVirtual / DIV));
		d->SetInt("freeExtendedVirtualMemory",
			  (int)(mem.ullAvailExtendedVirtual / DIV));
		d->SetInt("totalPageFileSize",
			  (int)(mem.ullTotalPageFile / DIV));
		d->SetInt("freePageFileSize",
			  (int)(mem.ullAvailPageFile / DIV));
	}
#else
	mach_port_t mach_port = mach_host_self();
	vm_statistics_data_t vm_stats;

	mach_msg_type_number_t vm_info_count = HOST_VM_INFO_COUNT;
	vm_size_t page_size;
	if (host_statistics((host_t)mach_port, HOST_VM_INFO,
			    (host_info_t)&vm_stats,
			    &vm_info_count) == KERN_SUCCESS &&
	    host_page_size(mach_port, &page_size) == KERN_SUCCESS) {
		int64_t free_memory =
			(int64_t)vm_stats.free_count * (int64_t)page_size;
		int64_t used_memory = ((int64_t)vm_stats.active_count +
				       (int64_t)vm_stats.inactive_count +
				       (int64_t)vm_stats.wire_count) *
				      (int64_t)page_size;
		int64_t total_memory = free_memory + used_memory;

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		output->SetDictionary(d);

		d->SetString("units", "MB");
		d->SetInt("memoryUsedPercentage",
			  used_memory * 100L / total_memory);
		d->SetInt("totalVirtualMemory", total_memory / DIV);
		d->SetInt("freeVirtualMemory", free_memory / DIV);
		d->SetInt("freeExtendedVirtualMemory", 0);
		d->SetInt("totalPageFileSize", 0);
		d->SetInt("freePageFileSize", 0);

		{
			int mib[2] = {CTL_HW, HW_MEMSIZE};
			int64_t physical_memory;
			size_t length = sizeof(physical_memory);
			sysctl(mib, 2, &physical_memory, &length, NULL, 0);

			d->SetInt("totalPhysicalMemory", physical_memory / DIV);
			d->SetInt("freePhysicalMemory",
				  free_memory / DIV); // inaccurate
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
#ifdef WIN32
//
// Windows-only body; the macOS half is in SEUtils.mm. The guard came
// with it out of the single translation unit.
//
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
#endif /* WIN32 */
std::string GetCefVersionString()
{
	char buf[64] = "unknown";

#ifdef _WIN32
	void *libcef = os_dlopen("libcef");
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
			(cef_api_hash_func_ptr_t)os_dlsym(libcef,
							  "cef_api_hash");

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
std::string GetSEPluginVersionString()
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
std::string GetSEApiVersionString()
{
	char version_buf[64];

	sprintf(version_buf, "%d.%d", HOST_API_VERSION_MAJOR,
		HOST_API_VERSION_MINOR);

	return version_buf;
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
//
// Windows-only body; the macOS half is in SEUtils.mm. The guard came
// with it out of the single translation unit.
//
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
		}

		BCryptCloseAlgorithmProvider(hAlgo, 0);
	}

	return result;
}
// MacOS version implemented in SEUtils.mm
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

	// ...and discard anything that is not even shaped like an id.
	//
	// Every id this function produces is "<4-letter tag>/<payload>" --
	// WUID, SWID or SEID here, ALSU on macOS. The list above only rejects
	// four exact strings, so a value that is merely malformed used to be
	// accepted and, because it is written straight back to the registry,
	// became that machine's permanent identity.
	//
	// Not hypothetical: a machine was found reporting a user id of literally
	// "S" -- one character, REG_SZ, the first letter of an SWID/SEID value.
	// Every crash from it was anonymous and none could be correlated with
	// any other. Rejecting the value here regenerates a good one on the next
	// call rather than requiring the registry to be cleaned by hand.
	if (result.size() && (result.size() < 16 || result.find('/') != 4)) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: discarding malformed machine unique id '%s'; a new one will be generated",
		     result.c_str());

		result = "";
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
#endif /* WIN32 */
