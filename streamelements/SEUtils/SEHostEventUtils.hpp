#pragma once

//
// Pushing events and messages to browser pages: globally, to one container,
// or to one client, plus the two advisories that announce a change in
// user-interface state and hotkey bindings.
//
// These still reach SEGlobalStateManager::GetInstance() for the websocket
// server. Replacing that with an injected dispatcher is phase 2 of this
// work; this file is a move.
//

#include <cef-headers.hpp>

#include <string>

void DispatchClientMessage(std::string target,
			   CefRefPtr<CefProcessMessage> msg);

void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson);

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson);

void AdviseHostUserInterfaceStateChanged();
void AdviseHostHotkeyBindingsChanged();
