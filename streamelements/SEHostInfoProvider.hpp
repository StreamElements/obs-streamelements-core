#pragma once

//
// A read-only description of the host and the machine it is running on:
// versions, platform, CPU and memory usage, hardware, and the loaded OBS
// modules.
//
// Five host API methods built this by hand. getHostProperties in particular
// stitched together obs_get_version_string(), four CEF and plug-in version
// helpers, a platform string from an #ifdef chain, a pointer-size check and
// two values from the analytics manager -- in a handler body, which is the
// one place in the plug-in where that kind of assembly is hardest to reuse
// or to read.
//
// It holds the analytics events manager, because the machine and session
// identifiers are its, and they belong in the same answer as the rest.
//

#include "cef-headers.hpp"

#include <memory>

class SEAnalyticsEventsManager;

class SEHostInfoProvider {
public:
	SEHostInfoProvider(std::shared_ptr<SEAnalyticsEventsManager>
				   analyticsEventsManager);
	~SEHostInfoProvider();

public:
	//
	// Versions, platform, architecture, and the machine and session ids.
	//
	void SerializeHostProperties(CefRefPtr<CefValue> &output);

	//
	// The three system readings and the module list. Each one was a
	// handler body calling a single free helper; they are here so that
	// "what can the host tell a page about itself" is one surface rather
	// than five unrelated calls.
	//
	void SerializeSystemCPUUsageTimes(CefRefPtr<CefValue> &output);
	void SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output);
	void SerializeSystemHardwareProperties(CefRefPtr<CefValue> &output);
	void SerializeLoadedHostModules(CefRefPtr<CefValue> &output);

private:
	std::shared_ptr<SEAnalyticsEventsManager> m_analyticsEventsManager;
};
