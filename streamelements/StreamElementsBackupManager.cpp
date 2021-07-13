#include "StreamElementsBackupManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsNetworkDialog.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "deps/zip/zip.h"

#ifdef WIN32
#include <io.h>
#else
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#endif
#include <fcntl.h>

#include <vector>
#include <map>
#include <codecvt>
#include <regex>

#ifndef BYTE
typedef unsigned char BYTE;
#endif

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef SH_DENYNO
#define SH_DENYNO 0
#endif

#define MAX_BACKUP_FILE_SIZE 0x7FFFFFFF

static bool GetLocalPathFromURL(std::string url, std::string &path)
{
	if (VerifySessionSignedAbsolutePathURL(url, path))
		return true;

	if (!GetTemporaryFilePath("obs-live-restore", path))
		return false;

	bool result = false;

	QtExecSync([path,url,&result]() {
		obs_frontend_push_ui_translation(obs_module_get_string);

		StreamElementsNetworkDialog netDialog(
			StreamElementsGlobalStateManager::GetInstance()
				->mainWindow());

		obs_frontend_pop_ui_translation();

		result = netDialog.DownloadFile(
			path.c_str(), url.c_str(), true,
			obs_module_text(
				"StreamElements.BackupRestore.Download.Message"));
	});

	if (!result)
		os_unlink(path.c_str());
	else
		StreamElementsGlobalStateManager::GetInstance()
			->GetCleanupManager()
			->AddPath(path);

	return result;
}

static bool AddFileToZip(zip_t *zip, std::string localPath, std::string zipPath)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

#ifdef WIN32
	int fd = _wsopen(myconv.from_bytes(localPath).c_str(),
			 O_RDONLY | O_BINARY, SH_DENYNO,
			 0 /*S_IREAD | S_IWRITE*/);
#else
    int fd = ::open(localPath.c_str(),
             O_RDONLY);
#endif
    
	if (-1 != fd) {
		size_t BUF_LEN = 32768;

		BYTE *buf = new BYTE[BUF_LEN];

		if (0 == zip_entry_open(zip, zipPath.c_str())) {
			bool success = true;

			int read = ::read(fd, buf, BUF_LEN);
			while (read > 0 && success) {
				int retVal = zip_entry_write(zip, buf, read);
				if (retVal < 0) {
					success = false;

					break;
				}

				read = ::read(fd, buf, BUF_LEN);
			}

			zip_entry_close(zip);

			delete[] buf;

			::close(fd);

			return success;
		} else {
			// Filed opening ZIP entry for writing
		}
	} else {
		// Failed opening file for reading
		//
		// This is a crash handler: you can't really do anything
		// here to mitigate.
	}

	return false;
}

static bool AddBufferToZip(zip_t *zip, BYTE *buf, size_t bufLen,
			   std::string zipPath)
{
	bool result = false;

	if (0 == zip_entry_open(zip, zipPath.c_str())) {
		if (0 == zip_entry_write(zip, buf, bufLen))
			result = true;

		zip_entry_close(zip);
	}

	return result;
};

static const std::string MONIKER_START =
	"<streamelements:relative-path:obs-studio>";
static const std::string MONIKER_END =
	"<streamelements:relative-path:obs-studio>";

