#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the SE.Live menu: the auxiliary items a page
// describes, the built-in-items flag, and a rebuild.
//
// Update() is on the contract and not off it for the usual reason -- what
// stays off is what only the root calls, and this one has three other
// callers, seventeen of them inside handler bodies: nearly every handler
// that changes a dock, a worker or the signed-in state finishes by rebuilding
// the menu. Reset() is SESessionManager's.
//
// The concrete class is a QObject holding a QPointer<QMenu> it does not own,
// a QMainWindow and six services; none of that is anything a consumer of the
// services container touches.
//
class ISEMenuManager {
public:
	virtual ~ISEMenuManager() {}

	//
	// Rebuilds the menu from the current configuration. Cheap enough to
	// call after any change, which is how it is used.
	//
	virtual void Update() = 0;

	virtual bool
	DeserializeAuxiliaryMenuItems(CefRefPtr<CefValue> input) = 0;
	virtual void
	SerializeAuxiliaryMenuItems(CefRefPtr<CefValue> &output) = 0;

	virtual void SetShowBuiltInMenuItems(bool show) = 0;
	virtual bool GetShowBuiltInMenuItems() = 0;

	//
	// Signing out: back to the menu an unsigned-in OBS gets.
	//
	virtual void Reset() = 0;
};
