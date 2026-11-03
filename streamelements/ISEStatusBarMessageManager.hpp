#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the status bar message manager: one
// call, behind showStatusBarTemporaryMessage.
//
// The smallest of the set, and the first extraction of the whole
// refactor (C2) precisely because it owns nothing and needed only the
// main window.
//
class ISEStatusBarMessageManager {
public:
	virtual ~ISEStatusBarMessageManager() {}

	//
	// Shows a message in OBS's status bar for a bounded time. Answers
	// whether the description was usable.
	//
	virtual bool
	DeserializeStatusBarTemporaryMessage(CefRefPtr<CefValue> input) = 0;
};
