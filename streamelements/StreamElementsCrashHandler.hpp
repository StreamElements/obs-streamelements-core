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
class StreamElementsCrashHandler {
public:
	virtual ~StreamElementsCrashHandler() {}

public:
	// Stops the Windows hang-detection worker before OBS tears itself down.
	// Defaults to a no-op for backends that have none, which is what lets
	// StreamElementsGlobalStateManager call this without a platform guard.
	virtual void StopAsyncHangDetection() {}

public:
	// Constructs the backend selected at build time, or nullptr when crash
	// reporting is compiled out entirely.
	static StreamElementsCrashHandler *Create();
};
