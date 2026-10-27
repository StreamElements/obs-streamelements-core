//
// The link seams, and the stub's registrations.
//
// Separate from the header so the two executables share one definition of
// each seam rather than each getting its own copy.
//

#include "api_handler_harness.hpp"

#include <cstdarg>
#include <cstdio>
#include <mutex>

//
// The API call lock, defined here because this translation unit expands
// API_HANDLER_BEGIN. Forgetting it is a compile error, which is the intended
// outcome -- see the comment on the macro for why there is one of these per
// expanding translation unit rather than one in the base.
//
static std::recursive_mutex s_sync_api_call_mutex;

// --- the seams -------------------------------------------------------

bool IsObsInitialized()
{
	return se_harness::world().obsInitialized;
}

bool IsTraceLogLevel()
{
	return se_harness::world().traceLogLevel;
}

void SELogVa(int logLevel, const char *format, va_list args)
{
	char buf[4096];

	std::vsnprintf(buf, sizeof(buf), format, args);

	se_harness::logged().push_back({logLevel, std::string(buf)});
}

void SELog(int logLevel, const char *format, ...)
{
	va_list args;

	va_start(args, format);
	SELogVa(logLevel, format, args);
	va_end(args);
}

//
// Queued, not run. drain_posted() is what runs them, and the whole point of
// the substitution is that the test decides when.
//
std::future<void> __QtPostTask_Impl(std::function<void()> task,
				    std::string file, int line)
{
	auto promise = std::make_shared<std::promise<void>>();

	//
	// Pushed at post time, exactly as production does, so the async-call
	// context the crash handler reads is populated by the same sequence.
	//
	auto item = AsyncCallContextPush(file, line, false);

	se_harness::posted().push_back({task, promise, item, file, line});

	return promise->get_future();
}

namespace se_harness {

void StubApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	++registrationCalls;

	//
	// Type-checked before reading, which is not politeness: the local
	// CefValue model's GetString() does a bare `throw;` on a type
	// mismatch, and a bare throw outside a catch block calls
	// std::terminate -- so a handler that reads an argument it did not
	// check takes the process down rather than returning an error. Worth
	// knowing before writing the next one.
	//
	API_HANDLER_BEGIN("echo");
	{
		++stub_state().echoCalls;

		if (args->GetSize() > 0 && args->GetType(0) == VTYPE_STRING)
			result->SetString(args->GetString(0));
		else
			result->SetNull();
	}
	API_HANDLER_END();

	//
	// Reports the shape of what it was handed, which is how the
	// dispatcher's argument handling is asserted without reading a value
	// whose type is the thing under test.
	//
	API_HANDLER_BEGIN("argTypes");
	{
		std::string types;

		for (std::size_t i = 0; i < args->GetSize(); ++i) {
			if (i)
				types += ",";

			switch (args->GetType(i)) {
			case VTYPE_NULL:
				types += "null";
				break;
			case VTYPE_BOOL:
				types += "bool";
				break;
			case VTYPE_INT:
				types += "int";
				break;
			case VTYPE_DOUBLE:
				types += "double";
				break;
			case VTYPE_STRING:
				types += "string";
				break;
			case VTYPE_DICTIONARY:
				types += "dictionary";
				break;
			case VTYPE_LIST:
				types += "list";
				break;
			default:
				types += "other";
				break;
			}
		}

		result->SetString(types);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("counter");
	{
		result->SetInt(++stub_state().counterCalls);
	}
	API_HANDLER_END();

	//
	// Invokes another registered method synchronously, on this thread,
	// from inside a handler body -- which is only possible because the
	// API call lock is recursive. A plain mutex deadlocks here.
	//
	API_HANDLER_BEGIN("reenter");
	{
		++stub_state().reentrantCalls;

		CefRefPtr<CefValue> inner;

		self->InvokeApiCallHandlerAsync(
			message, target, "counter", CefListValue::Create(),
			[&inner](CefRefPtr<CefValue> r) { inner = r; },
			cefClientId);

		result->SetInt(inner && inner->GetType() == VTYPE_INT
				       ? inner->GetInt()
				       : -1);
	}
	API_HANDLER_END();

	//
	// Hands the work off and completes later, which is what
	// API_HANDLER_END_ASYNC exists for: nothing is called for you, so a
	// body that forgets leaves the caller waiting forever.
	//
	API_HANDLER_BEGIN("park");
	{
		stub_state().parkedCompletion = complete_callback;
		stub_state().parkedResult = result;
	}
	API_HANDLER_END_ASYNC();

	//
	// Registered twice on purpose, under one name. The registry is a
	// std::map, so the second silently replaces the first -- latent in
	// the plug-in today, where all 208 names happen to be unique.
	//
	API_HANDLER_BEGIN("shadowed");
	{
		result->SetString("first");
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("shadowed");
	{
		result->SetString("second");
	}
	API_HANDLER_END();
}

} // namespace se_harness
