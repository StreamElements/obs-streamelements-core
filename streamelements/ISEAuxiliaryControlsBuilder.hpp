#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the auxiliary controls builder: the one
// call behind showPopupMenuAtMousePointerPosition.
//
// Everything else the builder does -- dock menus, scene and scene-item
// auxiliary widgets, the docks menu -- is driven by SEMenuManager and
// SEWidgetManager through concrete handles, so it stays off this
// contract.
//
// Note that the method blocks: it calls QMenu::exec(), which does not
// return until a human dismisses the menu. That is why probing this one
// over CDP would hang OBS, and why the contract table is its only
// coverage.
//
class ISEAuxiliaryControlsBuilder {
public:
	virtual ~ISEAuxiliaryControlsBuilder() {}

	//
	// Builds a menu from a serialized description and shows it at the
	// mouse pointer. Answers whether it built one; blocks while it is
	// open.
	//
	virtual bool
	ShowMenuAtMousePointerPosition(CefRefPtr<CefValue> input) = 0;
};
