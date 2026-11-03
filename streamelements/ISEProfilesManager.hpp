#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the profiles manager: read the profile
// list and the current profile, and switch to one by id.
//
// Switching a profile tears down and rebuilds most of the plug-in's
// state, which is why the page-facing surface is deliberately this
// small.
//
class ISEProfilesManager {
public:
	virtual ~ISEProfilesManager() {}

	//
	// Every OBS profile on this installation.
	//
	virtual void SerializeAllProfiles(CefRefPtr<CefValue> &output) = 0;

	//
	// The one currently loaded.
	//
	virtual void SerializeCurrentProfile(CefRefPtr<CefValue> &output) = 0;

	//
	// Switches to the profile with that id.
	//
	virtual bool
	DeserializeCurrentProfileById(CefRefPtr<CefValue> input) = 0;
};
