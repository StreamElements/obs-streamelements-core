#pragma once

//
// The harness two test executables share: the link seams the API handler
// infrastructure is compiled against, a recording transport, and a stub
// subclass that registers five fake host API methods.
//
// Everything here stands in for something the plug-in build supplies from a
// translation unit that needs libobs or Qt. None of it stands in for the code
// under test: SEApiMessageHandlerBase.cpp is compiled as it ships, and so is
// SEDiagnosticsUtils.cpp, so the API-context stack and the crash flag are the
// real ones.
//
// There are two executables because SetCrashReportingInProgress() is one-way
// by design -- nothing clears it -- so the crash gate needs a process of its
// own. Same reason test_session_signature_no_entropy is a second executable
// rather than a second case.
//

#include "streamelements/SEApiMessageHandlerBase.hpp"

#include "streamelements/SEUtils/SEAsyncTaskUtils.hpp"
#include "streamelements/SEUtils/SEDiagnosticsUtils.hpp"
#include "streamelements/SEUtils/SEEnvConfigUtils.hpp"
#include "streamelements/SEUtils/SELifecycleUtils.hpp"
#include "streamelements/SEUtils/SELogUtils.hpp"

#include <cstdarg>
#include <cstdio>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

//
// The handshake message names are defined in the production .cpp.
//
extern const char *MSG_ON_CONTEXT_CREATED;
extern const char *MSG_INCOMING_API_CALL;
extern const char *MSG_BIND_JAVASCRIPT_FUNCTIONS;
extern const char *MSG_BIND_JAVASCRIPT_PROPS;

namespace se_harness {

// --- assertions ------------------------------------------------------

inline int &failures()
{
	static int n = 0;
	return n;
}

inline void check(bool cond, const std::string &msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg.c_str());
		++failures();
	}
}

inline void check_eq(const std::string &got, const std::string &want,
		     const std::string &msg)
{
	check(got == want,
	      msg + " (got \"" + got + "\", wanted \"" + want + "\")");
}

inline void check_eq(std::size_t got, std::size_t want, const std::string &msg)
{
	check(got == want, msg + " (got " + std::to_string(got) + ", wanted " +
				   std::to_string(want) + ")");
}

// --- the state the seams read ----------------------------------------

struct World {
	bool obsInitialized = true;
	bool traceLogLevel = false;
};

inline World &world()
{
	static World w;
	return w;
}

struct LogLine {
	int level;
	std::string text;
};

inline std::vector<LogLine> &logged()
{
	static std::vector<LogLine> lines;
	return lines;
}

inline bool logged_contains(const std::string &needle)
{
	for (const LogLine &line : logged()) {
		if (line.text.find(needle) != std::string::npos)
			return true;
	}

	return false;
}

// --- the posted-task queue -------------------------------------------

struct PostedTask {
	std::function<void()> task;
	std::shared_ptr<std::promise<void>> promise;
	std::shared_ptr<SEAsyncCallContextItem> contextItem;
	std::string file;
	int line = -1;
};

inline std::vector<PostedTask> &posted()
{
	static std::vector<PostedTask> tasks;
	return tasks;
}

//
// Drained on demand rather than by a thread, which is what makes "fired
// exactly once" assertable instead of raced. Safe here specifically: the
// infrastructure's one QtPostTask discards the future it is handed, so
// nothing is ever waiting for a task this queue has not run yet.
//
// honourCrashGate mirrors production: __QtPostTask_Impl drops the task but
// still satisfies the promise once crash reporting has begun (CORE-862).
// Passing false runs it anyway, which is how the infrastructure's OWN crash
// check -- a second gate, reachable in production only if the crash begins
// after the task was dequeued -- is reached from a test.
//
// Production's posted path has no shutting-down gate; that one belongs to
// __QtExecSync_Impl's cross-thread branch (CORE-1131), and the
// infrastructure never calls QtExecSync, so there is nothing here to stand
// in for.
//
inline void drain_posted(bool honourCrashGate = true)
{
	std::vector<PostedTask> tasks;

	tasks.swap(posted());

	for (PostedTask &t : tasks) {
		auto finish = [&t]() {
			AsyncCallContextRemove(t.contextItem);
			t.promise->set_value();
		};

		if (honourCrashGate && IsCrashReportingInProgress()) {
			finish();
			continue;
		}

		t.contextItem->running = true;

		try {
			t.task();
		} catch (...) {
			//
			// Production rethrows after finishing, so a throwing
			// task cannot leave an entry on the async-call stack
			// or strand a waiter. Mirrored, including the rethrow.
			//
			finish();
			throw;
		}

		finish();
	}
}

// --- the recording transport -----------------------------------------

