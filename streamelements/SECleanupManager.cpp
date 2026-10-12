#include "SECleanupManager.hpp"

#include <sys/stat.h>
#include <obs.h>
#include <util/platform.h>

SECleanupManager::SECleanupManager()
{
}

SECleanupManager ::~SECleanupManager()
{
	Clean();
}

void SECleanupManager::AddPath(std::string path)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	m_paths[path] = true;
}

void SECleanupManager::Clean()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	for (auto kv : m_paths) {
		std::string path = kv.first;

		if (!os_file_exists(path.c_str()))
			continue;

		struct stat st;

		if (os_stat(path.c_str(), &st) < 0)
			continue;

		if (st.st_mode & S_IFDIR) {
			if (0 == os_rmdir(path.c_str())) {
				blog(LOG_INFO,
				     "obs-streamelements-core: SECleanupManager: removed folder: %s",
				     path.c_str());
			} else {
				blog(LOG_ERROR,
				     "obs-streamelements-core: SECleanupManager: failed removing folder: %s",
				     path.c_str());
			}
		} else {
			if (0 == os_unlink(path.c_str())) {
				blog(LOG_INFO,
				     "obs-streamelements-core: SECleanupManager: removed file: %s",
				     path.c_str());
			} else {
				blog(LOG_ERROR,
				     "obs-streamelements-core: SECleanupManager: failed removing file: %s",
				     path.c_str());
			}
		}
	}

	m_paths.clear();
}
