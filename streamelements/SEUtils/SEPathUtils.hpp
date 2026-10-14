#pragma once

//
// Filesystem paths, temporary files, and the lists of OBS scene collections
// and profiles read off disk.
//

#include <map>
#include <string>

bool GetTemporaryFilePath(std::string prefixString, std::string &result);
std::string GetUniqueFileNameFromPath(std::string path, size_t maxLength);
std::string GetFolderPathFromFilePath(std::string filePath);

bool ReadListOfObsSceneCollections(std::map<std::string, std::string> &output);
bool ReadListOfObsProfiles(std::map<std::string, std::string> &output);

bool IsSafeFileExtension(std::string path);
bool SecureJoinPaths(std::string base, std::string subpath,
		     std::string &result);
