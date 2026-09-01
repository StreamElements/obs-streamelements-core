#include "StreamElementsRazerWyvrnManager.hpp"
#include "StreamElementsUtils.hpp"
#include "Version.hpp"

#include <obs.h>
#include <util/platform.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>

#ifdef SE_ENABLE_WYVRN
#include "deps/wyvrn-sdk/WyvrnAPI.h"
#include "deps/wyvrn-sdk/WyvrnErrors.h"
#endif

/* ========================================================================= */

//
// How long startup waits for init before giving up and letting it finish in the
// background.
//
// Zero, deliberately. CoreInitSDK was measured at a flat ~3.3 seconds across
// four runs on a machine with Razer running, so no wait short enough to be
// acceptable at startup could ever be satisfied - it would add its full duration
// to every OBS start and still hand off to the background. Kept as a named
// constant rather than deleted so it can be raised if a future SDK gets fast
// enough for waiting to mean anything.
//
static const int kInitGraceMs = 0;

// SetEventName is rate-limited to 30 FPS by the SDK.
static const int kMinEventIntervalMs = 34;

/* ========================================================================= */

const char *
StreamElementsRazerWyvrnStatusToString(StreamElementsRazerWyvrnStatus status)
{
	switch (status) {
	case StreamElementsRazerWyvrnStatus::NotCompiledIn:
		return "notCompiledIn";
	case StreamElementsRazerWyvrnStatus::NotSupportedOnPlatform:
		return "notSupportedOnPlatform";
	case StreamElementsRazerWyvrnStatus::Initializing:
		return "initializing";
	case StreamElementsRazerWyvrnStatus::DllNotFound:
		return "dllNotFound";
	case StreamElementsRazerWyvrnStatus::DllInvalidSignature:
		return "dllInvalidSignature";
	case StreamElementsRazerWyvrnStatus::InitFailed:
		return "initFailed";
	case StreamElementsRazerWyvrnStatus::Ok:
		return "ok";
	case StreamElementsRazerWyvrnStatus::ShuttingDown:
		return "shuttingDown";
	}

	return "initFailed";
}

namespace {

std::string ToLower(const std::string &s)
{
	std::string out;
	out.reserve(s.size());
	for (unsigned char c : s)
		out.push_back((char)std::tolower(c));
	return out;
}

bool StartsWithNoCase(const std::string &haystack, const std::string &prefix)
{
	if (prefix.empty())
		return true;
	if (prefix.size() > haystack.size())
		return false;
	return ToLower(haystack).compare(0, prefix.size(), ToLower(prefix)) ==
	       0;
}

std::string ReadWholeFile(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in)
		return std::string();

	std::ostringstream buf;
	buf << in.rdbuf();
	return buf.str();
}

} // namespace

/* ========================================================================= */

StreamElementsRazerWyvrnManager::StreamElementsRazerWyvrnManager()
{
#ifdef SE_ENABLE_WYVRN
	m_status = StreamElementsRazerWyvrnStatus::Initializing;
#else
	m_status = StreamElementsRazerWyvrnStatus::NotCompiledIn;
#endif
}

StreamElementsRazerWyvrnManager::~StreamElementsRazerWyvrnManager()
{
	Shutdown();
}

std::vector<std::string> StreamElementsRazerWyvrnManager::GetConfigRoots()
{
	std::vector<std::string> roots;

	// An override first, so a wrong guess below is a configuration problem
	// rather than a code change. The documented path and the path actually
	// used by the shipping installer differ in casing, which is a hint that
	// neither is guaranteed stable.
	const char *override_ = getenv("SE_WYVRN_HAPTIC_FOLDERS");
	if (override_ && *override_)
		roots.push_back(override_);

#ifdef WIN32
	// Observed on a machine with Synapse 4 installed. Case is irrelevant on
	// Windows; both spellings are listed because the documentation and the
	// installer disagree and either may be what a given build produces.
	roots.push_back("C:\\Program Files (x86)\\Interhaptics\\hapticFolders");
	roots.push_back("C:\\Program Files (x86)\\InterHaptics\\HapticFolders");
#endif

	return roots;
}

