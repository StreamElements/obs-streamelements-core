#include "SEEnvConfigUtils.hpp"

#include "SEStringUtils.hpp"

#include <util/config-file.h>
#include <util/platform.h>

#ifdef WIN32
#include <windows.h>
// PathRemoveFileSpecW, for trimming the module path down to its folder
#include <shlwapi.h>
#endif

static const char *ENV_PRODUCT_NAME = "OBS.Live";

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
std::string ReadEnvironmentConfigString(const char *regValueName,
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
	SETRACE_ADDREF(config);
	bfree(filePath);

	const char *str = config_get_string(
		config, productName ? productName : "Global", regValueName);

	if (str) {
		result = str;
	}

	config_close(SETRACE_DECREF(config));
#endif

	return result;
}
bool WriteEnvironmentConfigString(const char *regValueName,
				  const char *regValue, const char *productName)
{
	bool result = false;

#ifdef WIN32
	std::string REG_KEY_PATH = GetEnvironmentConfigRegKeyPath(productName);

	// +1 for the terminator. RegSetKeyValueA does not add one, and the docs
	// are explicit that a REG_SZ written without it leaves readers to cope
	// with an unterminated value.
	LSTATUS lResult = RegSetKeyValueA(HKEY_LOCAL_MACHINE,
					  REG_KEY_PATH.c_str(), regValueName,
					  REG_SZ, regValue,
					  (DWORD)(strlen(regValue) + 1));

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
	SETRACE_ADDREF(config);
	bfree(filePath);

	config_set_string(config, productName ? productName : "Global",
			  regValueName, regValue);

	config_save_safe(config, "tmp", "bak");

	config_close(SETRACE_DECREF(config));

	result = true;
#endif
	return result;
}
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
bool WriteEnvironmentConfigStrings(streamelements_env_update_requests requests)
{
#ifndef WIN32
	for (auto req : requests) {
		if (!WriteEnvironmentConfigString(req.key.c_str(),
						  req.value.c_str(),
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
