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

//
// os_file_exists() without the libobs leak on empty input. Use this wherever
// the path comes from data rather than from code.
//
// os_utf8_to_wcs_ptr() always bmalloc()s its output buffer and then returns
// the LENGTH of the conversion, which is zero for an empty string. So
// os_file_exists()'s own guard --
//
//	if (!os_utf8_to_wcs_ptr(path, 0, &path_utf16))
//		return false;
//
// reads that zero as failure and returns before its bfree(), leaking one
// allocation per call. Upstream, in libobs/util/platform-windows.c.
//
// It matters here because the backup scan asks this question about every
// string in a scene collection, and scene collections contain empty strings:
// measured at 37 leaked allocations for one
// queryUserEnvironmentBackupReferencedFiles call, which is three empty
// strings in each of fourteen collections. An empty path is not a file, so
// answering it here is both correct and one syscall cheaper.
//
// The leak is Windows-only -- only that platform converts to UTF-16 first,
// and platform-nix.c is a bare access(path, F_OK) -- but the empty-path
// check is deliberately not guarded by platform. Both libobs bodies
// already answer false for an empty path (access("", F_OK) fails with
// ENOENT), so answering it here returns exactly what os_file_exists()
// returned, on every platform. A guard would buy nothing, and would leave
// the same question answered two different ways depending on where it
// compiled.
//
bool FileExists(std::string path);
