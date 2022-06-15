#pragma once

#include <string>
#include <map>
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
	std::map<std::string,bool> m_paths;

	std::mutex m_mutex;
};
