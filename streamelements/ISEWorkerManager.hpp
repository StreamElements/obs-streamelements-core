#pragma once

#include "cef-headers.hpp"

#include <string>
#include <vector>

//
// What the host API needs from the background worker manager: add one,
// remove one, list them.
//
// The rest of the concrete class is the root's -- RemoveAll, the
// whole-state Serialize/Deserialize pair SEStatePersistence drives, and
// OnObsExit from its SEObsAppMonitor base. That base is also why the
// implementation header cannot be made obs-free on its own, and so why an
// interface is the route here rather than cleaning its includes.
//
class ISEWorkerManager {
public:
	virtual ~ISEWorkerManager() {}

	virtual std::string DeserializeOne(CefRefPtr<CefValue> input) = 0;

	virtual void Remove(std::string id) = 0;

	virtual void Serialize(CefRefPtr<CefValue> &output) = 0;

	//
	// Both called by SESessionManager rather than by a handler: signing
	// out drops every worker, having first listed them to shut each one
	// down. On the contract for the same reason as above -- the root is
	// not their only caller.
	//
	virtual void RemoveAll() = 0;

	virtual void GetIdentifiers(std::vector<std::string> &result) = 0;
};