static bool ScanForFileReferencesMonikersToRestore(CefRefPtr<CefValue> &node,
						   CefRefPtr<CefValue> &result,
						   std::string srcBasePath,
						   std::string destBasePath)
{
	result = node->Copy();

	if (node->GetType() == VTYPE_STRING) {
		std::string moniker = node->GetString().ToString();

		if (moniker.substr(0, MONIKER_START.size()) == MONIKER_START &&
		    moniker.substr(moniker.size() - MONIKER_END.size()) ==
			    MONIKER_END) {
			std::string relPath = moniker.substr(
				MONIKER_START.size(),
				moniker.size() - (MONIKER_START.size() +
						  MONIKER_END.size()));

			std::string srcPath = srcBasePath + "/" + relPath;

			if (os_file_exists(srcPath.c_str())) {
				std::string destPath =
					destBasePath + "/" + relPath;

				std::transform(destPath.begin(), destPath.end(),
					       destPath.begin(), [](char ch) {
						       if (ch == '\\')
							       return '/';
						       else
							       return ch;
					       });

				result->SetString(destPath);
			}
		}
	} else if (node->GetType() == VTYPE_LIST) {
		CefRefPtr<CefListValue> list = node->GetList();
		CefRefPtr<CefListValue> out = CefListValue::Create();

		for (size_t index = 0; index < list->GetSize(); ++index) {
			CefRefPtr<CefValue> value =
				list->GetValue(index)->Copy();

			if (!ScanForFileReferencesMonikersToRestore(
				    value, value, srcBasePath, destBasePath))
				return false;

			out->SetValue(index, value);
		}

		result->SetList(out);
	} else if (node->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = node->GetDictionary();
		CefRefPtr<CefDictionaryValue> out =
			CefDictionaryValue::Create();

		CefDictionaryValue::KeyList keys;
		if (d->GetKeys(keys)) {
			for (auto key : keys) {
				CefRefPtr<CefValue> value =
					d->GetValue(key)->Copy();

				if (!ScanForFileReferencesMonikersToRestore(
					    value, value, srcBasePath,
					    destBasePath))
					return false;

				out->SetValue(key, value);
			}
		}

		result->SetDictionary(out);
	}

	return true;
}

static bool ScanForFileReferencesMonikersToRestore(std::string srcBasePath,
						   std::string destBasePath)
{
	bool success = true;

	std::string srcScanPath = srcBasePath + "/basic/scenes";

	os_dir_t *dir = os_opendir(srcScanPath.c_str());

	if (!dir) {
		// No scenes to restore, so the result is empty but operation succeeds
		return true;
	}

	struct os_dirent *entry;

	while ((entry = os_readdir(dir)) != NULL) {
		if (entry->directory || *entry->d_name == '.')
			continue;

		std::string fileName = entry->d_name;

		std::smatch match;

		if (!std::regex_search(fileName, match,
				       std::regex("^(.+?)\\.json$")))
			continue;

		std::string srcFilePath = srcScanPath + "/" + fileName;

		char *buffer = os_quick_read_utf8_file(srcFilePath.c_str());

		if (!buffer) {
			success = false;

			break;
		}

		CefRefPtr<CefValue> content = CefParseJSON(
			CefString(buffer), JSON_PARSER_ALLOW_TRAILING_COMMAS);
		bfree(buffer);

		if (!content.get() || content->GetType() == VTYPE_NULL)
			continue;

		CefRefPtr<CefValue> resultContent = CefValue::Create();

		if (!ScanForFileReferencesMonikersToRestore(
			    content, resultContent, srcBasePath,
			    destBasePath)) {
			success = false;

			break;
		}

		std::string json =
			CefWriteJSON(resultContent, JSON_WRITER_PRETTY_PRINT);

		std::string destFilePath = srcFilePath; // this is NOT a bug

		if (!os_quick_write_utf8_file(destFilePath.c_str(),
					      json.c_str(), json.size(),
					      false)) {
			success = false;

			break;
		}
	}

	os_closedir(dir);

	return success;
}

static bool
ScanForFileReferences(CefRefPtr<CefValue> &node, std::map<std::string, std::string> &filesMap)
{
	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	if (node->GetType() == VTYPE_STRING) {
		std::string path = node->GetString().ToString();

		if (!filesMap.count(path)) {
			if (os_file_exists(path.c_str())) {
				filesMap[path] =
					CreateSessionSignedAbsolutePathURL(
						myconv.from_bytes(path));
			}
		}
	} else if (node->GetType() == VTYPE_LIST) {
		CefRefPtr<CefListValue> list = node->GetList();

		for (size_t index = 0; index < list->GetSize(); ++index) {
			CefRefPtr<CefValue> value =
				list->GetValue(index)->Copy();

			if (!ScanForFileReferences(value, filesMap))
				return false;
		}
	} else if (node->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = node->GetDictionary();

		CefDictionaryValue::KeyList keys;
		if (d->GetKeys(keys)) {
			for (auto key : keys) {
				CefRefPtr<CefValue> value =
					d->GetValue(key)->Copy();

				if (!ScanForFileReferences(value, filesMap))
					return false;
			}
		}
	}

	return true;
}

