#pragma once

#include "cef-headers.hpp"

#include <memory>
#include <string>

class SEWebsocketApiServer;

//
// Host events on their way to browser pages: globally, to one container, or
// to one client, plus the two advisories that announce a change in
// user-interface state and in hotkey bindings.
//
// These were free functions in SEUtils that fetched the websocket server
// from the singleton on every call, which is how a scene change, an output
// signal, a hotkey and a dialog closing all ended up reaching global state.
// The surface is three methods wide because that is all the server exposes
// for it -- DispatchJSEvent, DispatchTargetJSEvent and
// DispatchTargetClientMessage -- so a consumer can be handed a recording
// stand-in and asserted against.
//
// Holding the server is not a ring: nothing the server owns owns this.
//
class SEHostEventDispatcher
	: public std::enable_shared_from_this<SEHostEventDispatcher> {
public:
	SEHostEventDispatcher(std::shared_ptr<SEWebsocketApiServer> apiServer);
	~SEHostEventDispatcher();

	void DispatchJSEventGlobal(std::string event,
				   std::string eventArgsJson);

	void DispatchJSEventContainer(std::string target, std::string event,
				      std::string eventArgsJson);

	void DispatchClientMessage(std::string target,
				   CefRefPtr<CefProcessMessage> msg);

	//
	// Both debounce by 250ms on the Qt thread, so they can be called from
	// anything that nudges the window layout without flooding the page.
	//
	// The timer holds a weak reference to this object rather than a raw
	// one: a queued advisory that fires after teardown has nothing to
	// announce, and that is what the availability check inside the old
	// free functions was standing in for.
	//
	void AdviseHostUserInterfaceStateChanged();
	void AdviseHostHotkeyBindingsChanged();

private:
	std::shared_ptr<SEWebsocketApiServer> m_apiServer;
};