void StreamElementsRazerWyvrnManager::Start()
{
#ifndef SE_ENABLE_WYVRN
	// Nothing to start. The status set in the constructor already says so,
	// and every accessor answers normally.
	return;
#else
	std::unique_lock<std::mutex> lock(m_mutex);

	if (m_threadRunning)
		return;

	m_threadRunning = true;
	m_stopRequested = false;

	m_thread = std::thread([this]() { ThreadProc(); });

	// Startup does not wait: CoreInitSDK is a flat ~3.3 seconds, so any
	// grace period short enough to be acceptable here would always expire.
	// The constant is honoured if someone raises it.
	if (kInitGraceMs > 0) {
		m_wake.wait_for(lock, std::chrono::milliseconds(kInitGraceMs),
				[this]() {
					return m_status !=
					       StreamElementsRazerWyvrnStatus::
						       Initializing;
				});
	}
#endif
}

void StreamElementsRazerWyvrnManager::Shutdown()
{
	std::thread thread;

	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!m_threadRunning)
			return;

		m_stopRequested = true;

		// Discard anything parked: the process is going away, and
		// sending one more light cue on the way out helps nobody.
		m_pendingValid = false;
		m_pendingName.clear();

		thread = std::move(m_thread);
		m_threadRunning = false;
	}

	m_wake.notify_all();

	// Join, not detach. The thread unwinds through CoreUnInit and
	// UninitAPI, and UninitAPI frees RzChromatic64.dll - letting that race
	// with our own teardown means unloading a module while it is still
	// executing, which is a crash inside somebody else's DLL.
	if (thread.joinable())
		thread.join();
}

StreamElementsRazerWyvrnStatus
StreamElementsRazerWyvrnManager::GetStatus() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_status;
}

long StreamElementsRazerWyvrnManager::GetLastResult() const
{
	std::lock_guard<std::mutex> lock(m_mutex);
	return m_lastResult;
}

void StreamElementsRazerWyvrnManager::SetStatus(
	StreamElementsRazerWyvrnStatus status, long result)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (m_status == status && m_lastResult == result)
			return;

		m_status = status;
		m_lastResult = result;
	}

	m_wake.notify_all();

	DispatchStatusChanged();
}

void StreamElementsRazerWyvrnManager::DispatchStatusChanged()
{
	// Init finishes seconds after the page has loaded, so without this the
	// only way to learn the outcome is to poll getHostCapabilities. Fired on
	// failure as well as success: a page waiting for "ready" would otherwise
	// wait forever on the overwhelming majority of machines, which have no
	// Razer software at all.
	//
	// DispatchJSEventGlobal already guards on IsInstanceAvailable() and on a
	// null API server, which matters because ShuttingDown is reached exactly
	// when the singleton may be going away.
	auto value = SerializeStatus();

	DispatchJSEventGlobal(
		"hostRazerWyvrnStatusChanged",
		CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString());
}

/* ========================================================================= */

