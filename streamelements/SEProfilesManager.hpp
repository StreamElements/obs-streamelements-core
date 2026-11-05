#pragma once

#include "ISEProfilesManager.hpp"

#include "cef-headers.hpp"
#include <mutex>

class SEProfilesManager : public ISEProfilesManager {
public:
	SEProfilesManager();
	~SEProfilesManager();

public:
	void SerializeAllProfiles(CefRefPtr<CefValue> &output) override;
	void SerializeCurrentProfile(CefRefPtr<CefValue> &output) override;
	bool DeserializeCurrentProfileById(CefRefPtr<CefValue> input) override;

private:
	std::recursive_mutex m_mutex;
};
