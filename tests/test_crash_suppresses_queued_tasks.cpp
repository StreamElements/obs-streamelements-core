// CORE-862: queued Qt tasks must not run once crash reporting has begun.
//
// Background. The crash consent prompt is a native Win32 modal dialog, and
// every Win32 modal loop calls DispatchMessage on all messages -- including the
// private one Qt's event dispatcher posts to its own hidden window to drain the
// posted-event queue. So merely putting the prompt up runs whatever
// QtPostTask/QtExecSync/QtDelayTask work was already queued, inside the crash
// handler, on the crashing thread.
//
// When one of those tasks calls QDialog::exec(), its nested event loop sits on
// top of the prompt's loop and the crash path cannot continue until that
// unrelated dialog is answered. Observed with a cdb attach: the prompt had been
// answered and hidden while USER32!DialogBox2 was still on the stack, unable to
// return past the nested Qt loop above it.
//
// The fix gates the task at the single point every queued task passes through.
// The subtle half is that the gate must still run finish(): a QtExecSync caller
// on another thread is blocked in result.wait(), and dropping its task without
// releasing the promise would wedge that thread instead of the crashing one.
// That is the property most worth pinning down, and it is what this tests.
//
// Mirrors the executor lambda from __QtPostTask_Impl in StreamElementsUtils.cpp
// -- the real one needs qApp and a Qt event loop, neither of which belongs in a
// unit test. The control flow under test is reproduced exactly.

#include <cstdio>
#include <chrono>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

// Stand-in for the async-call context entry the real code pushes and pops.
struct ContextItem {
	std::string file;
	int line = 0;
	bool running = false;
	bool onStack = true;
};

static bool g_crashReportingInProgress = false;

static bool IsCrashReportingInProgress()
{
	return g_crashReportingInProgress;
}

// Mirror of the executor built by __QtPostTask_Impl, and of the QTimer lambda
// in __QtDelayTask_Impl, which have the same shape.
static void RunQueuedTask(const std::function<void()> &task, ContextItem *item,
			  std::promise<void> *promise)
{
	auto finish = [&]() {
		item->onStack = false;

		promise->set_value();
	};

	if (IsCrashReportingInProgress()) {
		finish();

		return;
	}

	item->running = true;

	try {
		task();
	} catch (...) {
		finish();

		throw;
	}

	finish();
}

// --- Normal operation: the task runs -------------------------------------
static void check_task_runs_when_not_crashing()
{
	g_crashReportingInProgress = false;

	ContextItem item;
	std::promise<void> promise;
	auto future = promise.get_future();

	bool ran = false;
	RunQueuedTask([&]() { ran = true; }, &item, &promise);

	check(ran,
	      "CORE-862: a queued task must still run when no crash is being reported");
	check(!item.onStack,
	      "CORE-862: the async-call context entry must be popped");
	check(future.wait_for(std::chrono::seconds(0)) ==
		      std::future_status::ready,
	      "CORE-862: the waiter must be released");
}

// --- On the crash path: the task is dropped ------------------------------
static void check_task_is_dropped_while_crashing()
{
	g_crashReportingInProgress = true;

	ContextItem item;
	std::promise<void> promise;
	auto future = promise.get_future();

	bool ran = false;
	RunQueuedTask([&]() { ran = true; }, &item, &promise);

	check(!ran,
	      "CORE-862: a task queued before the crash must NOT run inside the consent prompt");

	// This is the half that matters: dropping the task without releasing
	// the promise moves the deadlock from the crashing thread to whichever
	// thread called QtExecSync.
	check(future.wait_for(std::chrono::seconds(0)) ==
		      std::future_status::ready,
	      "CORE-862: a dropped task must still release its QtExecSync waiter");

	check(!item.onStack,
	      "CORE-862: a dropped task must still pop its async-call context entry, or it appears in every later crash report");

	check(!item.running,
	      "CORE-862: a dropped task must not be reported as having been running");
}

// --- The blocked-caller scenario, end to end -----------------------------
//
// The real deadlock shape: a background thread calls QtExecSync, which posts
// the task and blocks on the future. If the crash path drops the task without
// releasing the promise, that thread never wakes.
static void check_execsync_caller_is_released()
{
	g_crashReportingInProgress = true;

	ContextItem item;
	std::promise<void> promise;
	auto future = promise.get_future();

	// The "background thread" waiting for its marshalled task.
	std::thread waiter([&]() { future.wait(); });

	RunQueuedTask([&]() {}, &item, &promise);

	// If the gate forgot finish(), this join never returns.
	waiter.join();

	check(true,
	      "CORE-862: the QtExecSync caller must be released even though its task was dropped");
}

// --- A throwing task must still release --------------------------------
static void check_throwing_task_still_releases()
{
	g_crashReportingInProgress = false;

	ContextItem item;
	std::promise<void> promise;
	auto future = promise.get_future();

	bool threw = false;

	try {
		RunQueuedTask([]() { throw std::runtime_error("boom"); }, &item,
			      &promise);
	} catch (...) {
		threw = true;
	}

	check(threw, "CORE-862: a throwing task must still propagate");
	check(future.wait_for(std::chrono::seconds(0)) ==
		      std::future_status::ready,
	      "CORE-862: a throwing task must still release its waiter");
	check(!item.onStack,
	      "CORE-862: a throwing task must still pop its context entry");
}

// --- The gate is one-way -------------------------------------------------
//
// SetCrashReportingInProgress() is never cleared, deliberately: the process is
// dying. Once dropped, tasks stay dropped.
static void check_gate_is_one_way()
{
	g_crashReportingInProgress = true;

	for (int i = 0; i < 3; ++i) {
		ContextItem item;
		std::promise<void> promise;
		auto future = promise.get_future();

		bool ran = false;
		RunQueuedTask([&]() { ran = true; }, &item, &promise);

		check(!ran,
		      "CORE-862: every task after the crash must be dropped, not just the first");
		check(future.wait_for(std::chrono::seconds(0)) ==
			      std::future_status::ready,
		      "CORE-862: every dropped task must release its waiter");
	}
}

int main()
{
	check_task_runs_when_not_crashing();
	check_task_is_dropped_while_crashing();
	check_execsync_caller_is_released();
	check_throwing_task_still_releases();
	check_gate_is_one_way();

	if (failures) {
		std::fprintf(stderr, "%d crash-suppression check(s) failed\n",
			     failures);
		return 1;
	}

	std::puts("test_crash_suppresses_queued_tasks: all checks passed");
	return 0;
}
