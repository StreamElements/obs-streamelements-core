#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the host update manager: the release
// group properties, read and written, plus a silent update check.
//
// This class is the only one in the plug-in that knows the update
// signal names. RequestCheckForUpdatesInteractive(), which
// SEMenuManager's own QAction fires, stays concrete -- a menu action is
// not a page.
//
class ISEHostUpdateManager {
public:
	virtual ~ISEHostUpdateManager() {}

	//
	// The configured release channel and manifest URL.
	//
	virtual void
	SerializeReleaseGroupProperties(CefRefPtr<CefValue> &output) = 0;

	//
	// Writes them. Changes which channel this install updates from.
	//
	virtual void
	DeserializeReleaseGroupProperties(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) = 0;

	//
	// Fires the silent check-for-updates signal.
	//
	virtual void RequestCheckForUpdates(bool allowDowngrade,
					    bool forceInstall,
					    bool allowUseLastResponse) = 0;
};
