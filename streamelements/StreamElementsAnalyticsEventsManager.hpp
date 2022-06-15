#pragma once

#include "StreamElementsUtils.hpp"
#include "deps/moodycamel/concurrentqueue.h"
#include "deps/moodycamel/blockingconcurrentqueue.h"
#include "json11/json11.hpp"

#include <vector>
#include <thread>
#include <string>

#include <QDockWidget>

class StreamElementsAnalyticsEventsManager
{
public:
	StreamElementsAnalyticsEventsManager(size_t numWorkers = 4);
	~StreamElementsAnalyticsEventsManager();

	void trackSynchronousEvent(
		const char *eventName,
		json11::Json::object props = json11::Json::object{},
		json11::Json::array fields = json11::Json::array{})
	{
		if (!eventName) {
			return;
		}

		AddRawEvent(eventName, props, fields, true);
	}

	void trackEvent(const char *eventName,
			json11::Json::object props = json11::Json::object{},
			json11::Json::array fields = json11::Json::array{})
	{
		if (!eventName) {
			return;
		}

		AddRawEvent(eventName, props, fields, false);
	}

	std::string identity() { return m_identity; }
	std::string sessionId() { return m_sessionId; }

protected:
	typedef std::function<void()> task_queue_item_t;

	void Enqueue(task_queue_item_t task);
	void AddRawEvent(
		const char *eventName,
		json11::Json::object propertiesJson = json11::Json::object{},
		json11::Json::array fieldsJson = json11::Json::array {}, bool synchronous = false);

private:
	uint64_t m_startTime;
	uint64_t m_prevEventTime;
	std::string m_appId;
	std::string m_sessionId;
	std::string m_identity;

	moodycamel::BlockingConcurrentQueue<task_queue_item_t> m_taskQueue;
	bool m_taskConsumersKeepRunning;
	std::vector<std::thread> m_taskConsumers;
};