static bool
ScanForFileReferencesToBackup(zip_t *zip, CefRefPtr<CefValue> &node,
			      std::map<std::string, std::string> &filesMap,
			      std::string timestamp,
			      CefRefPtr<CefValue> &result)
{
	result = node->Copy();

	if (node->GetType() == VTYPE_STRING) {
		std::string path = node->GetString().ToString();

		if (os_file_exists(path.c_str())) {
			int64_t fileSize = os_get_file_size(path.c_str());

			if (fileSize > MAX_BACKUP_FILE_SIZE) {
				blog(LOG_WARNING,
				     "obs-browser: backup: file skipped due to unsatisfied maximum file size constraint: %s",
				     path.c_str());
			} else {
				if (!filesMap.count(path)) {
					std::string fileName =
						GetUniqueFileNameFromPath(path,
									  48);
					std::string zipPath =
						"obslive_restored_files/" +
						timestamp + "/" + fileName;

					if (!AddFileToZip(zip, path, zipPath))
						return false;

					filesMap[path] = zipPath;
				}

				std::string zipPath = filesMap[path];

				std::string moniker =
					MONIKER_START + zipPath + MONIKER_END;

				result->SetString(moniker);
			}
		}
	} else if (node->GetType() == VTYPE_LIST) {
		CefRefPtr<CefListValue> list = node->GetList();
		CefRefPtr<CefListValue> out = CefListValue::Create();

		for (size_t index = 0; index < list->GetSize(); ++index) {
			CefRefPtr<CefValue> value =
				list->GetValue(index)->Copy();

			if (!ScanForFileReferencesToBackup(zip, value, filesMap,
							   timestamp, value))
				return false;

			out->SetValue(index, value);
		}

		result->SetList(out);
	} else if (node->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = node->GetDictionary();
		CefRefPtr<CefDictionaryValue> out =
			CefDictionaryValue::Create();

		CefDictionaryValue::KeyList keys;
		if (d->GetKeys(keys)) {
			for (auto key : keys) {
				CefRefPtr<CefValue> value =
					d->GetValue(key)->Copy();

				if (!ScanForFileReferencesToBackup(
					    zip, value, filesMap, timestamp,
					    value))
					return false;

				out->SetValue(key, value);
			}
		}

		result->SetDictionary(out);
	}

	return true;
}

static bool AddReferencedFilesToZip(zip_t *zip, std::string timestamp,
				    CefRefPtr<CefValue> &content,
				    CefRefPtr<CefValue> &result)
{
	std::map<std::string, std::string> filesMap;

	return ScanForFileReferencesToBackup(zip, content, filesMap, timestamp,
					     result);
}

static CefRefPtr<CefValue> ReadCollectionById(std::string basePath,
					      std::string collection)
{
	CefRefPtr<CefValue> result = CefValue::Create();

	result->SetNull();

	std::string relPath = "basic/scenes/" + collection + ".json";
	std::string absPath = basePath + "/" + relPath;

	if (os_file_exists(absPath.c_str())) {
		char *buffer = os_quick_read_utf8_file(absPath.c_str());
		CefRefPtr<CefValue> content = CefParseJSON(
			CefString(buffer), JSON_PARSER_ALLOW_TRAILING_COMMAS);
		bfree(buffer);

		if (content.get() && content->GetType() != VTYPE_NULL)
			result = content;
	}

	return result;
}

