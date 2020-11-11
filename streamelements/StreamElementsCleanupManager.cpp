#include "StreamElementsCleanupManager.hpp"

#include <sys/stat.h>
#include <obs.h>
#include <util/platform.h>

StreamElementsCleanupManager::StreamElementsCleanupManager()
{
}

StreamElementsCleanupManager ::~StreamElementsCleanupManager()
{
	Clean();
}

void StreamElementsCleanupManager::AddPath(std::string path)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	m_paths.push_back(path);
}

void StreamElementsCleanupManager::Clean()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_paths.empty())
		return;

	for (auto path : m_paths) {
		if (!os_file_exists(path.c_str()))
			continue;

		struct stat st;

		if (os_stat(path.c_str(), &st) < 0)
			continue;

		if (st.st_mode & S_IFDIR) {
			if (0 == os_rmdir(path.c_str())) {
				blog(LOG_INFO,
				     "obs-browser: StreamElementsCleanupManager: removed folder: %s",
				     path.c_str());
			} else {
				blog(LOG_ERROR,
				     "obs-browser: StreamElementsCleanupManager: failed removing folder: %s",
				     path.c_str());
			}
		} else {
			if (0 == os_unlink(path.c_str())) {
				blog(LOG_INFO,
				     "obs-browser: StreamElementsCleanupManager: removed file: %s",
				     path.c_str());
			} else {
				blog(LOG_ERROR,
				     "obs-browser: StreamElementsCleanupManager: failed removing file: %s",
				     path.c_str());
			}
		}
	}

	m_paths.clear();
}
