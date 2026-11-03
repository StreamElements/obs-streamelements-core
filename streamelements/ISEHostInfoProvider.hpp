#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the host info provider: five read-only
// descriptions of the host and the machine, one per method.
//
// All five are pure reads, which makes this the plainest interface of
// the set and the easiest to fake.
//
class ISEHostInfoProvider {
public:
	virtual ~ISEHostInfoProvider() {}

	//
	// The host dictionary getHostProperties answers with.
	//
	virtual void SerializeHostProperties(CefRefPtr<CefValue> &output) = 0;

	//
	// Every OBS module loaded into this process.
	//
	virtual void
	SerializeLoadedHostModules(CefRefPtr<CefValue> &output) = 0;

	//
	// Per-core user and kernel times.
	//
	virtual void
	SerializeSystemCPUUsageTimes(CefRefPtr<CefValue> &output) = 0;

	//
	// CPU, GPU and memory description of the machine.
	//
	virtual void
	SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output) = 0;

	//
	// Current physical and virtual memory use.
	//
	virtual void
	SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output) = 0;
};
