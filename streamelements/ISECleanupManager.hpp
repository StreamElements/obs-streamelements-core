#pragma once

#include <string>

//
// What a container consumer needs from the cleanup manager: register a path
// to be removed later. SEBrowserWidget is the only caller, and it registers
// the per-process cookie store an incognito widget creates.
//
// Clean() stays off this contract. It is the other half of the pair and
// only the root calls it, at the point where deleting those paths is
// safe -- a page asking for its own cookie store to be wiped mid-session
// is not something this surface should make expressible.
//
// Like ISEAnalyticsEventsManager, no handler body reaches this service; it
// is on the container for one non-handler consumer.
//
class ISECleanupManager {
public:
	virtual ~ISECleanupManager() {}

	//
	// Registers a filesystem path for deletion during cleanup. Idempotent
	// -- the same path added twice is still removed once.
	//
	virtual void AddPath(std::string path) = 0;
};
