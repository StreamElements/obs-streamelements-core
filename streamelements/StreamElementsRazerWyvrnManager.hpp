#pragma once

//
// Razer WYVRN integration: Chroma RGB lighting and Sensa HD haptics driven by
// naming an event, with the user's WYVRN configuration deciding what that event
// looks and feels like.
//
// Optional at runtime and Windows-only. The SDK needs Razer Synapse 4 and the
// Chroma App, which most OBS users do not have, so every failure path ends in
// "unavailable" and nothing worse. On macOS the whole subsystem compiles out and
// the accessors still answer.
//
// Two facts, both measured rather than assumed, shape the design:
//
//   * CoreInitSDK takes ~3.3 seconds, every run, with almost no variance. It is
//     therefore never on the startup path: init is posted to a worker and OBS
//     carries on immediately. WYVRN becomes available a few seconds into the
//     session.
//
//   * Every SDK call must happen on the thread that performed the init. The
//     Chroma stack beneath is COM-based and thread-affine. That single rule is
//     why there is one dedicated thread here rather than a pool or a set of
//     posted tasks, and it is also what makes shutdown correct by construction:
//     CoreUnInit runs as the last act of the same thread that called
//     CoreInitSDK, so Shutdown() never calls into the SDK at all.
//

#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "StreamElementsRazerWyvrnConfig.hpp"

#include "deps/cef-stub/cef_value.hpp"

/* ========================================================================= */

//
// Availability, as reported to page JavaScript.
//
// A closed vocabulary so a caller can branch without parsing prose. Mirrored
// verbatim into the `razerWyvrn` member of getHostCapabilities and into the
// hostRazerWyvrnStatusChanged event.
//
enum class StreamElementsRazerWyvrnStatus {
	// Compiled out: not a Windows build, or the CMake option is off.
	NotCompiledIn,

	// Windows build, option on, but the platform cannot support it.
	NotSupportedOnPlatform,

	// Init is running on the SDK thread.
	Initializing,

	// RzChromatic64.dll is absent - the ordinary outcome on a machine with
	// no Razer software.
	DllNotFound,

	// The DLL is present but is not signed by Razer.
	DllInvalidSignature,

	// The DLL loaded but CoreInitSDK refused.
	InitFailed,

	// Ready to accept events.
	Ok,

	// Tearing down; no new events accepted.
	ShuttingDown,
};

const char *
StreamElementsRazerWyvrnStatusToString(StreamElementsRazerWyvrnStatus status);

/* ========================================================================= */

class StreamElementsRazerWyvrnManager {
public:
	StreamElementsRazerWyvrnManager();
	~StreamElementsRazerWyvrnManager();

	//
	// Begin initialization. Returns immediately: the ~3.3 second
	// CoreInitSDK runs on the SDK thread, and the manager reaches Ok or a
	// failure status later. Safe to call when the integration is compiled
	// out, in which case it does nothing.
	//
	void Start();

	//
	// Stop accepting events, join the SDK thread, and let it unwind through
	// CoreUnInit on its way out. Idempotent, because the destructor calls it
	// too.
	//
	void Shutdown();

	StreamElementsRazerWyvrnStatus GetStatus() const;

	// Non-zero only for a status that came from an RZRESULT.
	long GetLastResult() const;

	//
	// Fire an event by name. Returns false when the manager is not ready, or
	// when the name was superseded by a newer one inside the rate-limit
	// window.
	//
	// An empty name stops playback, which is the SDK's own convention.
	//
	// Does not block: the name is handed to the SDK thread and this returns.
	//
	bool SetEventName(const std::string &name);

	//
	// Every event declared by every WYVRN configuration on this machine.
	//
	// Cached after the first scan - 5.3 MB across 146 files is not something
	// to re-read per API call. `refresh` forces a re-scan.
	//
	// Returns a copy rather than a reference: a reference would be read
	// after the lock was released, and a concurrent refresh would then be
	// rewriting the vector underneath the reader.
	//
	std::vector<StreamElementsRazerWyvrnEventInfo>
	GetEvents(bool refresh = false);

	size_t GetEventCount();

	//
	// The `razerWyvrn` object shared by getHostCapabilities and the
	// hostRazerWyvrnStatusChanged event. One serializer, so the two cannot
	// disagree about what "status" means.
	//
	CefRefPtr<CefValue> SerializeStatus();

	//
	// Serialize one event, including its components and session-signed asset
	// URLs, so a caller can see and preview what firing it would do.
	//
	CefRefPtr<CefValue>
	SerializeEvent(const StreamElementsRazerWyvrnEventInfo &event);

	//
	// The whole list, optionally filtered. `sourceFilter` matches the
	// containing folder and `idPrefix` the event id; both are
	// case-insensitive and either may be empty.
	//
	CefRefPtr<CefValue> SerializeEvents(const std::string &sourceFilter,
					    const std::string &idPrefix);

	// Roots searched for wyvrn.config files, in order.
	static std::vector<std::string> GetConfigRoots();

private:
	void ThreadProc();
	void SetStatus(StreamElementsRazerWyvrnStatus status, long result);
	void DispatchStatusChanged();

	// Caller must hold m_mutex.
	void ScanEventsLocked();

	//
	// A consistent copy of the scan, taken under the lock.
	//
	// Serializing ~4,000 events means thousands of filesystem probes and URL
	// signings. Holding the mutex across that would block every other caller
	// for the duration and, worse, would deadlock the moment a serializer
	// reached back into a public accessor that locks. Copy, release, then
	// work.
	//
	struct Snapshot {
		std::vector<StreamElementsRazerWyvrnEventInfo> events;
		std::vector<std::pair<std::string, std::string>> sourcePaths;
	};

	Snapshot TakeSnapshot(bool refresh);

	// Absolute paths of the assets a component refers to, discovered by
	// looking beside the config rather than trusting a naming convention.
	// Lock-free: everything they need comes from the snapshot.
	static std::vector<std::pair<std::string, std::string>>
	FindChromaAssets(const Snapshot &snapshot, const std::string &source,
			 const std::string &effect);
	static std::string FindHapticAsset(const Snapshot &snapshot,
					   const std::string &source,
					   const std::string &effect);

	static CefRefPtr<CefValue>
	SerializeEventInternal(const Snapshot &snapshot,
			       const StreamElementsRazerWyvrnEventInfo &event);

	mutable std::mutex m_mutex;
	std::condition_variable m_wake;

	std::thread m_thread;
	bool m_threadRunning = false;
	bool m_stopRequested = false;

	StreamElementsRazerWyvrnStatus m_status =
		StreamElementsRazerWyvrnStatus::NotCompiledIn;
	long m_lastResult = 0;

	//
	// Single-slot mailbox: the newest name wins.
	//
	// SetEventName is capped at 30 FPS. Rather than drop anything arriving
	// inside the window, the newest name is parked here and sent when the
	// window expires - and if another arrives first, it replaces the parked
	// one. Older events are discarded, never the newest, because the last
	// thing a stream did is the thing worth rendering. One slot bounds the
	// backlog at one by construction.
	//
	bool m_pendingValid = false;
	std::string m_pendingName;

	std::vector<StreamElementsRazerWyvrnEventInfo> m_events;
	bool m_eventsScanned = false;

	// Cache of source folder -> absolute path, filled during the scan so
	// asset lookup does not re-walk the tree per component.
	std::vector<std::pair<std::string, std::string>> m_sourcePaths;
};
