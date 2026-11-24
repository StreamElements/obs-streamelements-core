// CORE-4732 / SELIVE-KN: a pending HTTP callback must finish before the
// message bus destroys its waiting-request map and mutex.
//
// The production defect is member-destruction order. The server map is
// declared before the waiting-request map, so reverse destruction retires
// the request state first. Only destruction of the servers stops and joins
// cpp-httplib's workers. This models that order with a real worker thread;
// an invalid access is recorded rather than performed as actual C++ UB.

#include <cstdio>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
	if (!condition) {
		std::fprintf(stderr, "FAIL: %s\n", message);
		++failures;
	}
}

struct WaitingState {
	std::recursive_mutex mutex;
	std::map<std::string, int> requests;
	std::promise<void> completion;
	std::shared_future<void> future = completion.get_future();
	bool alive = true;
	int invalidAccesses = 0;
};

struct ServerManager {
	explicit ServerManager(std::shared_ptr<WaitingState> state)
		: state(state), worker([state] {
			  state->future.wait();

			  std::lock_guard<decltype(state->mutex)> guard(
				  state->mutex);

			  if (!state->alive) {
				  ++state->invalidAccesses;
				  return;
			  }

			  state->requests.erase("pending");
		  })
	{
		state->requests["pending"] = 1;
	}

	~ServerManager() { Stop(); }

	void Stop()
	{
		if (!worker.joinable())
			return;

		state->completion.set_value();
		worker.join();
	}

	std::shared_ptr<WaitingState> state;
	std::thread worker;
};

struct BusModel {
	explicit BusModel(bool drainServersInDestructor)
		: state(std::make_shared<WaitingState>()),
		  servers(std::make_shared<ServerManager>(state)),
		  drainServersInDestructor(drainServersInDestructor)
	{
	}

	~BusModel()
	{
		if (drainServersInDestructor)
			servers.reset();

		{
			std::lock_guard<decltype(state->mutex)> guard(
				state->mutex);
			state->alive = false;
		}

		if (!drainServersInDestructor)
			servers.reset();
	}

	void AnswerRequest() { servers->Stop(); }

	std::shared_ptr<WaitingState> state;
	std::shared_ptr<ServerManager> servers;
	bool drainServersInDestructor;
};

void a_response_before_teardown_is_handled_normally()
{
	auto bus = std::make_unique<BusModel>(true);
	const auto state = bus->state;

	bus->AnswerRequest();
	bus.reset();

	check(state->requests.empty(),
	      "an answered request must be erased normally");
	check(state->invalidAccesses == 0,
	      "an answered request must not access destroyed bus state");
}

void destruction_drains_a_pending_request_before_destroying_its_state()
{
	auto state = std::make_shared<WaitingState>();

	{
		BusModel bus(true);
		state = bus.state;
		// No response: destructor drain releases the pending worker, just
		// as server shutdown releases it ahead of member destruction.
	}

	check(state->requests.empty(),
	      "the drained worker must finish erasing its request");
	check(state->invalidAccesses == 0,
	      "CORE-4732: server workers must be joined before waiting-request state is destroyed");
}

void several_pending_requests_all_finish_before_teardown()
{
	constexpr int count = 8;
	std::shared_ptr<WaitingState> states[count];

	for (int i = 0; i < count; ++i) {
		auto bus = std::make_unique<BusModel>(true);
		states[i] = bus->state;
		bus.reset();
	}

	for (const auto &state : states) {
		check(state->requests.empty(),
		      "every pending worker must finish during the server drain");
		check(state->invalidAccesses == 0,
		      "no pending worker may outlive its callback state");
	}
}

void the_old_member_order_reproduces_the_invalid_access()
{
	auto state = std::make_shared<WaitingState>();

	{
		BusModel bus(false);
		state = bus.state;
	}

	check(state->invalidAccesses == 1,
	      "non-vacuity: retiring request state before joining the worker must reproduce the invalid access");
}

} // namespace

int main()
{
	a_response_before_teardown_is_handled_normally();
	destruction_drains_a_pending_request_before_destroying_its_state();
	several_pending_requests_all_finish_before_teardown();
	the_old_member_order_reproduces_the_invalid_access();

	if (failures) {
		std::fprintf(stderr, "%d assertion(s) failed\n", failures);
		return 1;
	}

	std::puts("test_message_bus_lifetime: all assertions hold");
	return 0;
}
