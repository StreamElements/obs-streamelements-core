#pragma once

//
// Handing something to the desktop shell: a URL to the default browser, a
// path to the file manager.
//
// Neither had an owner. openDefaultBrowser built a QUrl and called
// QDesktopServices::openUrl inside a handler body -- all six remaining Qt
// tokens in the host API surface -- and openFileLocationInHostFileManager
// went through a free function in the Qt widget-utils domain, which had
// nothing else in it. Same concern either way: ask the platform to open
// something, outside our own process.
//
// Nothing here needs any of the plug-in's own state, so it takes no
// dependencies. It is a service rather than two free functions because the
// point of part 1 is that a handler body names no Qt type, and a free
// function's declaration would still have to.
//

#include "cef-headers.hpp"

#include <string>

class SEShellServices {
public:
	SEShellServices();
	~SEShellServices();

public:
	//
	// Parsed in QUrl::TolerantMode, as the handler body did: the URL
	// comes from a page and is not necessarily well formed.
	//
	// Returns true whether or not the shell accepted it, which is what
	// the handler reported before. QDesktopServices::openUrl's own result
	// was discarded, and a page that has been told `true` since this
	// method existed should not start seeing `false` because the code
	// moved.
	//
	bool OpenDefaultBrowser(std::string url);

	//
	// Shows the file in the platform's file manager, selected where the
	// platform can do that. output is a bool: whether the file manager
	// was launched.
	//
	void RevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output);
};