static bool AddCollectionToZip(zip_t *zip, std::string basePath,
			       std::string collection,
			       bool includeReferencedFiles,
			       std::string timestamp)
{
	std::string relPath = "basic/scenes/" + collection + ".json";
	std::string absPath = basePath + "/" + relPath;

	if (!os_file_exists(absPath.c_str()))
		return false;

	if (!includeReferencedFiles) {
		return AddFileToZip(zip, absPath, relPath);
	}

	char *buffer = os_quick_read_utf8_file(absPath.c_str());
	CefRefPtr<CefValue> content = CefParseJSON(
		CefString(buffer), JSON_PARSER_ALLOW_TRAILING_COMMAS);
	bfree(buffer);

	if (!content.get() || content->GetType() == VTYPE_NULL)
		return false;

	std::map<std::string, std::string> filesMap;
	CefRefPtr<CefValue> resultContent = CefValue::Create();

	if (!AddReferencedFilesToZip(zip, timestamp, content, resultContent))
		return false;

	std::string json =
		CefWriteJSON(resultContent, JSON_WRITER_PRETTY_PRINT);

	return AddBufferToZip(zip, (BYTE *)json.c_str(), json.size(), relPath);
}

static bool AddProfileToZip(zip_t *zip, std::string basePath,
			    std::string profile)
{
	std::string relPath = "basic/profiles/" +
			      std::regex_replace(profile, std::regex(" "), "_");
	std::string absPath = basePath + "/" + relPath;

	if (!os_file_exists(absPath.c_str()))
		return false;

	if (!AddFileToZip(zip, absPath + "/basic.ini", relPath + "/basic.ini"))
		return false;

	AddFileToZip(zip, absPath + "/service.json", relPath + "/service.json");

	AddFileToZip(zip, absPath + "/streamEncoder.json",
		     relPath + "/streamEncoder.json");

	config_t *profile_config;

	if (config_open(&profile_config, (absPath + "/basic.ini").c_str(),
			CONFIG_OPEN_EXISTING) == CONFIG_SUCCESS) {
		const char *cookieProfileId =
			config_get_string(profile_config, "Panels", "CookieId");

		if (cookieProfileId) {
			std::string cookieRelPath =
				"plugin_config/obs-browser/obs_profile_cookies/" +
				std::string(cookieProfileId);

			std::string cookieAbsPath = basePath + "/" + relPath;

			AddFileToZip(zip, cookieAbsPath + "/Cookies",
				     cookieRelPath + "/Cookies");

			AddFileToZip(zip, cookieRelPath + "/Cookies-journal",
				     cookieRelPath + "/Cookies-journal");
		}

		config_close(profile_config);
	}

	return true;
}

static void ReadListOfSceneCollectionIds(std::vector<std::string> &output)
{
	std::map<std::string, std::string> items;
	ReadListOfObsSceneCollections(items);

	for (auto item : items) {
		output.push_back(item.first);
	}
}

static void ReadListOfProfileIds(std::vector<std::string> &output)
{
	std::map<std::string, std::string> items;
	ReadListOfObsProfiles(items);

	for (auto item : items) {
		output.push_back(item.first);
	}
}

static void ReadListOfIdsFromCefValue(CefRefPtr<CefValue> input,
				      std::vector<std::string> &output)
{
	if (input->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list = input->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_DICTIONARY)
			continue;

		CefRefPtr<CefDictionaryValue> d = list->GetDictionary(index);

		if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
			continue;

		std::string id = d->GetString("id").ToString();

		if (!id.size())
			continue;

		output.push_back(id);
	}
}

StreamElementsBackupManager::StreamElementsBackupManager() {}

StreamElementsBackupManager::~StreamElementsBackupManager() {}

