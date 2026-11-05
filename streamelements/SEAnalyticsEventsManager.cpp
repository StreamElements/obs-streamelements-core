#include "SEAnalyticsEventsManager.hpp"
#include "SEConfig.hpp"
#include "SEUtils.hpp"

#include <obs.h>
#include <util/platform.h>
#include <string>

SEAnalyticsEventsManager::SEAnalyticsEventsManager(
	std::shared_ptr<SEConfig> config, size_t numWorkers)
	// m_startTime is const, so it is initialised here rather than
	// assigned below. It is declared before m_prevEventTime, so it is
	// already set by the time the second initialiser reads it.
	: m_startTime(os_gettime_ns()), m_prevEventTime(m_startTime)
{
	m_appId = config->GetHeapAnalyticsAppId();
	m_sessionId = CreateGloballyUniqueIdString();
	m_identity = GetComputerSystemUniqueId();

	m_taskConsumersKeepRunning = true;

	for (size_t i = 0; i < numWorkers; ++i) {
		m_taskConsumers.push_back(std::thread([this]() {
			task_queue_item_t task;

			while (m_taskConsumersKeepRunning || m_taskQueue.size_approx()) {
				if (m_taskQueue.wait_dequeue_timed(task, std::chrono::milliseconds(100))) {
					task();
				}
			}
		}));
	}
}

SEAnalyticsEventsManager::~SEAnalyticsEventsManager()
{
	m_taskConsumersKeepRunning = false;

	for (size_t i = 0; i < m_taskConsumers.size(); ++i) {
		if (m_taskConsumers[i].joinable()) {
			m_taskConsumers[i].join();
		}
	}

	m_taskConsumers.clear();
}

void SEAnalyticsEventsManager::Enqueue(task_queue_item_t task)
{
	m_taskQueue.enqueue(task);
}

#ifndef _WIN32
static const char* itoa(int input, char* buf, int radix)
{
	snprintf(buf, 32, "%d", input);
	
	return buf;
}
#endif

void SEAnalyticsEventsManager::AddRawEvent(
	const char *eventName, json11::Json::object propertiesJson,
	json11::Json::array fieldsJson,
	bool synchronous)
{
	if (!eventName) {
		return;
	}

	json11::Json::object props; //	= propertiesJson;

	//
	// One atomic exchange, not a read followed by a write.
	//
	// This function runs on whichever thread tracked the event: the Qt
	// thread for a dock click or the plug-in's own startup event, the
	// crash handler's thread for the synchronous one, the WYVRN SDK
	// thread for the init outcome. The old read-then-assign was a data
	// race between them -- unsynchronised access to a non-atomic
	// uint64_t, so undefined behaviour rather than merely a stale
	// number. exchange() makes "the previous event's time" a value that
	// exactly one event can claim, which an atomic load followed by an
	// atomic store still would not.
	//
	// Default (sequentially consistent) ordering, deliberately: on x86
	// an exchange is a locked instruction either way, so a weaker order
	// would buy nothing and cost the reader a question.
	//
	const uint64_t now = os_gettime_ns();
	const uint64_t prevEventTime = m_prevEventTime.exchange(now);

	const uint64_t secondsSinceStart =
		(now - m_startTime) / (uint64_t)1000000000L;

	//
	// Guarded, and the guard is not redundant: the exchange orders the
	// writes but not the os_gettime_ns() calls that precede them, so two
	// threads can read the clock in one order and reach the exchange in
	// the other. The loser then sees a previous time later than its own
	// now, and unsigned that underflows to ~584 years instead of the 0
	// those two events are genuinely apart.
	//
	const uint64_t secondsSincePrevEvent =
		now > prevEventTime
			? (now - prevEventTime) / (uint64_t)1000000000L
			: 0;

	char atoi_buf[32] = {0};

	json11::Json::array fields = json11::Json::array{
		json11::Json::array{"plugin_version",
				    GetSEPluginVersionString()},
		json11::Json::array{"obs_version", obs_get_version_string()},
		json11::Json::array{"seconds_since_start",
				    itoa(secondsSinceStart, atoi_buf, 10)},
		json11::Json::array{"seconds_since_previous_event",
				    itoa(secondsSincePrevEvent, atoi_buf, 10)}};

	props["feature"] = "obs_selive_core_plugin";
	props["name"] = eventName;
	props["source"] = "obs_plugin";
	props["placement"] = "obs";
	props["sessionId"] = m_sessionId;
	props["hostMachineId"] = m_identity;
	props["_meta"] = json11::Json::object{{"mobile", false},
					      {"platform", "desktop"},
#ifdef WIN32
					      {"os", "Windows"}
#elif defined(__APPLE__)
					      {"os", "macOS"}
#elif defined(__linux__)
					      {"os", "linux"}
#else
					      {"os", "unknown"}
#endif
	};

	for (auto kv : propertiesJson) {
		props[kv.first] = kv.second;
	}

	for (auto item : fieldsJson) {
		fields.push_back(item);
	}

	props["fields"] = fields;

	json11::Json json = props;

	std::string httpRequestBody = "[" + json.dump() + "]";

	if (!synchronous) {
		Enqueue([=]() {
			http_client_headers_t headers;

			headers.emplace(
				std::make_pair<std::string, std::string>(
					"Content-Type", "application/json"));

			HttpPost(
				"https://api.streamelements.com/science/insert/obslive",
				headers,
				(void*)httpRequestBody.c_str(),
				httpRequestBody.size(),
				nullptr,
				nullptr);
		});
	}
	else {
		http_client_headers_t headers;

		headers.emplace(
			std::make_pair<std::string, std::string>(
				"Content-Type", "application/json"));

		HttpPost(
			"https://api.streamelements.com/science/insert/obslive",
			headers,
			(void*)httpRequestBody.c_str(),
			httpRequestBody.size(),
			nullptr,
			nullptr);
	}
}