void StreamElementsRazerWyvrnManager::ThreadProc()
{
#ifdef SE_ENABLE_WYVRN
	using clock = std::chrono::steady_clock;

	const auto initStart = clock::now();

	// --- init, on this thread and no other ------------------------------
	const int initApi = WyvrnSDK::WyvrnAPI::InitAPI();

	// RZRESULT_INVALID, not RZRESULT_FAILED: nothing has produced an
	// RZRESULT yet on this path, and the SDK's own RZRESULT_FAILED is
	// unusable as a value -- it is defined as 2147500037L (0x80004005)
	// while RZRESULT is a signed LONG, so assigning it truncates. -1L is
	// the SDK's "invalid" and fits.
	RZRESULT initResult = RZRESULT_INVALID;
	bool sdkInitialized = false;

	if (initApi == 0) {
		WyvrnSDK::APPINFOTYPE info{};

		// APPINFOTYPE carries a const member with an initialiser, so it
		// cannot be memset or assigned wholesale - value-initialise and
		// fill the fields.
		wcscpy_s(info.Title, L"SE.Live");
		wcscpy_s(info.Description,
			 L"StreamElements SE.Live plug-in for OBS Studio");
		wcscpy_s(info.Author.Name, L"StreamElements");
		wcscpy_s(info.Author.Contact, L"https://streamelements.com");

		// 0x01 is App, 0x02 is Game. This is a broadcasting tool, and
		// the value decides how Synapse and the Chroma App list us.
		info.Category = 0x01;

		initResult = WyvrnSDK::WyvrnAPI::CoreInitSDK(&info);
		sdkInitialized = (initResult == RZRESULT_SUCCESS);
	}

	const double initMs = std::chrono::duration<double, std::milli>(
				      clock::now() - initStart)
				      .count();

	if (sdkInitialized) {
		// The SDK asks for ~100 ms of settling before the first
		// SetEventName.
		os_sleep_ms(120);

		SetStatus(StreamElementsRazerWyvrnStatus::Ok, initResult);

		blog(LOG_INFO,
		     "obs-streamelements-core: WYVRN: initialized in %.0f ms",
		     initMs);
	} else {
		StreamElementsRazerWyvrnStatus status =
			StreamElementsRazerWyvrnStatus::InitFailed;

		if (initApi != 0)
			status = StreamElementsRazerWyvrnStatus::DllNotFound;
		else if (initResult == RZRESULT_DLL_NOT_FOUND)
			status = StreamElementsRazerWyvrnStatus::DllNotFound;
		else if (initResult == RZRESULT_DLL_INVALID_SIGNATURE)
			status = StreamElementsRazerWyvrnStatus::
				DllInvalidSignature;

		SetStatus(status, (long)initResult);

		// One line, at INFO rather than WARNING: on the overwhelming
		// majority of machines this is the expected outcome, not a
		// fault. It is also the first thing anyone diagnosing "why is
		// there no lighting" will look for.
		blog(LOG_INFO,
		     "obs-streamelements-core: WYVRN: unavailable (%s, InitAPI=%d, RZRESULT=0x%08lX) after %.0f ms",
		     StreamElementsRazerWyvrnStatusToString(status), initApi,
		     (unsigned long)initResult, initMs);
	}

	// --- service the mailbox --------------------------------------------
	auto lastSent =
		clock::now() - std::chrono::milliseconds(kMinEventIntervalMs);

	for (;;) {
		std::string toSend;
		bool haveWork = false;

		{
			std::unique_lock<std::mutex> lock(m_mutex);

			m_wake.wait_for(
				lock,
				std::chrono::milliseconds(kMinEventIntervalMs),
				[this]() {
					return m_stopRequested ||
					       m_pendingValid;
				});

			if (m_stopRequested)
				break;

			if (m_pendingValid && sdkInitialized) {
				const auto sinceLast =
					std::chrono::duration_cast<
						std::chrono::milliseconds>(
						clock::now() - lastSent)
						.count();

				if (sinceLast >= kMinEventIntervalMs) {
					toSend = m_pendingName;
					haveWork = true;
					m_pendingValid = false;
				}
				// Otherwise leave it parked. The timed wait
				// above brings us back when the window is up,
				// and a newer name may have replaced it by then
				// - which is the intent.
			}
		}

		if (!haveWork)
			continue;

		const std::wstring wide = utf8_to_wstring(toSend);
		const RZRESULT result =
			WyvrnSDK::WyvrnAPI::CoreSetEventName(wide.c_str());

		lastSent = clock::now();

		blog(LOG_INFO,
		     "obs-streamelements-core: WYVRN: sent event '%s' (RZRESULT=0x%08lX)",
		     toSend.c_str(), (unsigned long)result);
	}

	// --- teardown, on the same thread that initialised -------------------
	if (sdkInitialized) {
		const RZRESULT uninit = WyvrnSDK::WyvrnAPI::CoreUnInit();

		blog(LOG_INFO,
		     "obs-streamelements-core: WYVRN: CoreUnInit (RZRESULT=0x%08lX)",
		     (unsigned long)uninit);
	}

	if (initApi == 0)
		WyvrnSDK::WyvrnAPI::UninitAPI();
#endif
}

/* ========================================================================= */

bool StreamElementsRazerWyvrnManager::SetEventName(const std::string &name)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (m_status != StreamElementsRazerWyvrnStatus::Ok)
		return false;

	if (m_stopRequested)
		return false;

	// Newest wins. If something is already parked it is being replaced
	// before it ever reached the SDK, which is worth seeing in the log -
	// otherwise a name that never rendered simply vanishes.
	if (m_pendingValid && m_pendingName != name) {
		blog(LOG_INFO,
		     "obs-streamelements-core: WYVRN: event '%s' superseded by '%s' inside the rate limit",
		     m_pendingName.c_str(), name.c_str());
	}

	m_pendingName = name;
	m_pendingValid = true;

	m_wake.notify_all();

	return true;
}

