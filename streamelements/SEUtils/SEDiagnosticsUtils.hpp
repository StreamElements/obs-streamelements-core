#pragma once

//
// Crash and async-call diagnostics: the latch that says a crash report is
// being written, the stack of in-flight host API calls, the stack of posted Qt
// tasks with the file and line that posted them, and the name tag for whatever
// widget is being torn down.
//
// All of it is read by the crash handler while the process is dying, which is
// why the getters have non-blocking variants and why the teardown scope writes
// into a fixed buffer without allocating.
//
// Depends on nothing but the standard library and the local CefValue model, so
// this header can be included on its own -- SEUtils.hpp, which includes it,
// cannot be.
//

#include <cef-headers.hpp>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <list>
#include <memory>
#include <string>

//
// Set by the crash handler for as long as it is reporting a crash, and never
// cleared -- a process that has faulted is not going back to normal service.
//
// The consent prompt is modal, and a modal dialog on either platform runs a
// nested message loop that keeps draining this process's own event queue. So
// work queued before the fault -- including host API calls -- executes inside
// the crash handler, on the thread that just crashed. Observed on macOS: a
// second `crashProgram` call was delivered into the NSAlert's modal loop,
// faulted again, and because the SDK's signal handler ignores a re-entrant
// crash the faulting instruction simply re-executed forever. OBS hung at 100%
// CPU with the dialog frozen behind a spinning beachball.
//
// Anything that can run application code from an event loop should consult
// this and decline.
//
void SetCrashReportingInProgress();
bool IsCrashReportingInProgress();

class SEApiContextItem {
public:
	SEApiContextItem(CefString method_, CefRefPtr<CefListValue> args_,
			 uint32_t thread_)
		: method(method_), args(args_), thread(thread_)
	{
	}

public:
	CefString method;
	CefRefPtr<CefListValue> args;
	uint32_t thread;
};

class SEApiContext_t : public std::list<std::shared_ptr<SEApiContextItem>> {
public:
	SEApiContext_t() {}
	~SEApiContext_t() { clear(); }
};

void GetApiContext(std::function<void(SEApiContext_t *)> callback);

// Non-blocking variant for the crash path; false means the lock was held
// and the callback did not run. See the definition for why.
bool TryGetApiContext(std::function<void(SEApiContext_t *)> callback);
std::shared_ptr<SEApiContextItem> PushApiContext(CefString method,
						 CefRefPtr<CefListValue> args);
void RemoveApiContext(std::shared_ptr<SEApiContextItem> item);

class SEAsyncCallContextItem {
public:
	SEAsyncCallContextItem(std::string &file_, int line_, bool running_)
		: file(file_), line(line_), running(running_)
	{
#if _WIN32
		thread = GetCurrentThreadId();
#else
		thread = 0;
#endif
	}

public:
	std::string file = "";
	int line = -1;
	uint32_t thread = 0;
	bool running = false;
};

class SEAsyncCallContextStack_t
	: public std::list<std::shared_ptr<SEAsyncCallContextItem>> {
public:
	SEAsyncCallContextStack_t() {}
	~SEAsyncCallContextStack_t() { clear(); }
};

void GetAsyncCallContextStack(
	std::function<void(const SEAsyncCallContextStack_t *)> callback);

// Non-blocking variant for the crash path; false means the lock was held and
// the callback did not run. See the definition for why.
bool TryGetAsyncCallContextStack(
	std::function<void(const SEAsyncCallContextStack_t *)> callback);

std::shared_ptr<SEAsyncCallContextItem>
AsyncCallContextPush(std::string file, int line, bool running);
void AsyncCallContextRemove(std::shared_ptr<SEAsyncCallContextItem> item);

class SEAsyncCallContextMarker {
private:
	std::shared_ptr<SEAsyncCallContextItem> m_contextItem = nullptr;

public:
	SEAsyncCallContextMarker(const char *file, const int line,
				 bool running = true)
	{
		m_contextItem = AsyncCallContextPush(file, line, running);
	}

	~SEAsyncCallContextMarker()
	{
		AsyncCallContextRemove(m_contextItem);

		m_contextItem = nullptr;
	}
};

//
// Names the widget currently being destroyed, for whatever crash report the
// process produces while that is true.
//
// Qt aborts in _purecall when a virtual call reaches an object whose vtable is
// in the construction or destruction state -- a widget painted while it is
// being destroyed. We have 22 of those across 19 users on 26.9.4.994
// (SELIVE-8G) and cannot tell whether the widget is ours: Qt ships no PDBs in
// anything OBS publishes, so the frames above _purecall are unresolved, and no
// StreamElements frame appears in the stack at all.
//
// So the process records what it was doing instead. The crash context reads
// Current() and, when it says something, reports it as
// selive.widget.destroying. If those events start carrying it, the crash is
// ours and names the widget; if they never do, it is not ours (CORE-1922).
//
// Header-only on purpose: the crash context is compiled only when a crash
// backend is enabled, and this has to exist in every configuration that has
// widgets to destroy.
//
// The storage is a fixed buffer written without allocating and read without a
// lock. The reader runs on a dying process, possibly on this very thread a few
// frames deeper: a torn read costs one garbled attribute, while a lock would
// risk the crash handler waiting on the code that just crashed.
//
class SEWidgetTeardownScope {
public:
	SEWidgetTeardownScope(const char *kind, const char *id)
	{
		snprintf(m_previous, sizeof(m_previous), "%s", Buffer());

		snprintf(Buffer(), kBufferSize, "%s:%s",
			 kind ? kind : "(unnamed)", id ? id : "(unnamed)");
	}

	~SEWidgetTeardownScope()
	{
		// Restored rather than cleared: destroying a dock destroys the
		// widget inside it, so these nest, and the outer one is still
		// true afterwards.
		snprintf(Buffer(), kBufferSize, "%s", m_previous);
	}

	SEWidgetTeardownScope(const SEWidgetTeardownScope &) = delete;
	SEWidgetTeardownScope &
	operator=(const SEWidgetTeardownScope &) = delete;

	// Empty when nothing is being destroyed. snprintf always terminates, so
	// this is always a valid C string.
	static const char *Current() { return Buffer(); }

private:
	static const size_t kBufferSize = 160;

	// Zero-initialised before any dynamic initialisation runs, so it is
	// readable from the first instruction of the process.
	static char *Buffer()
	{
		static char buffer[kBufferSize] = {0};

		return buffer;
	}

	char m_previous[kBufferSize];
};
