#include "StreamElementsAnalyticsEventsManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsUtils.hpp"

#include <obs.h>
#include <util/platform.h>
#include <string>

StreamElementsAnalyticsEventsManager::StreamElementsAnalyticsEventsManager(size_t numWorkers)
{
	uint64_t now = os_gettime_ns();
	m_startTime = now;
	m_prevEventTime = now;
	m_appId = StreamElementsConfig::GetInstance()->GetHeapAnalyticsAppId();
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

StreamElementsAnalyticsEventsManager::~StreamElementsAnalyticsEventsManager()
{
	m_taskConsumersKeepRunning = false;

	for (size_t i = 0; i < m_taskConsumers.size(); ++i) {
		if (m_taskConsumers[i].joinable()) {
			m_taskConsumers[i].join();
		}
	}

	m_taskConsumers.clear();
}

void StreamElementsAnalyticsEventsManager::Enqueue(task_queue_item_t task)
{
	m_taskQueue.enqueue(task);
}

static const char* itoa(int input, char* buf, int radix)
{
	snprintf(buf, 32, "%d", input);
	
	return buf;
}

void StreamElementsAnalyticsEventsManager::AddRawEvent(
	const char *eventName, json11::Json::object propertiesJson,
	json11::Json::array fieldsJson,
	bool synchronous)
{
	if (!eventName) {
		return;
	}

	json11::Json::object props; //	= propertiesJson;

	uint64_t now = os_gettime_ns();
	uint64_t secondsSinceStart = (now - m_startTime) / (uint64_t)1000000000L;
	uint64_t secondsSincePrevEvent = (now - m_prevEventTime) / (uint64_t)1000000000L;
	m_prevEventTime = now;

	char atoi_buf[32];

	json11::Json::array fields = json11::Json::array{
		json11::Json::array{"plugin_version",
				    GetStreamElementsPluginVersionString()},
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
