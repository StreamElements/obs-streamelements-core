#include "SEPathUtils.hpp"

#include "SEStringUtils.hpp"

// ReadListOfObsProfiles traces the config_t handle it opens, the same
// way every other refcounted handle in the plugin is traced.
#include "../SETrace.hpp"

#include <cef-headers.hpp>

#include <util/bmem.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <filesystem>

#ifdef WIN32
#include <windows.h>
#include <algorithm>
#include <regex>
#endif

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
	std::filesystem::path path =
		std::filesystem::path(utf8_to_wstring(fullPath).c_str());

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
			SETRACE_ADDREF(ini);

			const char *value =
				config_get_string(ini, "General", "Name");

			if (value) {
				std::string name = value;

				output[id] = name;
			}

			config_close(SETRACE_DECREF(ini));
		}
	}

	os_closedir(dir);

	return true;
}
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

bool FileExists(std::string path)
{
	//
	// See the comment on the declaration: os_file_exists("") leaks one
	// allocation per call inside libobs. An empty path is not a file, so
	// this answers it without asking.
	//
	if (path.empty())
		return false;

	return os_file_exists(path.c_str());
}
