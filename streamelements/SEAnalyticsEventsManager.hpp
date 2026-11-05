#pragma once

#include "ISEAnalyticsEventsManager.hpp"

#include "SEUtils.hpp"
#include "deps/moodycamel/concurrentqueue.h"
#include "deps/moodycamel/blockingconcurrentqueue.h"
#include "json11/json11.hpp"

#include <atomic>
#include <vector>
#include <thread>
#include <string>

#include <QDockWidget>

class SEConfig;

class SEAnalyticsEventsManager : public ISEAnalyticsEventsManager {
public:
	//
	// The config is read once, here, for the analytics app id. Taken
	// rather than fetched: this constructor runs early in Initialize(),
	// which is exactly when a lazily built singleton would have been
	// created as a side effect of reading it.
	//
	SEAnalyticsEventsManager(std::shared_ptr<SEConfig> config,
				 size_t numWorkers = 4);
	~SEAnalyticsEventsManager();

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

	void
	trackEvent(const char *eventName,
		   json11::Json::object props = json11::Json::object{},
		   json11::Json::array fields = json11::Json::array{}) override
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
	//
	// Written once, in the constructor's initialiser list, and only read
	// afterwards -- which is what makes it safe to read from every thread
	// that tracks an event. const so that stays true by compiler rather
	// than by comment: of the three members below it is the one that only
	// looked shared.
	//
	const uint64_t m_startTime;

	//
	// Genuinely shared mutable state: read-modify-written by whichever
	// thread tracked the event -- the Qt thread, the WYVRN SDK thread, the
	// crash handler's thread. Updated with a single exchange, not a load
	// and a store; see AddRawEvent for why that distinction survives
	// making it atomic.
	//
	std::atomic<uint64_t> m_prevEventTime;

	std::string m_appId;
	std::string m_sessionId;
	std::string m_identity;

	moodycamel::BlockingConcurrentQueue<task_queue_item_t> m_taskQueue;

	//
	// The worker threads' stop flag, so the destructor writes it while
	// four of them are reading it in their loop condition. A plain bool
	// there is the same data race as above, and worse in one way: nothing
	// stops the compiler hoisting the load out of the loop, which turns
	// "stop" into "never stop".
	//
	std::atomic<bool> m_taskConsumersKeepRunning;

	std::vector<std::thread> m_taskConsumers;
};
