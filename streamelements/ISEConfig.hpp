#pragma once

#include "cef-headers.hpp"

#include <cstdint>
#include <string>

//
// What the host API needs from the plug-in's own ini.
//
// The concrete SEConfig is far larger than this: a config_t * handle with an
// open/close lifecycle, six typed read/write helpers, and about twenty
// named accessors for settings nothing outside the plug-in ever asks about
// -- the crash-report contact details, the auxiliary menu configuration, the
// startup state string. None of that is on the contract, because no consumer
// of the services container reaches it.
//
// The startup flags are, though, and they are the reason this interface is
// not just four scoped-storage methods: the bits are what a page sets when
// onboarding finishes and what SESessionManager clears when the account signs
// out, so the flag values belong to the contract and not to one
// implementation of it. SEConfig inherits them, so the seven sites spelling
// SEConfig::STARTUP_FLAGS_SIGNED_IN keep resolving unchanged.
//
class ISEConfig {
public:
	virtual ~ISEConfig() {}

public:
	//
	// constexpr rather than const: a static constexpr data member is
	// implicitly inline in C++17, so a fake config in a test needs no
	// definition of its own for these.
	//
	static constexpr uint64_t STARTUP_FLAGS_ONBOARDING_MODE =
		0x0000000000000001ULL;
	static constexpr uint64_t STARTUP_FLAGS_SIGNED_IN =
		0x0000000000000002ULL;

public:
	//
	// Note the width mismatch, which is older than this interface: the
	// flags are uint64_t and these two are int. Both bits in use fit, so
	// nothing is lost today.
	//
	virtual int GetStartupFlags() = 0;

	//
	// Saves the ini and publishes the new system state on the message bus,
	// which is the one thing SEConfig needs the bus for.
	//
	virtual void SetStartupFlags(int value) = 0;

	//
	// The onboarding URL: the command line first, then the product
	// environment configuration, then a built-in default. Read by
	// SESessionManager when it starts the onboarding UI.
	//
	virtual std::string GetUrlOnBoarding() = 0;

public:
	//
	// Scoped JSON storage -- a small per-scope, per-container key/value
	// store under the plug-in's config directory, which pages use to keep
	// their own state across restarts.
	//
	virtual void ReadScopedJsonFile(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;
	virtual void WriteScopedJsonFile(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) = 0;
	virtual void ReadScopedJsonFilesList(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output) = 0;
	virtual void RemoveScopedJsonFile(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) = 0;
};
