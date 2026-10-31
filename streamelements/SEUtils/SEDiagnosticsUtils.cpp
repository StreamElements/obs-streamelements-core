#include "SEDiagnosticsUtils.hpp"

#include <atomic>
#include <mutex>
#include <shared_mutex>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/syscall.h>
#include <unistd.h>
#endif

// Written from a signal handler / exception filter, so std::atomic rather than
// a plain bool. One-way: nothing clears it.
static std::atomic<bool> s_crashReportingInProgress(false);

void SetCrashReportingInProgress()
{
	s_crashReportingInProgress.store(true);
}

bool IsCrashReportingInProgress()
{
	return s_crashReportingInProgress.load();
}

static SEApiContext_t s_apiContext;
static std::shared_mutex s_apiContextMutex;

void GetApiContext(std::function<void(SEApiContext_t *)> callback)
{
	std::shared_lock lock(s_apiContextMutex);

	callback(&s_apiContext);
}

// Non-blocking variant, for the crash path. Same reasoning as
// TryGetAsyncCallContextStack.
bool TryGetApiContext(std::function<void(SEApiContext_t *)> callback)
{
	std::shared_lock lock(s_apiContextMutex, std::try_to_lock);

	if (!lock.owns_lock())
		return false;

	callback(&s_apiContext);

	return true;
}

std::shared_ptr<SEApiContextItem> PushApiContext(CefString method,
						 CefRefPtr<CefListValue> args)
{
	std::unique_lock lock(s_apiContextMutex);

	auto item = std::make_shared<SEApiContextItem>(method, args,
#ifdef _WIN32
						       GetCurrentThreadId()
#else
						       uint32_t(syscall(
							       SYS_thread_selfid))
#endif
	);

	s_apiContext.push_back(item);

	return item;
}

void RemoveApiContext(std::shared_ptr<SEApiContextItem> item)
{
	std::unique_lock lock(s_apiContextMutex);

	s_apiContext.remove(item);
}

static SEAsyncCallContextStack_t s_asyncCallContextStack;
static std::shared_mutex s_asyncCallContextStackMutex;

void GetAsyncCallContextStack(
	std::function<void(const SEAsyncCallContextStack_t *)> callback)
{
	std::shared_lock lock(s_asyncCallContextStackMutex);

	callback(&s_asyncCallContextStack);
}

//
// As above, but never blocks. Returns false, without invoking the callback, if
// the lock is held.
//
// For the crash path only. This lock is taken exclusively by
// AsyncCallContextPush and AsyncCallContextRemove, which run on every
// QtPostTask, QtExecSync and QtDelayTask -- hundreds of call sites, constantly.
// A crashing thread that blocks on it waits on whichever thread holds it, and
// that thread may itself be stuck or already gone; worse, std::shared_mutex is
// not recursive, so a thread that faulted while holding it exclusively would
// deadlock against itself.
//
// Losing this section of a crash report is a far better outcome than a handler
// that never returns, which produces no report at all.
//
bool TryGetAsyncCallContextStack(
	std::function<void(const SEAsyncCallContextStack_t *)> callback)
{
	std::shared_lock lock(s_asyncCallContextStackMutex, std::try_to_lock);

	if (!lock.owns_lock())
		return false;

	callback(&s_asyncCallContextStack);

	return true;
}

std::shared_ptr<SEAsyncCallContextItem>
AsyncCallContextPush(std::string file, int line, bool running)
{
	std::unique_lock lock(s_asyncCallContextStackMutex);

	auto item =
		std::make_shared<SEAsyncCallContextItem>(file, line, running);

	s_asyncCallContextStack.push_back(item);

	return item;
}

void AsyncCallContextRemove(std::shared_ptr<SEAsyncCallContextItem> item)
{
	std::unique_lock lock(s_asyncCallContextStackMutex);

	s_asyncCallContextStack.remove(item);
}