/* ========================================================================= */

void StreamElementsRazerWyvrnManager::ScanEventsLocked()
{
	m_events.clear();
	m_sourcePaths.clear();
	m_eventsScanned = true;

	for (const auto &root : GetConfigRoots()) {
		os_dir_t *dir = os_opendir(root.c_str());

		if (!dir)
			continue;

		struct os_dirent *entry;

		while ((entry = os_readdir(dir)) != NULL) {
			if (!entry->directory)
				continue;

			if (entry->d_name[0] == '.')
				continue;

			const std::string source = entry->d_name;
			const std::string folder = root + "/" + source;

			os_dir_t *sub = os_opendir(folder.c_str());

			if (!sub)
				continue;

			struct os_dirent *file;

			while ((file = os_readdir(sub)) != NULL) {
				if (file->directory)
					continue;

				if (!IsRazerWyvrnConfigFileName(file->d_name))
					continue;

				const std::string path =
					folder + "/" + file->d_name;

				auto events = ParseRazerWyvrnConfig(
					ReadWholeFile(path), source);

				// An empty result is the normal outcome for
				// most files - 124 of 146 on a stock machine
				// are audio profiles with no events at all -
				// so it is not logged.
				for (auto &e : events)
					m_events.push_back(std::move(e));
			}

			// Closed on every path, including the early
			// continues above, which is why this sits here and
			// not at the end of a conditional.
			os_closedir(sub);

			m_sourcePaths.push_back({source, folder});
		}

		os_closedir(dir);
	}

	blog(LOG_INFO,
	     "obs-streamelements-core: WYVRN: scanned %zu event(s) from %zu folder(s)",
	     m_events.size(), m_sourcePaths.size());
}

std::vector<StreamElementsRazerWyvrnEventInfo>
StreamElementsRazerWyvrnManager::GetEvents(bool refresh)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (refresh || !m_eventsScanned)
		ScanEventsLocked();

	return m_events;
}

StreamElementsRazerWyvrnManager::Snapshot
StreamElementsRazerWyvrnManager::TakeSnapshot(bool refresh)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (refresh || !m_eventsScanned)
		ScanEventsLocked();

	Snapshot snapshot;
	snapshot.events = m_events;
	snapshot.sourcePaths = m_sourcePaths;

	return snapshot;
}

size_t StreamElementsRazerWyvrnManager::GetEventCount()
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (!m_eventsScanned)
		ScanEventsLocked();

	return m_events.size();
}

/* ========================================================================= */

std::vector<std::pair<std::string, std::string>>
StreamElementsRazerWyvrnManager::FindChromaAssets(const Snapshot &snapshot,
						  const std::string &source,
						  const std::string &effect)
{
	std::vector<std::pair<std::string, std::string>> assets;

	std::string folder;
	for (const auto &kv : snapshot.sourcePaths) {
		if (kv.first == source) {
			folder = kv.second;
			break;
		}
	}

	if (folder.empty())
		return assets;

	// Device variants are discovered by looking at what is actually beside
	// the config, rather than by assuming a fixed set of suffixes: not every
	// effect ships every device, and the caller wants the ones that exist.
	static const char *kDevices[] = {"Keyboard", "Keypad",  "Mouse",
					 "Mousepad", "Headset", "ChromaLink"};

	for (const char *device : kDevices) {
		const std::string path =
			folder + "/" + effect + "_" + device + ".chroma";

		if (os_file_exists(path.c_str()))
			assets.push_back({device, path});
	}

	return assets;
}

std::string
StreamElementsRazerWyvrnManager::FindHapticAsset(const Snapshot &snapshot,
						 const std::string &source,
						 const std::string &effect)
{
	for (const auto &kv : snapshot.sourcePaths) {
		if (kv.first != source)
			continue;

		const std::string path = kv.second + "/" + effect + ".haps";

		if (os_file_exists(path.c_str()))
			return path;

		break;
	}

	return std::string();
}

/* ========================================================================= */

CefRefPtr<CefValue> StreamElementsRazerWyvrnManager::SerializeStatus()
{
	CefRefPtr<CefValue> result = CefValue::Create();
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	const auto status = GetStatus();

	d->SetBool("available", status == StreamElementsRazerWyvrnStatus::Ok);
	d->SetBool("initialized", status == StreamElementsRazerWyvrnStatus::Ok);
	d->SetString("status", StreamElementsRazerWyvrnStatusToString(status));
	d->SetInt("eventCount", (int)GetEventCount());

	result->SetDictionary(d);

	return result;
}

