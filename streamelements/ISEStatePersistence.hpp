#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from state persistence: the user interface
// state, read and written, and a request to persist the layout.
//
// PersistState is the most called method on this whole contract -- some
// two dozen handler sites -- because sixteen dock handlers end in
// "rebuild the menu and persist the layout". They do not all agree on
// when: three different rules are in play, and the contract table pins
// each of them.
//
// The serializer methods that walk the widget, worker and controls
// managers stay concrete: this class borrows those three as raw
// pointers, and only the root's construction and destruction order
// makes that safe.
//
class ISEStatePersistence {
public:
	virtual ~ISEStatePersistence() {}

	//
	// The persisted UI state blob a page can read back.
	//
	virtual void
	SerializeUserInterfaceState(CefRefPtr<CefValue> &output) = 0;

	//
	// Replaces it.
	//
	virtual bool
	DeserializeUserInterfaceState(CefRefPtr<CefValue> input) = 0;

	//
	// Writes the current layout to the profile. sendEventToGuest controls
	// whether pages are told it changed.
	//
	// The default argument is spelled here and on the implementation both
	// -- SEWidgetManager holds this service concretely and weakly for its
	// two deferred saves and calls PersistState() with no argument. See
	// the note on ISESessionManager::Reset.
	//
	virtual void PersistState(bool sendEventToGuest = true) = 0;
};
