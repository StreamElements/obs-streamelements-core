#pragma once

#include "cef-headers.hpp"

#include <cstdint>

//
// What the host API needs from the hotkey manager. The concrete class also
// registers OBS hotkeys and routes presses, which only it and the root
// touch, so none of that belongs here.
//
class ISEHotkeyManager {
public:
	virtual ~ISEHotkeyManager() {}

	virtual bool DeserializeHotkeyTriggers(CefRefPtr<CefValue> input) = 0;

	//
	// The new binding's id, or -1. This returned obs_hotkey_id -- a
	// size_t -- until 9dbf8d7, where returning false on the failure path
	// handed the page the perfectly valid id 0.
	//
	virtual int64_t
	DeserializeSingleHotkeyBinding(CefRefPtr<CefValue> input) = 0;

	virtual bool RemoveHotkeyBindingById(uint64_t id) = 0;

	//
	// Called by SESessionManager, not by any handler: signing out drops
	// every binding the page registered. It is on this contract rather
	// than off it because the rule is that methods ONLY the root calls
	// have no place here, and a second container consumer calls this one.
	//
	virtual void RemoveAllManagedHotkeyBindings() = 0;

	//
	// The default argument lives here and nowhere else. A default is
	// bound statically, so the same one written on both the interface and
	// the implementation is a trap waiting for the two to drift. The one
	// caller that does not come through this interface, in
	// SEStatePersistence, passes it explicitly.
	//
	virtual bool
	SerializeHotkeyBindings(CefRefPtr<CefValue> &result,
				bool onlyManagedBindings = false) = 0;
};
