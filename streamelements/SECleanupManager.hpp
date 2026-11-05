#pragma once

#include "ISECleanupManager.hpp"

#include <string>
#include <map>
#include <mutex>

class SECleanupManager : public ISECleanupManager {
public:
	SECleanupManager();
	~SECleanupManager();

public:
	void AddPath(std::string path) override;

public:
	void Clean();

private:
	std::map<std::string,bool> m_paths;

	std::mutex m_mutex;
};
