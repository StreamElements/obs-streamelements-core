#pragma once

//
// One real SEApiMessageHandler over a fully populated fake container, shared
// by the two test files that dispatch host API calls:
//
//   test_api_handler_bodies.cpp     the narrative cases -- a body at a time,
//                                   each pinning something a reader would
//                                   otherwise have to infer
//   test_api_handler_contract.cpp   the contract table -- every registered
//                                   method, what it calls and what it
//                                   answers, which is broad rather than deep
//
// They are one executable on purpose. The production translation unit takes
// most of this target's build time, and compiling it twice to run two kinds
// of test over it would double that for nothing.
//

#include "api_handler_harness.hpp"
#include "fake_api_services.hpp"

#include "streamelements/SEApiMessageHandler.hpp"

#include <memory>
#include <string>
#include <vector>

namespace se_bodies {

//
// A test case. Both files fill an array of these; main() in the bodies file
// runs both arrays, so a case added to either is picked up without touching
// the other.
//
struct Test {
	const char *name;
	void (*fn)();
};

//
// Defined by test_api_handler_contract.cpp and run by main() in the bodies
// file, so a case added to either file needs no edit to the other.
//
extern const Test *const contract_tests;
extern const std::size_t contract_test_count;

//
// Fresh per test, and the journal is reset with it, so nothing one body did
// is visible to the next.
//
struct Fixture {
	se_fakes::FakeApiServices w;
	std::shared_ptr<se_harness::RecordingSink> sink =
		std::make_shared<se_harness::RecordingSink>();
	std::shared_ptr<SEApiMessageHandler> handler;

	explicit Fixture(const std::string &containerType = "probe")
	{
		//
		// The journal is reset by FakeApiServices' own constructor,
		// which also installs the invariants the fakes have to carry,
		// so nothing is cleared here -- clearing it after w was built
		// would throw those away.
		//
		w.services->apiMessageSink = sink;

		handler = std::make_shared<SEApiMessageHandler>(containerType,
								w.services);

		se_harness::world() = se_harness::World();
		se_harness::logged().clear();
		se_harness::posted().clear();

		handler->OnProcessMessageReceived(
			se_harness::client(), se_harness::context_created(), 0);

		sink->sent.clear();
	}

	se_fakes::Journal &journal() const { return se_fakes::journal(); }

	//
	// Dispatch one host API call and run it. Returns the JSON the page
	// would receive, or "<no reply>" if the handler never completed --
	// which is a real outcome and one of the things asserted.
	//
	std::string Call(const std::string &method,
			 const std::vector<std::string> &jsonArgs = {})
	{
		//
		// Cleared first so that what comes back belongs to THIS call:
		// the sink's find() answers with the earliest match, so a
		// second call on one fixture would otherwise read the first
		// call's reply. The journal is deliberately not cleared --
		// several tests assert across a sequence of calls.
		//
		sink->sent.clear();

		handler->OnProcessMessageReceived(
			se_harness::client(),
			se_harness::incoming_call(method, jsonArgs, 1), 0);

		se_harness::drain_posted();

		const se_harness::SentMessage *reply =
			sink->find("executeCallback");

		if (!reply || !reply->args || reply->args->GetSize() < 2)
			return "<no reply>";

		return reply->args->GetString(1).ToString();
	}

	std::size_t replies() const { return sink->count("executeCallback"); }
};

//
// The names the page is given, read back out of the handshake rather than
// counted in the source: this is the surface as the renderer sees it.
//
inline std::vector<std::string> registered_names()
{
	Fixture f;

	//
	// A second handshake. The fixture's constructor does one and then
	// clears the sink, so that a test asserting on a call's reply does
	// not have to step over three handshake messages first -- and this is
	// the one helper that wants them.
	//
	f.handler->OnProcessMessageReceived(se_harness::client(),
					    se_harness::context_created(), 0);

	const se_harness::SentMessage *functions =
		f.sink->find(MSG_BIND_JAVASCRIPT_FUNCTIONS);

	std::vector<std::string> names;

	if (!functions || !functions->args || functions->args->GetSize() < 2)
		return names;

	CefRefPtr<CefValue> parsed =
		CefParseJSON(functions->args->GetString(1), JSON_PARSER_RFC);
	CefRefPtr<CefDictionaryValue> dict = parsed ? parsed->GetDictionary()
						    : nullptr;

	if (!dict)
		return names;

	CefDictionaryValue::KeyList keys;

	dict->GetKeys(keys);

	for (const CefString &key : keys)
		names.push_back(key.ToString());

	return names;
}

} // namespace se_bodies
