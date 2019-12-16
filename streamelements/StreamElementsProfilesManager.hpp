#pragma once

#include "cef-headers.hpp"

class StreamElementsProfilesManager {
public:
	StreamElementsProfilesManager();
	~StreamElementsProfilesManager();

public:
	void SerializeAllProfiles(CefRefPtr<CefValue> &output);
	void SerializeCurrentProfile(CefRefPtr<CefValue> &output);
	bool DeserializeCurrentProfileById(CefRefPtr<CefValue> input);
};
