#pragma once

//
// Posting work to the Qt main thread, and the flag that says the main thread
// has stopped serving it.
//
// QtPostTask, QtExecSync and QtDelayTask are macros rather than functions so
// that they can capture __FILE__ and __LINE__ into the async-call context the
// crash handler reads. Nothing should call the __*_Impl functions directly.
//

#include <functional>
#include <future>
#include <mutex>
#include <string>

#define SYNC_ACCESS()                                                    \
	static std::recursive_mutex __sync_access_mutex;                 \
	std::lock_guard<std::recursive_mutex> __sync_access_mutex_guard( \
		__sync_access_mutex);

std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line);
std::future<void> __QtExecSync_Impl(std::function<void()> task,
				    std::string file, int line);

class QtAsyncCallFunctor {
private:
	typedef std::future<void> (*call_t)(std::function<void()> /*task*/,
					    std::string /*file*/, int /*line*/);

	std::string file;
	int line;
	call_t impl;

public:
	QtAsyncCallFunctor(const char *file_, const int line_,
			   const call_t impl_)
		: file(file_), line(line_), impl(impl_)
	{
	}

	std::future<void> operator()(std::function<void()> task) const
	{
		return impl(task, file, line);
	}
};

std::future<void> __QtDelayTask_Impl(std::function<void()> task, int delayMs,
				     const char *file, const int line);

#define QtDelayTask(task, delayMs) \
	__QtDelayTask_Impl(task, delayMs, __FILE__, __LINE__)
#define QtPostTask QtAsyncCallFunctor(__FILE__, __LINE__, &__QtPostTask_Impl)
#define QtExecSync QtAsyncCallFunctor(__FILE__, __LINE__, &__QtExecSync_Impl)

//
// True once teardown has begun and the Qt main thread has left its event loop
// for good.
//
// From that moment a cross-thread QtExecSync can never be served: the thread it
// is waiting for is inside our own shutdown and will not process another
// posted task. Anything still blocking on one deadlocks the shutdown it is
// blocking (CORE-1131).
//
bool IsShuttingDown();
void SetShuttingDown();
