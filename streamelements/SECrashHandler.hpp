#pragma once

//
// Crash-reporting backend interface.
//
// The concrete implementation is chosen at build time by the
// STREAMELEMENTS_CRASH_HANDLER CMake variable, which both selects the sources
// that compile and defines SE_CRASH_HANDLER_<BACKEND>. Exactly one
// implementation is ever linked, so two backends can never contend for the
// process-wide exception filter.
//
#include <memory>

class SEPerformanceHistoryTracker;
class SEAnalyticsEventsManager;
class SEConfig;

class SECrashHandler {
public:
	//
	// The config is taken at construction, not through
	// SetReportingServices() below, because a backend reads the contact
	// details a previous report left behind in its own constructor -- see
	// SESentryCrashHandler. It is stored by the base so that the derived
	// constructor body can already see it.
	//
	SECrashHandler(std::shared_ptr<SEConfig> config) : m_config(config) {}

	virtual ~SECrashHandler() {}

public:
	//
	// The two services a report is built from: the performance tracker the
	// CPU and memory histograms come from, and the analytics manager the
	// crash event is tracked through.
	//
	// Held strongly, and deliberately: this is the one path where asking
	// whether the plug-in is still available gives the wrong answer. A
	// crash while OBS is tearing down is exactly the crash worth
	// reporting, and both of these were fetched from the singleton at
	// crash time until CORE-757 -- one behind an availability check that
	// would silently drop the histograms, one behind nothing at all.
	//
	// Set after construction rather than injected, because Create() is the
	// first thing Initialize() does and neither service exists yet. That
	// ordering is on purpose: a crash during the rest of startup is still
	// caught, it simply reports without these two.
	//
	void SetReportingServices(
		std::shared_ptr<SEPerformanceHistoryTracker>
			performanceHistoryTracker,
		std::shared_ptr<SEAnalyticsEventsManager>
			analyticsEventsManager)
	{
		m_performanceHistoryTracker = performanceHistoryTracker;
		m_analyticsEventsManager = analyticsEventsManager;

		OnReportingServicesSet();
	}

public:
	// Stops the Windows hang-detection worker before OBS tears itself down.
	// Defaults to a no-op for backends that have none, which is what lets
	// SEGlobalStateManager call this without a platform guard.
	virtual void StopAsyncHangDetection() {}

public:
	// Constructs the backend selected at build time, or nullptr when crash
	// reporting is compiled out entirely.
	static SECrashHandler *Create(std::shared_ptr<SEConfig> config);

protected:
	//
	// Where the backend puts the two above: each one reaches a different
	// place. The Windows handlers forward the tracker into their
	// SECrashContext, and the macOS BugSplat handler keeps the analytics
	// manager where its file-static crash callbacks can find it.
	//
	virtual void OnReportingServicesSet() {}

	std::shared_ptr<SEPerformanceHistoryTracker>
		m_performanceHistoryTracker;
	std::shared_ptr<SEAnalyticsEventsManager> m_analyticsEventsManager;
	std::shared_ptr<SEConfig> m_config;
};
