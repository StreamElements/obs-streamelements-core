//
// The API handler infrastructure: the registry, the dispatcher, the API
// context stack, and the renderer handshake that builds window.host.
//
// This is the first coverage any of it has had. The 208 host API methods are
// still out of reach -- their bodies dereference 27 concrete service types,
// which is part 2 of the plan -- but none of what is tested here belongs to
// a particular method: an unknown method, a duplicate registration, a reply
// that arrives after the services are gone, the completion callback firing
// exactly once on every path, and the handshake's shape.
//
// The production translation unit is compiled as it ships. What stands in
// for something is listed in api_handler_harness.hpp, and it is all seams:
// four functions whose real bodies need libobs or Qt, plus the transport
// interface. SEDiagnosticsUtils.cpp is the real one, so the API-context
// stack under test is the stack the crash handler reads.
//

#include "api_handler_harness.hpp"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

using namespace se_harness;

namespace {

struct Fixture {
	std::shared_ptr<SEApiServices> services =
		std::make_shared<SEApiServices>();
	std::shared_ptr<RecordingSink> sink = std::make_shared<RecordingSink>();
	std::shared_ptr<StubApiMessageHandler> handler;

	Fixture()
	{
		services->apiMessageSink = sink;
		services->ready = true;

		handler = std::make_shared<StubApiMessageHandler>("probe",
								  services);

		world() = World();
		logged().clear();
		posted().clear();
		stub_state() = StubState();
	}
};

std::size_t api_context_depth()
{
	std::size_t depth = 0;

	GetApiContext([&depth](SEApiContext_t *stack) {
		depth = stack ? stack->size() : 0;
	});

	return depth;
}

std::vector<std::string> api_context_methods()
{
	std::vector<std::string> methods;

	GetApiContext([&methods](SEApiContext_t *stack) {
		if (!stack)
			return;

		for (const std::shared_ptr<SEApiContextItem> &item : *stack)
			methods.push_back(item->method.ToString());
	});

	return methods;
}

std::vector<std::string> dictionary_keys(CefRefPtr<CefDictionaryValue> dict)
{
	std::vector<std::string> keys;

	if (!dict)
		return keys;

	CefDictionaryValue::KeyList raw;

	dict->GetKeys(raw);

	for (const CefString &key : raw)
		keys.push_back(key.ToString());

	return keys;
}

// --- the handshake ----------------------------------------------------

//
// What a page gets when its JavaScript context comes up: the method list,
// the four properties, and hostReady. Nothing in the plug-in asserts this
// today, and the whole of window.host is built from it.
//
void the_handshake_binds_every_registered_method()
{
	Fixture f;

	const bool handled = f.handler->OnProcessMessageReceived(
		client(), context_created(), 0);

	check(handled,
	      "handshake: the context-created message must be handled");

	check_eq(f.sink->sent.size(), 3,
		 "handshake: three messages reach the page");

	const SentMessage *functions =
		f.sink->find(MSG_BIND_JAVASCRIPT_FUNCTIONS);

	check(functions != nullptr, "handshake: the method list must be sent");

	if (functions) {
		check_eq(functions->clientTarget, "probe",
			 "handshake: the method list goes to the calling "
			 "client");

		//
		// [0] is the container name the page binds them under; [1] is
		// the JSON the renderer turns into window.host.
		//
		check_eq(functions->args->GetString(0).ToString(), "host",
			 "handshake: the container is window.host");

		CefRefPtr<CefValue> parsed = CefParseJSON(
			functions->args->GetString(1), JSON_PARSER_RFC);

		check(parsed != nullptr,
		      "handshake: the method list must be valid JSON");

		if (parsed) {
			std::vector<std::string> keys =
				dictionary_keys(parsed->GetDictionary());

			//
			// Six names, not seven: "shadowed" is registered
			// twice and the registry is a map.
			//
			check_eq(keys.size(), 6,
				 "handshake: window.host gets one entry per "
				 "registered name");

			for (const char *const want :
			     {"echo", "argTypes", "counter", "reenter", "park",
			      "shadowed"}) {
				check(std::find(keys.begin(), keys.end(),
						want) != keys.end(),
				      std::string("handshake: window.host is "
						  "missing ") +
					      want);
			}
		}
	}

	const SentMessage *props = f.sink->find(MSG_BIND_JAVASCRIPT_PROPS);

	check(props != nullptr, "handshake: the properties must be sent");

	if (props) {
		CefRefPtr<CefValue> parsed = CefParseJSON(
			props->args->GetString(1), JSON_PARSER_RFC);

		CefRefPtr<CefDictionaryValue> dict =
			parsed ? parsed->GetDictionary() : nullptr;

		check(dict != nullptr,
		      "handshake: the properties must be valid JSON");

		if (dict) {
			check_eq(dictionary_keys(dict).size(), 4,
				 "handshake: four properties");

			check(dict->GetBool("hostReady"),
			      "handshake: hostReady must be true");

			check(!dict->GetBool("hostContainerHidden"),
			      "handshake: hostContainerHidden follows the "
			      "initial state, which defaults to visible");

			check(dict->HasKey("apiMajorVersion") &&
				      dict->HasKey("apiMinorVersion"),
			      "handshake: the API version must be published -- "
			      "the page reads it to decide what it may call");
		}
	}

	const SentMessage *ready = f.sink->find("hostReady");

	check(ready != nullptr, "handshake: the hostReady event must fire");

	if (ready) {
		check_eq(ready->kind, "jsEvent",
			 "handshake: hostReady goes to the one client, not to "
			 "every connected page");
	}
}

//
// Finding E of the 2026-10-26 audit, and the reason it is a test rather than
// a note: registration is lazy and gated.
//
// OnProcessMessageReceived returns early when the plug-in is not up yet, and
// context-created is one-shot per page context -- so a page that lands in
// that window gets no window.host at all, and nothing retries. The plan
// claimed the gate had been removed; it had moved up a level and become
// all-or-nothing.
//
void a_page_that_arrives_too_early_gets_nothing_and_is_never_retried()
{
	// --- the services are published but the root has not finished ----
	{
		Fixture f;

		f.services->ready = false;

		const bool handled = f.handler->OnProcessMessageReceived(
			client(), context_created(), 0);

		check(handled,
		      "early page: the message is still reported as handled, "
		      "which is what makes the loss silent");

		check_eq(f.sink->sent.size(), 0,
			 "early page: nothing is sent, so window.host is never "
			 "built");

		check_eq(f.handler->RegisteredCount(), 0,
			 "early page: nothing is even registered");

		//
		// The page is not told, and will not ask again: this message
		// arrives once per context. Readiness arriving afterwards
		// changes nothing for that page -- only a new context helps.
		//
		f.services->ready = true;

		check_eq(f.sink->sent.size(), 0,
			 "early page: becoming ready does not deliver anything "
			 "retroactively");
	}

	// --- libobs not up yet: the same door --------------------------
	{
		Fixture f;

		world().obsInitialized = false;

		f.handler->OnProcessMessageReceived(client(), context_created(),
						    0);

		check_eq(f.sink->sent.size(), 0,
			 "early page: obs_initialized() is the other half of "
			 "the same gate");
	}

	// --- and an unknown message is reported as not handled ---------
	{
		Fixture f;

		const bool handled = f.handler->OnProcessMessageReceived(
			client(), CefProcessMessage::Create("somethingElse"),
			0);

		check(!handled,
		      "an unrecognised renderer message must be reported as "
		      "not handled, so the caller can try something else");
	}
}

// --- the dispatcher ---------------------------------------------------

void a_call_runs_and_its_result_travels_back_as_json()
{
	Fixture f;

	f.handler->Register();

	f.handler->OnProcessMessageReceived(
		client(), incoming_call("echo", {"\"hello\""}, 7), 0);

	check_eq(stub_state().echoCalls, 0,
		 "dispatch: the handler must not run before the queue is "
		 "drained");

	check_eq(posted().size(), 1, "dispatch: exactly one task is posted");

	drain_posted();

	check_eq(stub_state().echoCalls, 1,
		 "dispatch: the handler runs exactly once");

	const SentMessage *reply = f.sink->find("executeCallback");

	check(reply != nullptr, "dispatch: the reply must be sent");

	if (reply) {
		check_eq(reply->args->GetInt(0), 7,
			 "dispatch: the reply carries the callback id it was "
			 "called with");

		check_eq(reply->args->GetString(1).ToString(), "\"hello\"",
			 "dispatch: the result is written back as JSON");
	}
}

void a_callback_id_of_minus_one_is_answered_with_nothing()
{
	Fixture f;

	f.handler->Register();

	f.handler->OnProcessMessageReceived(
		client(), incoming_call("echo", {"\"hi\""}, -1), 0);

	drain_posted();

	check_eq(stub_state().echoCalls, 1,
		 "fire and forget: the handler still runs");

	check_eq(f.sink->count("executeCallback"), 0,
		 "fire and forget: -1 means the page wants no reply, so none "
		 "is sent");
}

void an_unparsable_argument_becomes_null_rather_than_shifting_the_rest()
{
	Fixture f;

	f.handler->Register();

	//
	// Two arguments, the first of them not JSON at all. The dispatcher
	// substitutes null rather than skipping it, so the second argument
	// keeps its position -- skipping would silently shift every argument
	// after it by one, which is the kind of bug that surfaces as a method
	// misbehaving rather than as an error.
	//
	f.handler->OnProcessMessageReceived(
		client(),
		incoming_call("argTypes", {"{not json", "\"second\""}, 1), 0);

	drain_posted();

	const SentMessage *reply = f.sink->find("executeCallback");

	check(reply != nullptr, "bad json: the call still completes");

	if (reply) {
		check_eq(reply->args->GetString(1).ToString(),
			 "\"null,string\"",
			 "bad json: the unparsable argument arrives as null, "
			 "in its own position");
	}
}

void an_unknown_method_is_answered_with_null_exactly_once()
{
	Fixture f;

	f.handler->Register();

	int calls = 0;
	CefRefPtr<CefValue> answer;

	f.handler->InvokeApiCallHandlerAsync(
		CefProcessMessage::Create("direct"), client(), "noSuchMethod",
		CefListValue::Create(),
		[&](CefRefPtr<CefValue> r) {
			++calls;
			answer = r;
		},
		0);

	check_eq(calls, 1,
		 "unknown method: the caller is answered exactly once");

	check(answer && answer->GetType() == VTYPE_NULL,
	      "unknown method: the answer is null, not a hang");

	check(logged_contains("invalid API call to 'noSuchMethod'"),
	      "unknown method: it is logged, with the name");
}

//
// A duplicate name is not an error and not a warning: the registry is a
// std::map, so the second registration replaces the first and nothing says
// so. Latent in the plug-in -- all 208 names are unique today -- and pinned
// here because it is the kind of thing a merge introduces.
//
void a_duplicate_registration_silently_replaces_the_first()
{
	Fixture f;

	f.handler->Register();

	check_eq(f.handler->RegisteredCount(), 6,
		 "duplicate: seven registrations under six names");

	CefRefPtr<CefValue> answer;

	f.handler->InvokeApiCallHandlerAsync(
		CefProcessMessage::Create("direct"), client(), "shadowed",
		CefListValue::Create(),
		[&](CefRefPtr<CefValue> r) { answer = r; }, 0);

	check(answer != nullptr, "duplicate: the call is answered");

	if (answer) {
		check_eq(answer->GetString().ToString(), "second",
			 "duplicate: the last registration wins, silently");
	}
}

//
// The lock API_HANDLER_BEGIN takes is recursive, and this is what needs it:
// a body that invokes another method on the same thread before returning.
// With a plain mutex this deadlocks.
//
void a_handler_may_invoke_another_handler_on_the_same_thread()
{
	Fixture f;

	f.handler->Register();

	CefRefPtr<CefValue> answer;

	f.handler->InvokeApiCallHandlerAsync(
		CefProcessMessage::Create("direct"), client(), "reenter",
		CefListValue::Create(),
		[&](CefRefPtr<CefValue> r) { answer = r; }, 0);

	check_eq(stub_state().reentrantCalls, 1,
		 "re-entrancy: the outer handler ran");

	check_eq(stub_state().counterCalls, 1,
		 "re-entrancy: the inner handler ran too");

	check(answer && answer->GetInt() == 1,
	      "re-entrancy: the outer handler saw the inner result");
}

void an_async_handler_completes_nothing_until_it_says_so()
{
	Fixture f;

	f.handler->Register();

	int calls = 0;

	f.handler->InvokeApiCallHandlerAsync(
		CefProcessMessage::Create("direct"), client(), "park",
		CefListValue::Create(), [&](CefRefPtr<CefValue>) { ++calls; },
		0);

	check_eq(calls, 0,
		 "async handler: API_HANDLER_END_ASYNC calls nothing for you, "
		 "which is the whole difference from API_HANDLER_END");

	check(stub_state().parkedCompletion != nullptr,
	      "async handler: the body was handed a completion to keep");

	if (stub_state().parkedCompletion) {
		stub_state().parkedResult->SetString("late");
		stub_state().parkedCompletion();
	}

	check_eq(calls, 1, "async handler: completing once answers once");
}

// --- the services bundle ----------------------------------------------

void a_call_with_no_services_is_answered_rather_than_left_pending()
{
	Fixture f;

	f.handler->Register();

	int calls = 0;
	CefRefPtr<CefValue> answer;

	//
	// The handler holds the bundle weakly, so this is what a page sees
	// when the container it belonged to has been torn down under it.
	//
	f.services.reset();

	f.handler->InvokeApiCallHandlerAsync(
		CefProcessMessage::Create("direct"), client(), "echo",
		CefListValue::Create(),
		[&](CefRefPtr<CefValue> r) {
			++calls;
			answer = r;
		},
		0);

	check_eq(calls, 1,
		 "dead bundle: the caller is answered, not left waiting");

	check(answer && answer->GetType() == VTYPE_NULL,
	      "dead bundle: the answer is null");

	check_eq(stub_state().echoCalls, 0,
		 "dead bundle: the handler never ran -- there was nothing for "
		 "it to call");
}

//
// Current behaviour, recorded rather than endorsed: this is the one path
// through InvokeApiCallHandlerAsync that answers nothing at all. Its callers
// are InvokeHandler and the batchInvokeSeries registration, so what is left
// hanging is an internal invocation rather than a page's promise -- but it
// is still a return with no callback, and a test is where that gets noticed
// if it ever becomes a page-facing path.
//
void a_shut_down_handler_answers_nothing_at_all()
{
	Fixture f;

	f.handler->Register();

	f.handler->Shutdown();

	int calls = 0;

	f.handler->InvokeApiCallHandlerAsync(
		CefProcessMessage::Create("direct"), client(), "echo",
		CefListValue::Create(), [&](CefRefPtr<CefValue>) { ++calls; },
		0);

	check_eq(calls, 0,
		 "after Shutdown(): the caller gets no callback -- the one "
		 "path that answers nothing. If this ever becomes reachable "
		 "from a page, that page's promise never settles");

	check_eq(stub_state().echoCalls, 0,
		 "after Shutdown(): the handler does not run");
}

//
// The queued path has its own bundle lock, and its own reason: by the time
// the task runs, the bundle may have gone. It completes the call instead of
// running the handler.
//
void a_queued_call_whose_bundle_died_completes_without_running()
{
	Fixture f;

	f.handler->Register();

	f.handler->OnProcessMessageReceived(
		client(), incoming_call("echo", {"\"hello\""}, 3), 0);

	//
	// Readiness goes with the bundle, so the completion's own
	// IsPluginInitialized() check is what trips first -- which is the
	// production order too.
	//
	f.services.reset();

	drain_posted();

	check_eq(stub_state().echoCalls, 0,
		 "queued call, dead bundle: the handler does not run");

	check_eq(api_context_depth(), 0,
		 "queued call, dead bundle: nothing is left on the API "
		 "context stack");

	check(logged_contains("plugin is no longer initialized"),
	      "queued call, dead bundle: the abandoned call is logged");
}

// --- the API context stack -------------------------------------------

void the_api_context_is_pushed_for_the_call_and_popped_after_it()
{
	Fixture f;

	f.handler->Register();

	check_eq(api_context_depth(), 0,
		 "api context: nothing on the stack to begin with");

	//
	// Observed from inside the handler, which is the only place the entry
	// exists: pushed immediately before the body runs and removed by the
	// completion.
	//
	std::vector<std::string> seenFromInside;

	f.handler->RegisterIncomingApiCallHandler(
		"observe",
		[](std::shared_ptr<SEApiMessageHandlerBase>,
		   std::shared_ptr<SEApiServices>, CefRefPtr<CefProcessMessage>,
		   CefRefPtr<CefListValue>, CefRefPtr<CefValue> &result,
		   std::shared_ptr<SEApiClientInfo>, const long,
		   std::function<void()> complete_callback) {
			GetApiContext([](SEApiContext_t *stack) {
				stub_state().counterCalls = static_cast<int>(
					stack ? stack->size() : 0);
			});

			result->SetBool(true);
			complete_callback();
		});

	f.handler->OnProcessMessageReceived(client(),
					    incoming_call("observe", {}, 5), 0);

	drain_posted();

	check_eq(stub_state().counterCalls, 1,
		 "api context: the call is on the stack while its body runs, "
		 "which is what makes it appear in a crash report");

	check_eq(api_context_depth(), 0,
		 "api context: and off it once the call completes");

	check(api_context_methods().empty(), "api context: by name, too");
}

//
// The leak the plan calls out, as a test. The completion's reply path used
// to return early when the transport was gone, skipping both
// RemoveApiContext and the delete -- unreachable in practice until a lock
// could fail, routine afterwards.
//
void a_reply_with_no_transport_still_releases_the_context()
{
	Fixture f;

	f.handler->Register();

	f.handler->OnProcessMessageReceived(
		client(), incoming_call("echo", {"\"hello\""}, 9), 0);

	//
	// The bundle survives -- so IsPluginInitialized() passes and the
	// completion reaches its reply path -- but there is nothing to send
	// on.
	//
	f.services->apiMessageSink = nullptr;

	drain_posted();

	check_eq(stub_state().echoCalls, 1,
		 "no transport: the handler still ran");

	check_eq(api_context_depth(), 0,
		 "no transport: the context entry is still removed. If this "
		 "fails, the entry stays for the life of the process and "
		 "appears in every later crash report");
}

// --- Clone ------------------------------------------------------------

void clone_hands_back_the_same_handler()
{
	Fixture f;

	//
	// The base's Clone() is shared_from_this: one handler, one registry,
	// one runtime status. A subclass that genuinely needs a copy
	// overrides it -- the point of the test is that the default does not
	// silently make one, because a copy would have an empty registry.
	//
	std::shared_ptr<SEApiMessageHandlerBase> clone = f.handler->Clone();

	check(clone.get() == f.handler.get(),
	      "Clone(): the default hands back the same object");

	f.handler->Register();

	check(std::static_pointer_cast<StubApiMessageHandler>(clone)->HasHandler(
		      "echo"),
	      "Clone(): so a registration through one is visible through the "
	      "other");
}

void the_handler_reports_what_it_was_built_with()
{
	Fixture f;

	check_eq(f.handler->ContainerType(), "probe",
		 "the container type is readable by a handler body");

	check(f.handler->IsRunning(), "a fresh handler is running");

	f.handler->Shutdown();

	check(!f.handler->IsRunning(), "and stops being after Shutdown()");
}

} // namespace

int main()
{
	the_handshake_binds_every_registered_method();
	a_page_that_arrives_too_early_gets_nothing_and_is_never_retried();

	a_call_runs_and_its_result_travels_back_as_json();
	a_callback_id_of_minus_one_is_answered_with_nothing();
	an_unparsable_argument_becomes_null_rather_than_shifting_the_rest();
	an_unknown_method_is_answered_with_null_exactly_once();
	a_duplicate_registration_silently_replaces_the_first();
	a_handler_may_invoke_another_handler_on_the_same_thread();
	an_async_handler_completes_nothing_until_it_says_so();

	a_call_with_no_services_is_answered_rather_than_left_pending();
	a_shut_down_handler_answers_nothing_at_all();
	a_queued_call_whose_bundle_died_completes_without_running();

	the_api_context_is_pushed_for_the_call_and_popped_after_it();
	a_reply_with_no_transport_still_releases_the_context();

	clone_hands_back_the_same_handler();
	the_handler_reports_what_it_was_built_with();

	if (failures()) {
		std::fprintf(stderr, "%d assertion(s) failed\n", failures());
		return 1;
	}

	std::puts("test_api_handler_infrastructure: all assertions hold");
	return 0;
}