void StreamElementsBackupManager::QueryLocalBackupPackageReferencedFiles(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> in = input->GetDictionary();

	std::vector<std::string> requestCollections;

	CefRefPtr<CefListValue> addedCollections = CefListValue::Create();

	if (in->HasKey("sceneCollections") &&
	    in->GetType("sceneCollections") == VTYPE_LIST) {
		ReadListOfIdsFromCefValue(in->GetValue("sceneCollections"),
					  requestCollections);
	} else {
		ReadListOfSceneCollectionIds(requestCollections);
	}

	char *basePathPtr = os_get_config_path_ptr("obs-studio");
	std::string basePath = basePathPtr;
	bfree(basePathPtr);

	for (auto collection : requestCollections) {
		CefRefPtr<CefValue> collectionValue =
			ReadCollectionById(basePath, collection);

		if (!collectionValue.get() ||
		    collectionValue->GetType() == VTYPE_NULL)
			continue;

		std::map<std::string, std::string> filesMap;

		if (!ScanForFileReferences(collectionValue, filesMap))
			continue;

		CefRefPtr<CefListValue> list = CefListValue::Create();

		for (auto kv : filesMap) {
			auto path = kv.first;
			auto url = kv.second;

			CefRefPtr<CefDictionaryValue> fileDict =
				CefDictionaryValue::Create();

			int64_t fileSize = os_get_file_size(path.c_str());

			fileDict->SetString("path", path.c_str());
			fileDict->SetString("url", url.c_str());
			fileDict->SetDouble("size", (double)fileSize); // We cast to double so it won't overflow
			fileDict->SetBool("canBackup",
					  fileSize <= MAX_BACKUP_FILE_SIZE);

			list->SetDictionary(list->GetSize(), fileDict);
		}

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", collection);
		d->SetString("name", collection);
		d->SetList("referencedFiles", list);

		addedCollections->SetDictionary(addedCollections->GetSize(), d);
	}

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	CefRefPtr<CefDictionaryValue> out = CefDictionaryValue::Create();

	out->SetList("sceneCollections", addedCollections);

	output->SetDictionary(out);
}

void StreamElementsBackupManager::CreateLocalBackupPackage(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> in = input->GetDictionary();

	std::vector<std::string> requestCollections;
	std::vector<std::string> requestProfiles;
	bool includeReferencedFiles = true;

	CefRefPtr<CefListValue> addedCollections = CefListValue::Create();
	CefRefPtr<CefListValue> addedProfiles = CefListValue::Create();

	if (in->HasKey("includeReferencedFiles") &&
	    in->GetType("includeReferencedFiles") == VTYPE_BOOL)
		includeReferencedFiles = in->GetBool("includeReferencedFiles");

	if (in->HasKey("sceneCollections") &&
	    in->GetType("sceneCollections") == VTYPE_LIST) {
		ReadListOfIdsFromCefValue(in->GetValue("sceneCollections"),
					  requestCollections);
	} else {
		ReadListOfSceneCollectionIds(requestCollections);
	}

	if (in->HasKey("profiles") && in->GetType("profiles") == VTYPE_LIST) {
		ReadListOfIdsFromCefValue(in->GetValue("profiles"),
					  requestProfiles);
	} else {
		ReadListOfProfileIds(requestProfiles);
	}

	std::string backupPackagePath;

	if (!GetTemporaryFilePath("obs-live-backup", backupPackagePath))
		return;

	backupPackagePath += ".zip";

	char *basePathPtr = os_get_config_path_ptr("obs-studio");
	std::string basePath = basePathPtr;
	bfree(basePathPtr);

	zip_t *zip = zip_open(backupPackagePath.c_str(), 9, 'w');

	if (!zip)
		return;

	StreamElementsGlobalStateManager::GetInstance()
		->GetCleanupManager()
		->AddPath(backupPackagePath);

	for (auto profile : requestProfiles) {
		if (!AddProfileToZip(zip, basePath, profile))
			continue;

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", profile);
		d->SetString("name", profile);

		addedProfiles->SetDictionary(addedProfiles->GetSize(), d);
	}

	char timestampBuf[16];
	time_t time = std::time(nullptr);
	std::strftime(timestampBuf, sizeof(timestampBuf), "%Y%m%d%H%M%S",
		      std::localtime(&time));
	std::string timestamp = timestampBuf;

	for (auto collection : requestCollections) {
		if (!AddCollectionToZip(zip, basePath, collection,
					includeReferencedFiles, timestamp))
			continue;

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", collection);
		d->SetString("name", collection);

		addedCollections->SetDictionary(addedCollections->GetSize(), d);
	}

	zip_close(zip);

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	CefRefPtr<CefDictionaryValue> out = CefDictionaryValue::Create();

	out->SetList("profiles", addedProfiles);
	out->SetList("sceneCollections", addedCollections);
	out->SetString("url", CreateSessionSignedAbsolutePathURL(
				      myconv.from_bytes(backupPackagePath)));

	output->SetDictionary(out);
}

