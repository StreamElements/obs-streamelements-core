#pragma once

#include "cef-headers.hpp"

#include <string>

//
// What container consumers need from the host event dispatcher: announce a
// host event to every page, to one page, or hand one page a message, plus
// the debounced UI-state advisory.
//
// Three consumers between them, and the union is why this interface has
// four methods rather than one:
//
//   SENativeObsUIControlsManager  DispatchJSEventGlobal
//   SEBrowserWidget              DispatchJSEventContainer,
//                                DispatchClientMessage
//   SEWidgetManager's
//     TrackedDockWidget          AdviseHostUserInterfaceStateChanged
//
// AdviseHostHotkeyBindingsChanged is deliberately absent. It exists on the
// concrete class and has exactly one caller, SEHotkeyManager, which holds
// the dispatcher as its own member injected by the root -- so it is not on
// the container's surface and does not belong on the container's contract.
// That is the same rule the other interfaces follow: what only the root
// wires up stays concrete.
//
// Fourteen other classes hold this service concretely, all of them handed
// it by the root. They are unaffected: an interface on the container field
// changes only the handles that come FROM the container, which are
// SEBrowserWidget's weak member and TrackedDockWidget's constructor
// parameter.
//
class ISEHostEventDispatcher {
public:
	virtual ~ISEHostEventDispatcher() {}

	//
	// Announces a host event to every connected page.
	//
	virtual void DispatchJSEventGlobal(std::string event,
					   std::string eventArgsJson) = 0;

	//
	// Announces one to a single page, named by its client id.
	//
	virtual void DispatchJSEventContainer(std::string target,
					      std::string event,
					      std::string eventArgsJson) = 0;

	//
	// Hands one page a message rather than an event.
	//
	virtual void
	DispatchClientMessage(std::string target,
			      CefRefPtr<CefProcessMessage> msg) = 0;

	//
	// Tells pages the host UI state changed. Debounced by 250ms on the Qt
	// thread, so a dock being dragged does not flood them, and the timer
	// holds this object weakly -- an advisory that fires after teardown
	// has nothing left to announce.
	//
	virtual void AdviseHostUserInterfaceStateChanged() = 0;
};
