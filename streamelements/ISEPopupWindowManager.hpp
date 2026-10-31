#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the pop-up window manager: one method,
// behind openPopupWindow.
//
// First of part 2's interfaces, and it is the smallest on purpose. The
// point of the exercise is not this method -- it is that
// SEApiMessageHandler.cpp can now name the pop-up window manager without
// including SEPopupWindowManager.hpp, whose QPointer<QMainWindow> and
// QPointer<SEBrowserWidget> members put Qt into the handler's include
// closure for a service the handler only ever hands a CefValue to.
//
// The concrete class keeps its lifecycle surface -- a constructor taking
// the bundle weakly and a destructor that shuts the windows' browsers down
// -- because only the root calls those, and an API-facing contract is the
// wrong place for them.
//
class ISEPopupWindowManager {
public:
	virtual ~ISEPopupWindowManager() {}

	//
	// Opens a top-level window with a browser in it, from a serialized
	// description. Answers whether it opened one.
	//
	virtual bool DeserializePopupWindow(CefRefPtr<CefValue> input) = 0;
};
