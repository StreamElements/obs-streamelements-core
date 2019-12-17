#pragma once

#include <string>
#include <vector>
#include <mutex>

class StreamElementsCleanupManager {
public:
	StreamElementsCleanupManager();
	~StreamElementsCleanupManager();

public:
	void AddPath(std::string path);

public:
	void Clean();

private:
	std::vector<std::string> m_paths;

	std::mutex m_mutex;
};
