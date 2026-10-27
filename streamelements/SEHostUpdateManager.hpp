#pragma once

//
// The release channel this installation follows, and asking the updater to
// look for a new build.
//
// Three host API methods and one menu action were the whole of it, spread
// across two files. The two property methods read and write the product
// environment configuration directly; the third built a calldata and fired
// `streamelements_request_check_for_updates_silent` on the global obs signal
// handler from inside a handler body, and SEMenuManager fired the non-silent
// twin of the same signal from its own QAction.
//
// So two places knew the update signal names and the calldata keys, and
// neither of them was about updates. One class knows them now, which is also
// the only libobs the host API surface had left here.
//
// Depends on nothing: the configuration is in the registry (Windows) or a
// plist (macOS), and the signal handler is global. It is a service so that a
// handler body can reach it without naming calldata_t.
//

#include "cef-headers.hpp"

class SEHostUpdateManager {
public:
	SEHostUpdateManager();
	~SEHostUpdateManager();

public:
	//
	// { quality, manifestUrl }, or null when either is unset -- which is
	// what the handler answered before, rather than a half-filled
	// dictionary.
	//
	void SerializeReleaseGroupProperties(CefRefPtr<CefValue> &output);

	//
	// Both keys or neither: output is left untouched unless the input
	// carries both as strings, exactly as the handler body decided.
	//
	void DeserializeReleaseGroupProperties(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	//
	// Silent: the updater checks and installs without putting anything up
	// in front of the user. This is what a page asks for.
	//
	void RequestCheckForUpdates(bool allowDowngrade, bool forceInstall,
				    bool allowUseLastResponse);

	//
	// Interactive: the updater shows its own UI. This is what the menu
	// action asks for, and it is a separate signal rather than a flag.
	//
	void RequestCheckForUpdatesInteractive();
};
