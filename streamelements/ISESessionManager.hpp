#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the session manager: sign out, and drop
// cookies.
//
// UiModifier lives here rather than on the concrete class because
// Reset() takes one and Reset() is on this contract. It is inherited,
// so every existing SESessionManager::OnBoarding spelling keeps working
// -- the same move ISEMessageBus made with its destination flags and
// ISEConfig with its startup flags.
//
// Both methods are destructive on a live profile: they clear
// STARTUP_FLAGS_SIGNED_IN, drop every dock and push the onboarding URL.
// That is why no live run in this series has called either, and why the
// contract table is their only coverage.
//
// The reset is one state machine -- dispatch hostStateReset, optionally
// clear cookies, reset the menu, then start the onboarding UI -- and
// the steps only the root drives stay concrete.
//
class ISESessionManager {
public:
	virtual ~ISESessionManager() {}

	//
	// Which UI the reset lands the user in. Default is the ordinary
	// signed-out shell; OnBoarding and Import push their own flows.
	//
	enum UiModifier { Default = 0, OnBoarding = 1, Import = 2 };

	//
	// Signs out: clears the signed-in startup flag, drops the docks, and
	// starts the UI the modifier names.
	//
	// The default arguments are spelled here and on the implementation
	// both, because SEMenuManager calls Reset() through a concrete handle
	// while the handler calls it through this interface -- and a derived
	// declaration without them would hide the base's and break the first.
	// check_contract_default_arguments_agree_with_their_overrides is what
	// keeps the two copies in step.
	//
	virtual void Reset(bool deleteAllCookies = true,
			   UiModifier uiModifier = Default) = 0;

	//
	// Drops every cookie in the CEF cookie store.
	//
	virtual void DeleteCookies() = 0;
};
