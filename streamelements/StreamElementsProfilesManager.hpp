#pragma once

#include "cef-headers.hpp"
#include <mutex>

class StreamElementsProfilesManager {
public:
	StreamElementsProfilesManager();
	~StreamElementsProfilesManager();

public:
	void SerializeAllProfiles(CefRefPtr<CefValue> &output);
	void SerializeCurrentProfile(CefRefPtr<CefValue> &output);
	bool DeserializeCurrentProfileById(CefRefPtr<CefValue> input);

private:
	std::recursive_mutex m_mutex;
};