void StreamElementsBackupManager::QueryBackupPackageContent(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> in = input->GetDictionary();

	if (!in->HasKey("url") || in->GetType("url") != VTYPE_STRING)
		return;

	std::string url = in->GetString("url").ToString();

	std::string localPath;

	if (!GetLocalPathFromURL(url, localPath))
		return;

	zip_t *zip = zip_open(localPath.c_str(), 0, 'r');

	if (!zip)
		return;

	std::map<std::string, std::string> profiles;
	std::map<std::string, std::string> collections;

	for (int index = 0; index < zip_total_entries(zip) &&
			    0 == zip_entry_openbyindex(zip, index);
	     ++index) {
		const char *namePtr = zip_entry_name(zip);

		if (namePtr) {
			std::string name = namePtr;

			std::smatch match;

			if (std::regex_search(
				    name, match,
				    std::regex(
					    "^basic/profiles/(.+?)/basic.ini$"))) {
				profiles[match[1].str()] = name;
			} else if (std::regex_search(
					   name, match,
					   std::regex(
						   "^basic/scenes/(.+?)\\.json$"))) {
				collections[match[1].str()] = name;
			}
		}

		zip_entry_close(zip);
	}

	zip_close(zip);

	CefRefPtr<CefListValue> profilesList = CefListValue::Create();
	CefRefPtr<CefListValue> collectionsList = CefListValue::Create();

	for (auto item : profiles) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", item.first);
		d->SetString("name", item.first);

		profilesList->SetDictionary(profilesList->GetSize(), d);
	}

	for (auto item : collections) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", item.first);
		d->SetString("name", item.first);

		collectionsList->SetDictionary(collectionsList->GetSize(), d);
	}

	CefRefPtr<CefDictionaryValue> result = CefDictionaryValue::Create();

	std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

	result->SetString("url", CreateSessionSignedAbsolutePathURL(
					 myconv.from_bytes(localPath)));
	result->SetList("profiles", profilesList);
	result->SetList("sceneCollections", collectionsList);

	output->SetDictionary(result);
}

static bool IsQualifiedFileForRestore(
	std::string zipPath,
	std::unordered_map<std::string, bool> &requestProfiles,
	std::unordered_map<std::string, bool> &requestCollections)
{
	std::smatch match;

	if (std::regex_search(zipPath, match,
			      std::regex("^basic/profiles/(.+?)/"))) {
		if (!requestProfiles.empty()) {
			std::string id = match[1].str();

			if (requestProfiles.count(id))
				return true;
			else
				return false;
		} else
			return true;
	} else if (std::regex_search(
			   zipPath, match,
			   std::regex("^basic/scenes/(.+?)\\.json$"))) {
		if (!requestCollections.empty()) {
			std::string id = match[1].str();

			if (requestCollections.count(id))
				return true;
			else
				return false;
		} else
			return true;
	} else {
		return true;
	}
}

struct zip_extract_context_t {
	int handle;
};

static size_t HandleZipExtract(void *arg, unsigned long long offset,
			       const void *data, size_t size)
{
	zip_extract_context_t *context = (zip_extract_context_t *)arg;

	return ::write(context->handle, data, size);
};

