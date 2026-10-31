//
// CORE-862 at the API handler: once a crash is being reported, a queued API
// call must not run -- and must still be completed, so nothing is left
// waiting on it.
//
// A process of its own, because SetCrashReportingInProgress() is one-way by
// design: it is written from a signal handler and nothing clears it, so one
// process gets one answer. Same reason test_session_signature_no_entropy is
// a separate executable.
//
// Why the handler has a check of its own, given that __QtPostTask_Impl also
// gates: the crash can begin after the task has been dequeued and before it
// runs. The harness's drain_posted(false) runs a task the production queue
// would have dropped, which is the only way to reach that second gate
// deliberately.
//

#include "api_handler_harness.hpp"

#include <cstdio>
#include <memory>

using namespace se_harness;

int main()
{
	std::shared_ptr<SEApiServices> services =
		std::make_shared<SEApiServices>();
	std::shared_ptr<RecordingSink> sink = std::make_shared<RecordingSink>();

	services->apiMessageSink = sink;
	services->ready = true;

	std::shared_ptr<StubApiMessageHandler> handler =
		std::make_shared<StubApiMessageHandler>("probe", services);

	handler->Register();

	// --- the queue's own gate: dropped, but still satisfied ----------
	{
		handler->OnProcessMessageReceived(
			client(), incoming_call("echo", {"\"before\""}, 1), 0);

		check(posted().size() == 1, "crash gate: one task posted");

		std::future<void> *waiter = nullptr;

		(void)waiter;

		SetCrashReportingInProgress();

		check(IsCrashReportingInProgress(),
		      "crash gate: the flag is set");

		drain_posted();

		check(stub_state().echoCalls == 0,
		      "crash gate: a task queued before the crash does not run "
		      "after it -- the consent prompt is modal, and a Win32 "
		      "modal loop drains this queue on the crashing thread");

		check(posted().empty(),
		      "crash gate: and the queue is emptied rather than left "
		      "to run later");
	}

	// --- the handler's own gate, reached by running it anyway ---------
	{
		logged().clear();
		sink->sent.clear();

		handler->OnProcessMessageReceived(
			client(), incoming_call("echo", {"\"during\""}, 2), 0);

		drain_posted(/* honourCrashGate */ false);

		check(stub_state().echoCalls == 0,
		      "crash gate: the handler refuses the call itself, so a "
		      "task already dequeued when the crash began does not "
		      "reach a body either");

		check(logged_contains("a crash is being reported"),
		      "crash gate: and says so");

		//
		// The point of the second gate is that it completes rather
		// than returns: a refused call still answers, so nothing is
		// left waiting and the context entry comes off the stack.
		//
		const SentMessage *reply = sink->find("executeCallback");

		check(reply != nullptr,
		      "crash gate: a refused call is still completed");

		std::size_t depth = 0;

		GetApiContext([&depth](SEApiContext_t *stack) {
			depth = stack ? stack->size() : 0;
		});

		check(depth == 0,
		      "crash gate: nothing left on the API context stack");
	}

	if (failures()) {
		std::fprintf(stderr, "%d assertion(s) failed\n", failures());
		return 1;
	}

	std::puts("test_api_handler_crash_gate: all assertions hold");
	return 0;
}