CefRefPtr<CefValue> StreamElementsRazerWyvrnManager::SerializeEventInternal(
	const Snapshot &snapshot,
	const StreamElementsRazerWyvrnEventInfo &event)
{
	CefRefPtr<CefValue> result = CefValue::Create();
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetString("id", event.id);
	d->SetString("source", event.source);
	d->SetString("kind", event.kind);

	CefRefPtr<CefListValue> chroma = CefListValue::Create();

	for (const auto &component : event.chroma) {
		CefRefPtr<CefDictionaryValue> c = CefDictionaryValue::Create();

		c->SetString("effect", component.effect);
		c->SetBool("interrupt", component.interrupt);

		CefRefPtr<CefListValue> assets = CefListValue::Create();

		for (const auto &asset : FindChromaAssets(
			     snapshot, event.source, component.effect)) {
			CefRefPtr<CefDictionaryValue> a =
				CefDictionaryValue::Create();

			a->SetString("device", asset.first);

			// A session-signed URL, so the page fetches the bytes
			// through the local file server rather than being handed
			// a raw filesystem path it could not use anyway.
			a->SetString("url",
				     CreateSessionSignedAbsolutePathURL(
					     utf8_to_wstring(asset.second)));

			assets->SetDictionary(assets->GetSize(), a);
		}

		c->SetList("assets", assets);
		chroma->SetDictionary(chroma->GetSize(), c);
	}

	d->SetList("chroma", chroma);

	CefRefPtr<CefListValue> haptics = CefListValue::Create();

	for (const auto &component : event.haptics) {
		CefRefPtr<CefDictionaryValue> h = CefDictionaryValue::Create();

		h->SetString("effect", component.effect);
		h->SetInt("loop", component.loop);
		h->SetString("mixing", component.mixing);
		h->SetString("priority", component.priority);

		const std::string path = FindHapticAsset(snapshot, event.source,
							 component.effect);

		if (!path.empty()) {
			h->SetString("url", CreateSessionSignedAbsolutePathURL(
						    utf8_to_wstring(path)));
		}

		CefRefPtr<CefListValue> targeting = CefListValue::Create();

		// Where on the body the effect lands. Passed through as written -
		// the shipped data contains a lowercase "waist" and the
		// misspelling "Wasit", and a caller mapping these to body regions
		// should see what the data says rather than a cleaned-up version
		// of it.
		for (const auto &target : component.targeting) {
			CefRefPtr<CefDictionaryValue> t =
				CefDictionaryValue::Create();

			t->SetString("target", target.target);
			t->SetString("spatialization", target.spatialization);
			t->SetDouble("gain", target.gain);

			targeting->SetDictionary(targeting->GetSize(), t);
		}

		h->SetList("targeting", targeting);
		haptics->SetDictionary(haptics->GetSize(), h);
	}

	d->SetList("haptics", haptics);

	result->SetDictionary(d);

	return result;
}

CefRefPtr<CefValue> StreamElementsRazerWyvrnManager::SerializeEvent(
	const StreamElementsRazerWyvrnEventInfo &event)
{
	return SerializeEventInternal(TakeSnapshot(false), event);
}

CefRefPtr<CefValue> StreamElementsRazerWyvrnManager::SerializeEvents(
	const std::string &sourceFilter, const std::string &idPrefix)
{
	// One snapshot for the whole list. Everything below is lock-free, which
	// matters because serializing ~4,000 events means thousands of
	// os_file_exists probes and URL signings; holding the mutex across that
	// would stall every other caller for the duration.
	const Snapshot snapshot = TakeSnapshot(false);

	CefRefPtr<CefValue> result = CefValue::Create();
	CefRefPtr<CefListValue> list = CefListValue::Create();

	for (const auto &event : snapshot.events) {
		if (!sourceFilter.empty() &&
		    ToLower(event.source) != ToLower(sourceFilter))
			continue;

		if (!StartsWithNoCase(event.id, idPrefix))
			continue;

		list->SetValue(list->GetSize(),
			       SerializeEventInternal(snapshot, event));
	}

	result->SetList(list);

	return result;
}