void StreamElementsBackupManager::RestoreBackupPackageContent(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	/* Never allow restore during streaming or recording */
	if (obs_frontend_streaming_active() || obs_frontend_recording_active())
		return;

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> in = input->GetDictionary();

	std::unordered_map<std::string, bool> requestCollections;
	std::unordered_map<std::string, bool> requestProfiles;

	if (in->HasKey("sceneCollections") &&
	    in->GetType("sceneCollections") == VTYPE_LIST) {
		std::vector<std::string> ids;

		ReadListOfIdsFromCefValue(in->GetValue("sceneCollections"),
					  ids);

		for (auto id : ids) {
			requestCollections[id] = true;
		}
	}

	if (in->HasKey("profiles") && in->GetType("profiles") == VTYPE_LIST) {
		std::vector<std::string> ids;

		ReadListOfIdsFromCefValue(in->GetValue("profiles"), ids);

		for (auto id : ids) {
			requestProfiles[id] = true;
		}
	}

	if (!in->HasKey("url") || in->GetType("url") != VTYPE_STRING)
		return;

	std::string url = in->GetString("url").ToString();

    char *basePathPtr = os_get_config_path_ptr("obs-studio");
    std::string destBasePath = basePathPtr;
    bfree(basePathPtr);

    std::string outputPath = destBasePath.substr(
        0, destBasePath.size() -
               std::string("/obs-studio").size());

	std::string extractPath;

#ifdef WIN32
	if (!GetTemporaryFilePath("obs-live-restore-content", extractPath))
		return;

	StreamElementsGlobalStateManager::GetInstance()
		->GetCleanupManager()
		->AddPath(extractPath);

	std::string extractRootPath = extractPath + ".dir";

	extractPath = extractRootPath + "\\obs-studio";

	if (MKDIR_ERROR == os_mkdirs(extractPath.c_str()))
		return;
#else
    extractPath = destBasePath;
#endif
    
	std::string localPath;

	if (!GetLocalPathFromURL(url, localPath))
		return;

	zip_t *zip = zip_open(localPath.c_str(), 0, 'r');

	if (!zip)
		return;

	bool success = true;

	for (int index = 0; index < zip_total_entries(zip) &&
			    0 == zip_entry_openbyindex(zip, index) && success;
	     ++index) {
		const char *namePtr = zip_entry_name(zip);

		if (namePtr) {
			std::string name = namePtr;

			if (IsQualifiedFileForRestore(name, requestProfiles,
						      requestCollections)) {
				std::string destFilePath =
					extractPath + "/" + name;

				std::string extractDirPath =
					GetFolderPathFromFilePath(destFilePath);

				/* Create output base directory & extract file */
				if (MKDIR_ERROR ==
				    os_mkdirs(extractDirPath.c_str())) {
					/* Mkdir failed*/
					success = false;
				} else {
					std::wstring_convert<
						std::codecvt_utf8<wchar_t>>
						myconv;

					zip_extract_context_t context;

#ifdef WIN32
                    std::transform(
                        destFilePath.begin(),
                        destFilePath.end(),
                        destFilePath.begin(),
                        [](char ch) {
                            if (ch == '/')
                                return '\\';
                            else
                                return ch;
                        });

                    context.handle = _wopen(
						myconv.from_bytes(destFilePath)
							.c_str(),
						O_WRONLY | O_CREAT |
							O_BINARY,
						S_IREAD | S_IWRITE);
#else
                    context.handle = ::open(
                        destFilePath.c_str(),
                        O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);
#endif
                    
					if (-1 == context.handle) {
						success = false;
					} else {
						if (0 !=
						    zip_entry_extract(
							    zip,
							    HandleZipExtract,
							    &context)) {
							success = false;
						} else {
							/* Success */
						}

						::close(context.handle);
					}
				}
			}
		}

		zip_entry_close(zip);
	}

	zip_close(zip);

	if (!success)
		return;

	/* Replace file monikers for Scene Collections */

#ifdef WIN32
    if (ScanForFileReferencesMonikersToRestore(extractPath, destBasePath)) {
        std::string scriptPath;
        if (!GetTemporaryFilePath("obs-restore-script", scriptPath))
            return;

        std::string script = R"(
            void main() {
                wait_pid(${OBS_PID}, 0);

                uint64 count = 0;
                while (!filesystem_move("${SRC_PATH}\*", "${DEST_PATH}")) {
                    ++count;
                    if (count > 2) {
                        if (ui_confirm("${CONFIRM_STOP_MOVE_TEXT}", "${CONFIRM_STOP_MOVE_TITLE}"))
                            return;
                        else
                            count = 0;
                    }
                }

                shell_execute("${OBS_EXE_PATH}", "${OBS_ARGS}", "${OBS_EXE_FOLDER}");

                filesystem_delete("${SCRIPT_PATH}");
                filesystem_delete("${EXTRACT_ROOT_PATH}");
            }
        )";

        char obs_pid_buffer[32];
        ltoa(GetCurrentProcessId(), obs_pid_buffer, 10);

        std::string cwd;
        cwd.resize(MAX_PATH);

        wchar_t obs_path_utf16[MAX_PATH];
        GetModuleFileNameW(NULL, obs_path_utf16, MAX_PATH);

        std::wstring_convert<std::codecvt_utf8<wchar_t>> myconv;

        std::string obsArgs = "";

        {
            int argc;
            LPWSTR *wArgv =
                CommandLineToArgvW(GetCommandLineW(), &argc);

            for (int i = 1; i < argc; ++i) {
                std::string arg = myconv.to_bytes(wArgv[i]);

                if (arg.size() && arg.find_first_of(' ') >= 0) {
                    if (arg.substr(0, 1) != "\"")
                        arg = "\"" + arg;
                    if (arg.substr(arg.size() - 1, 1) !=
                        "\"")
                        arg += "\"";
                }

                if (obsArgs.size())
                    obsArgs += " ";

                obsArgs += arg;
            }

            LocalFree(wArgv);
        }

        std::map<std::string, std::string> vars;

        vars["OBS_PID"] = obs_pid_buffer;
        vars["SRC_PATH"] = extractPath;
        vars["DEST_PATH"] = outputPath;
        vars["CONFIRM_STOP_MOVE_TEXT"] = obs_module_text(
            "StreamElements.BackupRestore.MoveRestoredFilesAbortConfirmation.Text");
        vars["CONFIRM_STOP_MOVE_TITLE"] = obs_module_text(
            "StreamElements.BackupRestore.MoveRestoredFilesAbortConfirmation.Title");
        vars["OBS_EXE_PATH"] = myconv.to_bytes(obs_path_utf16);
        vars["OBS_ARGS"] = obsArgs;
        vars["OBS_EXE_FOLDER"] =
            os_getcwd((char *)cwd.data(), cwd.size());
        vars["SCRIPT_PATH"] = scriptPath;
        vars["EXTRACT_ROOT_PATH"] = extractRootPath;

        for (auto var : vars) {
            std::string regex = "\\$\\{" + var.first + "\\}";

            std::string val = "";

            for (char ch : var.second) {
                switch (ch) {
                case '\"':
                case '\'':
                case '\\':
                    val.push_back('\\');
                    val.push_back(ch);
                    break;
                case '\t':
                    val.push_back('\\');
                    val.push_back('t');
                    break;
                case '\r':
                    val.push_back('\\');
                    val.push_back('r');
                    break;
                case '\n':
                    val.push_back('\\');
                    val.push_back('n');
                    break;
                default:
                    val.push_back(ch);
                }
            }

            script = std::regex_replace(script, std::regex(regex),
                            val);
        }

        /* Spawn move backup to config process */

        std::string scriptHostExePath =
            obs_get_module_binary_path(obs_current_module());
        scriptHostExePath = scriptHostExePath.substr(
            0, scriptHostExePath.find_last_of('/') + 1);

        scriptHostExePath +=
            "obs-browser-streamelements-restore-script-host.exe";

        std::transform(scriptHostExePath.begin(),
                   scriptHostExePath.end(),
                   scriptHostExePath.begin(),
                   [](char ch) { return ch == '/' ? '\\' : ch; });

        if (!os_quick_write_utf8_file(scriptPath.c_str(),
                          script.c_str(), script.size(),
                          false))
            return;

        std::string command = "\"";
        command += scriptHostExePath;
        command += "\" \"";
        command += scriptPath;
        command += "\"";

        QProcess proc;
        if (proc.startDetached(QString(command.c_str()))) {
            success = true;

            /* Cleanup temporary resources */

            StreamElementsGlobalStateManager::GetInstance()
                ->GetCleanupManager()
                ->Clean();

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
            if (!TerminateProcess(GetCurrentProcess(), 0)) {
                /* Backup shutdown sequence */
                QApplication::quit();
            }
        }
    }
#else
    if (ScanForFileReferencesMonikersToRestore(extractPath, destBasePath)) {
        /* Cleanup temporary resources */

        StreamElementsGlobalStateManager::GetInstance()
            ->GetCleanupManager()
            ->Clean();

	RestartCurrentApplication();

	success = true;
    }
#endif

	output->SetBool(success);
}
