#pragma once

#include "cef-headers.hpp"

#include <string>

//
// What the host API needs from the shell services: hand a URL or a path
// to the desktop shell.
//
// Two methods with one concern -- give something to the host operating
// system and let it decide what to do with it -- and no owner at all
// before this class existed.
//
// RevealFileInGraphicalShell is the one method in this set whose macOS
// behaviour changed recently: its branch was guarded on `#ifdef APPLE`,
// a CMake variable the compiler never sees, so it had never been
// compiled and answered false on macOS without doing anything.
//
class ISEShellServices {
public:
	virtual ~ISEShellServices() {}

	//
	// Opens a URL in the user's default browser.
	//
	virtual bool OpenDefaultBrowser(std::string url) = 0;

	//
	// Reveals a path in Explorer or Finder.
	//
	virtual void
	RevealFileInGraphicalShell(CefRefPtr<CefValue> input,
				   CefRefPtr<CefValue> &output) = 0;
};