struct SentMessage {
	std::string kind; // "clientMessage" or "jsEvent"
	std::string source;
	std::string clientTarget;
	std::string clientId;
	std::string name;             // message name, or event name
	std::string json;             // jsEvent only
	CefRefPtr<CefListValue> args; // clientMessage only
};

class RecordingSink : public ISEApiMessageSink {
public:
	std::vector<SentMessage> sent;

	uint16_t GetPort() const override { return 27952; }

	bool DispatchJSEvent(std::string source, std::string event,
			     std::string json) override
	{
		sent.push_back({"jsEventGlobal", source, "", "", event, json,
				nullptr});
		return true;
	}

	bool DispatchJSEvent(std::string source,
			     std::shared_ptr<SEApiClientInfo> clientInfo,
			     std::string event, std::string json) override
	{
		sent.push_back({"jsEvent", source,
				clientInfo ? clientInfo->m_target : "",
				clientInfo ? clientInfo->m_unique_id : "",
				event, json, nullptr});
		return true;
	}

	bool DispatchClientMessage(std::string source,
				   std::shared_ptr<SEApiClientInfo> clientInfo,
				   CefRefPtr<CefProcessMessage> msg) override
	{
		sent.push_back({"clientMessage", source,
				clientInfo ? clientInfo->m_target : "",
				clientInfo ? clientInfo->m_unique_id : "",
				msg ? msg->GetName().ToString() : "", "",
				msg ? msg->GetArgumentList() : nullptr});
		return true;
	}

	bool RegisterMessageHandler(std::string, message_handler_t) override
	{
		return true;
	}

	bool UnregisterMessageHandler(std::string, message_handler_t) override
	{
		return true;
	}

	const SentMessage *find(const std::string &name) const
	{
		for (const SentMessage &m : sent) {
			if (m.name == name)
				return &m;
		}

		return nullptr;
	}

	std::size_t count(const std::string &name) const
	{
		std::size_t n = 0;

		for (const SentMessage &m : sent) {
			if (m.name == name)
				++n;
		}

		return n;
	}
};

// --- message builders -------------------------------------------------

inline std::shared_ptr<SEApiClientInfo> client(std::string target = "probe",
					       std::string id = "probe-1")
{
	return std::make_shared<SEApiClientInfo>(target, id);
}

inline CefRefPtr<CefProcessMessage> context_created()
{
	return CefProcessMessage::Create(MSG_ON_CONTEXT_CREATED);
}

//
// The wire shape of an incoming call, as the renderer sends it: a header
// count, a slot the infrastructure does not read, the fully qualified method
// name, one JSON string per argument, and the callback id last.
//
inline CefRefPtr<CefProcessMessage>
incoming_call(const std::string &method,
	      const std::vector<std::string> &jsonArgs, int callbackId)
{
	CefRefPtr<CefProcessMessage> msg =
		CefProcessMessage::Create(MSG_INCOMING_API_CALL);

	CefRefPtr<CefListValue> args = msg->GetArgumentList();

	const int headerSize = 3;

	args->SetInt(0, headerSize);
	args->SetString(1, "unused");
	args->SetString(2, "window.host." + method);

	std::size_t at = static_cast<std::size_t>(headerSize);

	for (const std::string &json : jsonArgs)
		args->SetString(at++, json);

	args->SetInt(at, callbackId);

	return msg;
}

// --- the stub subclass -------------------------------------------------

struct StubState {
	int echoCalls = 0;
	int counterCalls = 0;
	int reentrantCalls = 0;
	std::function<void()> parkedCompletion;
	CefRefPtr<CefValue> parkedResult;
};

inline StubState &stub_state()
{
	static StubState s;
	return s;
}

//
// Registers five fake host API methods through the production macros, which
// means this file also expands API_HANDLER_BEGIN and so has to define the
// lock itself -- exactly as SEApiMessageHandler.cpp and SEBrowserDialog.cpp
// each do, and for the reason recorded on the macro.
//
class StubApiMessageHandler : public SEApiMessageHandlerBase {
public:
	StubApiMessageHandler(std::string containerType,
			      std::shared_ptr<SEApiServices> services)
		: SEApiMessageHandlerBase(containerType, services)
	{
	}

	int registrationCalls = 0;

	//
	// Public so a test can register without a renderer handshake.
	//
	using SEApiMessageHandlerBase::RegisterIncomingApiCallHandler;

	void Register() { RegisterIncomingApiCallHandlers(); }

	std::size_t RegisteredCount() const { return m_apiCallHandlers.size(); }

	bool HasHandler(const std::string &id) const
	{
		return m_apiCallHandlers.count(id) > 0;
	}

protected:
	void RegisterIncomingApiCallHandlers() override;
};

} // namespace se_harness
