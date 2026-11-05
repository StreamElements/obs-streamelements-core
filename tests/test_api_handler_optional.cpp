//
// The eight handler bodies the other test build cannot see.
//
// SEApiMessageHandler.cpp has 208 API_HANDLER_BEGIN blocks and the default
// test build compiles 200 of them: three sit behind SE_ENABLE_WYVRN, four
// behind SE_ENABLE_CENTRAL_WIDGET_DECORATIONS, and crashProgramFastFail
// behind WIN32. Until now those seven had coverage of exactly one kind --
// check_handler_bodies_type_check_before_they_read reads the SOURCE, so it
// checks their argument handling as text -- and no behavioural coverage at
// all. Nothing could break a test by changing them.
//
// So this target is the same translation unit again, compiled with both
// switches on. It is a separate executable because the switches change what
// the file contains, which is not something one binary can hold twice.
//
// The seventh is still out of reach and always will be:
// crashProgramFastFail calls __fastfail, which cannot be caught, observed or
// returned from. The sweep next door skips it for the same reason.
//
// SE_ENABLE_WYVRN is a build-system switch already, so nothing in production
// changed for it. SE_ENABLE_CENTRAL_WIDGET_DECORATIONS was defined
// unconditionally in ISENativeObsUIControlsManager.hpp, which made it a
// switch nobody could actually set; it is now #ifndef-guarded, which is two
// lines and what its own comment always implied.
//
// The most valuable body here is setRazerWyvrnEvent. At roughly sixty lines
// with a fallback chain, a depth limit and four answer paths it is the most
// involved in the file, it is reachable from any page on Windows, and it had
// no test of any kind.
//

#include "api_handler_fixture.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

using namespace se_harness;
using namespace se_bodies;

namespace {

//
// Both switches have to be on for this file to mean anything, and a
// mis-wired target would otherwise compile, run and pass while testing the
// handlers' absence.
//
#ifndef SE_ENABLE_WYVRN
#error "test_api_handler_optional needs SE_ENABLE_WYVRN"
#endif

#if !SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
#error "test_api_handler_optional needs SE_ENABLE_CENTRAL_WIDGET_DECORATIONS=1"
#endif

bool sequence_is(const Fixture &f, const std::vector<std::string> &wanted,
		 const std::string &what)
{
	const std::vector<std::string> got = f.journal().Sequence();

	if (got == wanted)
		return true;

	std::string gotText;
	std::string wantText;

	for (const std::string &s : got)
		gotText += (gotText.empty() ? "" : ";") + s;

	for (const std::string &s : wanted)
		wantText += (wantText.empty() ? "" : ";") + s;

	check(false,
	      what + " (got \"" + gotText + "\", wanted \"" + wantText + "\")");

	return false;
}

// --- the surface ------------------------------------------------------

//
// Non-vacuity first, and it is the assertion this whole executable rests
// on: with both switches on the surface grows by exactly the seven guarded
// names. If a define failed to reach the compiler the handlers below would
// simply not be registered, every call would answer "unknown method", and
// a test that only checked replies could still pass.
//
void the_guarded_handlers_are_registered_when_their_switches_are_on()
{
	const std::vector<std::string> names = registered_names();

	//
	//     208   API_HANDLER_BEGIN blocks in the file
	//      +1   batchInvokeSeries, the one hand-written registration
	//      -1   crashProgramFastFail, behind WIN32, elsewhere
	//     ---
	//     209   on Windows, 208 elsewhere
	//
#ifdef WIN32
	const std::size_t expected = 209;
#else
	const std::size_t expected = 208;
#endif

	check_eq(names.size(), expected,
		 "surface: every block is compiled in this build, so the "
		 "surface is the whole file plus batchInvokeSeries");

	for (const char *const want :
	     {"getHostCapabilities", "getAllRazerWyvrnEvents",
	      "setRazerWyvrnEvent", "showOutputPreviewTitleBar",
	      "hideOutputPreviewTitleBar", "showOutputPreviewFrame",
	      "hideOutputPreviewFrame"}) {
		check(std::find(names.begin(), names.end(), want) !=
			      names.end(),
		      std::string("surface: window.host is missing ") + want);
	}
}

// --- the WYVRN capability report --------------------------------------

//
// getHostCapabilities is the only place WYVRN availability is reported, and
// deliberately so -- a second status call could disagree with this one. It
// shares its serializer with the hostRazerWyvrnStatusChanged event, so what
// a page reads here is what the event carries.
//
void host_capabilities_reports_the_wyvrn_status_through_one_serializer()
{
	Fixture f;

	f.journal().SetJson("SERazerWyvrnManager::SerializeStatus",
			    "{\"available\":true,\"initialized\":true,"
			    "\"status\":\"ok\",\"eventCount\":4046}");

	check_eq(f.Call("getHostCapabilities"),
		 "{\"razerWyvrn\":{\"available\":true,\"eventCount\":4046,"
		 "\"initialized\":true,\"status\":\"ok\"},"
		 "\"sceneCollections\":\"available\"}",
		 "getHostCapabilities: the manager's own status, under "
		 "razerWyvrn, beside the documented sceneCollections member");

	sequence_is(f, {"SERazerWyvrnManager::SerializeStatus"},
		    "getHostCapabilities: asks the manager once");
}

//
// And with no manager at all, which is most of a session's first few
// seconds: the same SHAPE rather than null, so a page never has to branch
// on its absence. The body builds this itself, which is why it is worth
// pinning -- it is a second definition of the status object and could drift
// from the manager's.
//
void host_capabilities_reports_the_same_shape_when_wyvrn_is_absent()
{
	Fixture f;

	f.w.services->razerWyvrnManager = nullptr;

	check_eq(f.Call("getHostCapabilities"),
		 "{\"razerWyvrn\":{\"available\":false,\"eventCount\":0,"
		 "\"initialized\":false,\"status\":\"notCompiledIn\"},"
		 "\"sceneCollections\":\"available\"}",
		 "getHostCapabilities: four keys, the same four, when there "
		 "is no manager");

	check_eq(f.journal().calls.size(), 0,
		 "getHostCapabilities: and nothing is asked");
}

// --- the event list ---------------------------------------------------

//
// Three optional filters, and the one that is not a filter: `components`
// defaults to TRUE, so a page that sends nothing gets the chroma and
// haptics components of every event rather than a bare list.
//
void listing_wyvrn_events_passes_the_filters_and_defaults_components()
{
	{
		Fixture f;

		f.journal().SetJson("SERazerWyvrnManager::SerializeEvents",
				    "[{\"id\":\"e1\"}]");

		check_eq(f.Call("getAllRazerWyvrnEvents",
				{"{\"group\":\"combat\",\"idPrefix\":\"se.\","
				 "\"components\":false}"}),
			 "[{\"id\":\"e1\"}]",
			 "getAllRazerWyvrnEvents: the manager's list is the "
			 "reply");

		check_eq(f.journal().Count(
				 "SERazerWyvrnManager::SerializeEvents"),
			 1, "getAllRazerWyvrnEvents: asked once");

		const se_fakes::Call *call = f.journal().First(
			"SERazerWyvrnManager::SerializeEvents");

		if (call && call->args.size() == 3) {
			check_eq(call->args[0], "combat",
				 "getAllRazerWyvrnEvents: the group filter");
			check_eq(call->args[1], "se.",
				 "getAllRazerWyvrnEvents: the id prefix");
			check_eq(call->args[2], "false",
				 "getAllRazerWyvrnEvents: components as the "
				 "page asked");
		} else {
			check(false, "getAllRazerWyvrnEvents: three arguments");
		}
	}

	//
	// No arguments, and an argument of the wrong type, take the same
	// path: empty filters and components ON.
	//
	for (const char *const args : {"", "\"everything\""}) {
		Fixture f;

		f.journal().SetJson("SERazerWyvrnManager::SerializeEvents",
				    "[]");

		std::vector<std::string> call;

		if (*args)
			call.push_back(args);

		check_eq(f.Call("getAllRazerWyvrnEvents", call), "[]",
			 "getAllRazerWyvrnEvents: answers the manager's list");

		const se_fakes::Call *made = f.journal().First(
			"SERazerWyvrnManager::SerializeEvents");

		if (made && made->args.size() == 3) {
			check_eq(made->args[0], "",
				 "getAllRazerWyvrnEvents: no group filter");
			check_eq(made->args[1], "",
				 "getAllRazerWyvrnEvents: no id prefix");
			check_eq(made->args[2], "true",
				 "getAllRazerWyvrnEvents: components default "
				 "to true, which is the opposite of the usual "
				 "default-is-off reading");
		} else {
			check(false,
			      "getAllRazerWyvrnEvents: the manager is still "
			      "asked when the page sends nothing");
		}
	}
}

//
// With no manager the reply is an empty list rather than null or false --
// the only handler in the file that answers an absent service with a typed
// empty value, which is worth knowing because a page can then treat
// "WYVRN not up yet" and "no events configured" identically.
//
void listing_wyvrn_events_answers_an_empty_list_when_wyvrn_is_absent()
{
	Fixture f;

	f.w.services->razerWyvrnManager = nullptr;

	check_eq(f.Call("getAllRazerWyvrnEvents"), "[]",
		 "getAllRazerWyvrnEvents: an empty list, not null");

	check_eq(f.journal().calls.size(), 0,
		 "getAllRazerWyvrnEvents: nothing is asked");
}

// --- setting an event -------------------------------------------------

//
// The simple form: a string. The id is resolved against the user's scanned
// WYVRN configuration first, because CoreSetEventName accepts a name
// belonging to another configuration and then does nothing visible.
//
void setting_a_wyvrn_event_resolves_the_id_it_was_given()
{
	Fixture f;

	f.journal().SetString("SERazerWyvrnManager::ResolveEventId",
			      "combat.hit");
	f.journal().SetBool("SERazerWyvrnManager::SetEventName", true);

	check_eq(f.Call("setRazerWyvrnEvent", {"\"hit\""}), "true",
		 "setRazerWyvrnEvent: answers the manager's own result");

	sequence_is(f,
		    {"SERazerWyvrnManager::ResolveEventId",
		     "SERazerWyvrnManager::SetEventName"},
		    "setRazerWyvrnEvent: resolved, then set");

	check_eq(f.journal().Arg("SERazerWyvrnManager::ResolveEventId"), "hit",
		 "setRazerWyvrnEvent: the page's id is what gets resolved");
	check_eq(f.journal().Arg("SERazerWyvrnManager::SetEventName"),
		 "combat.hit",
		 "setRazerWyvrnEvent: and the RESOLVED name is what is sent");
}

//
// An empty string, an empty dictionary, and an argument of neither type all
// mean "stop": SetEventName("") turns the effects off. Three spellings of
// the same request, which is the kind of thing only a test says out loud.
//
void setting_a_wyvrn_event_treats_three_shapes_as_stop()
{
	for (const char *const args : {"\"\"", "{}", "7", ""}) {
		Fixture f;

		f.journal().SetBool("SERazerWyvrnManager::SetEventName", true);

		std::vector<std::string> call;

		if (*args)
			call.push_back(args);

		check_eq(f.Call("setRazerWyvrnEvent", call), "true",
			 std::string("setRazerWyvrnEvent: a stop request "
				     "answers the manager's result -- ") +
				 args);

		sequence_is(f, {"SERazerWyvrnManager::SetEventName"},
			    std::string("setRazerWyvrnEvent: stop asks for no "
					"resolution at all -- ") +
				    args);

		check_eq(f.journal().Arg("SERazerWyvrnManager::SetEventName"),
			 "",
			 std::string("setRazerWyvrnEvent: the empty name is "
				     "what turns the effects off -- ") +
				 args);
	}
}

//
// The fallback chain, which is the body's real content: a dictionary may
// name an id and a `fallback` dictionary, and the first id the user's
// configuration actually declares wins. A page can therefore ask for
// something specific and degrade to something generic without knowing what
// the user has installed.
//
void setting_a_wyvrn_event_walks_the_fallback_chain()
{
	//
	// Nothing resolves -- ResolveEventId answers empty, its default --
	// so every link is tried and the FIRST id is sent as given. That
	// last part is a deliberate choice rather than a failure: the SDK
	// may know a name the scan did not see.
	//
	{
		Fixture f;

		f.journal().SetBool("SERazerWyvrnManager::SetEventName", true);

		check_eq(f.Call("setRazerWyvrnEvent",
				{"{\"id\":\"a\",\"fallback\":{\"id\":\"b\","
				 "\"fallback\":{\"id\":\"c\"}}}"}),
			 "true", "setRazerWyvrnEvent: still answers");

		sequence_is(f,
			    {"SERazerWyvrnManager::ResolveEventId",
			     "SERazerWyvrnManager::ResolveEventId",
			     "SERazerWyvrnManager::ResolveEventId",
			     "SERazerWyvrnManager::SetEventName"},
			    "setRazerWyvrnEvent: all three links are tried");

		check_eq(f.journal().calls[0].args[0], "a",
			 "setRazerWyvrnEvent: in order, a first");
		check_eq(f.journal().calls[1].args[0], "b",
			 "setRazerWyvrnEvent: then b");
		check_eq(f.journal().calls[2].args[0], "c",
			 "setRazerWyvrnEvent: then c");
		check_eq(f.journal().Arg("SERazerWyvrnManager::SetEventName"),
			 "a",
			 "setRazerWyvrnEvent: and with nothing resolved the "
			 "FIRST id is sent as given, not the last tried");
	}

	//
	// The first link resolves, so the chain stops there and the rest is
	// never asked about.
	//
	{
		Fixture f;

		f.journal().SetString("SERazerWyvrnManager::ResolveEventId",
				      "known.event");
		f.journal().SetBool("SERazerWyvrnManager::SetEventName", true);

		check_eq(f.Call("setRazerWyvrnEvent",
				{"{\"id\":\"a\",\"fallback\":{\"id\":\"b\"}}"}),
			 "true", "setRazerWyvrnEvent: answers");

		sequence_is(f,
			    {"SERazerWyvrnManager::ResolveEventId",
			     "SERazerWyvrnManager::SetEventName"},
			    "setRazerWyvrnEvent: a resolved first link ends "
			    "the walk");

		check_eq(f.journal().Arg("SERazerWyvrnManager::SetEventName"),
			 "known.event",
			 "setRazerWyvrnEvent: the resolved name is sent");
	}

	//
	// An empty dictionary at the end of a chain is a STOP, and it
	// overrides the candidates already seen. This case was written
	// expecting "a" to be sent as given, the way the chain above ends,
	// and the test is what found otherwise:
	//
	//   {"id":"a"}                sends "a"
	//   {"id":"a","fallback":{}}  sends "" -- the effects go OFF
	//
	// The reason is the order of the answer paths: an empty id with
	// nothing to fall back to sets stopRequested and breaks, and
	// stopRequested is tested BEFORE both the resolved name and the
	// first id. So a page that appends an empty fallback to an otherwise
	// valid request silently converts "set this event" into "turn the
	// effects off".
	//
	// Recorded rather than changed: it is page-visible behaviour, and
	// the body's own comment does say an empty id with nothing to fall
	// back to is a stop. What it does not say is that this outranks a
	// candidate already seen.
	//
	{
		Fixture f;

		f.journal().SetBool("SERazerWyvrnManager::SetEventName", true);

		check_eq(f.Call("setRazerWyvrnEvent",
				{"{\"id\":\"a\",\"fallback\":{}}"}),
			 "true", "setRazerWyvrnEvent: answers");

		sequence_is(f,
			    {"SERazerWyvrnManager::ResolveEventId",
			     "SERazerWyvrnManager::SetEventName"},
			    "setRazerWyvrnEvent: the empty tail ends the walk");

		check_eq(f.journal().Arg("SERazerWyvrnManager::SetEventName"),
			 "",
			 "setRazerWyvrnEvent: an empty fallback turns the "
			 "effects off, discarding the id that was asked for");
	}
}

//
// The depth limit. A page could otherwise hand over a chain thousands of
// links long, or a cyclic one -- `fallback` is just another dictionary and
// nothing stops it repeating -- and the loop would resolve ids until it
// finished or did not. Sixteen is the cap, and sixteen is what a
// twenty-link chain gets.
//
void setting_a_wyvrn_event_stops_walking_after_sixteen_links()
{
	Fixture f;

	f.journal().SetBool("SERazerWyvrnManager::SetEventName", true);

	//
	// Twenty links, built rather than written out: {"id":"i0","fallback":
	// {"id":"i1","fallback":{...}}}
	//
	std::string chain;

	for (int i = 19; i >= 0; --i) {
		const std::string id = "i" + std::to_string(i);

		chain = chain.empty()
				? "{\"id\":\"" + id + "\"}"
				: "{\"id\":\"" + id +
					  "\",\"fallback\":" + chain + "}";
	}

	check_eq(f.Call("setRazerWyvrnEvent", {chain}), "true",
		 "setRazerWyvrnEvent: a long chain still answers");

	check_eq(f.journal().Count("SERazerWyvrnManager::ResolveEventId"), 16,
		 "setRazerWyvrnEvent: sixteen links are tried and the rest "
		 "ignored, which is kMaxFallbackDepth");

	check_eq(f.journal().Arg("SERazerWyvrnManager::SetEventName"), "i0",
		 "setRazerWyvrnEvent: and the first id is what gets sent");
}

//
// With no manager the answer is false and nothing is attempted -- note it
// is false rather than the empty list getAllRazerWyvrnEvents answers, so
// the two WYVRN readers disagree about how to say "not available".
//
void setting_a_wyvrn_event_answers_false_when_wyvrn_is_absent()
{
	Fixture f;

	f.w.services->razerWyvrnManager = nullptr;

	check_eq(f.Call("setRazerWyvrnEvent", {"\"hit\""}), "false",
		 "setRazerWyvrnEvent: false when there is no manager");

	check_eq(f.journal().calls.size(), 0,
		 "setRazerWyvrnEvent: and nothing is attempted");
}

// --- the preview decorations ------------------------------------------

//
// The four bodies behind SE_ENABLE_CENTRAL_WIDGET_DECORATIONS, which have
// never been compiled by anything -- the switch is 0 in every shipping
// build and was unsettable until this target needed it.
//
// They carry a persistence rule of their own, and it is a FOURTH one for
// this file: show* saves the layout whether or not the deserialize
// succeeded, where the dock handlers gate the save on the service's answer.
// A page sending a malformed title bar therefore provokes a layout save.
//
void the_preview_decorations_persist_whether_or_not_they_took_effect()
{
	struct Case {
		const char *method;
		const char *call;
	};

	const Case shown[] = {
		{"showOutputPreviewTitleBar",
		 "ISENativeObsUIControlsManager::DeserializePreviewTitleBar"},
		{"showOutputPreviewFrame",
		 "ISENativeObsUIControlsManager::DeserializePreviewFrame"},
	};

	for (const Case &c : shown) {
		{
			Fixture f;

			f.journal().SetBool(c.call, true);

			check_eq(f.Call(c.method, {"{\"height\":24}"}), "true",
				 std::string(c.method) +
					 ": answers the service's result");

			sequence_is(
				f, {c.call, "SEStatePersistence::PersistState"},
				std::string(c.method) +
					": deserialized, then persisted");

			check_eq(f.journal().Arg(c.call), "{\"height\":24}",
				 std::string(c.method) +
					 ": the page's description arrives "
					 "intact");
		}

		{
			Fixture f;

			check_eq(f.Call(c.method, {"{\"height\":24}"}), "false",
				 std::string(c.method) +
					 ": a refused description answers "
					 "false");

			sequence_is(
				f, {c.call, "SEStatePersistence::PersistState"},
				std::string(c.method) +
					": and the layout is persisted "
					"anyway, unlike every dock "
					"handler");
		}

		{
			Fixture f;

			check_eq(f.Call(c.method), "false",
				 std::string(c.method) +
					 ": no arguments, no answer");

			check_eq(f.journal().calls.size(), 0,
				 std::string(c.method) +
					 ": and nothing is touched");
		}
	}
}

//
// Hiding takes no arguments, answers true unconditionally, and persists --
// in that order, which matters because the persisted state is what a
// relaunch reads.
//
void hiding_a_preview_decoration_always_answers_true()
{
	struct Case {
		const char *method;
		const char *call;
	};

	const Case hidden[] = {
		{"hideOutputPreviewTitleBar",
		 "ISENativeObsUIControlsManager::HidePreviewTitleBar"},
		{"hideOutputPreviewFrame",
		 "ISENativeObsUIControlsManager::HidePreviewFrame"},
	};

	for (const Case &c : hidden) {
		Fixture f;

		check_eq(f.Call(c.method), "true",
			 std::string(c.method) + ": always true");

		sequence_is(f, {c.call, "SEStatePersistence::PersistState"},
			    std::string(c.method) + ": hidden, then persisted");
	}
}

// --- the run ----------------------------------------------------------

const Test tests[] = {
	{"the guarded handlers are registered when their switches are on",
	 the_guarded_handlers_are_registered_when_their_switches_are_on},
	{"host capabilities reports the wyvrn status through one serializer",
	 host_capabilities_reports_the_wyvrn_status_through_one_serializer},
	{"host capabilities reports the same shape when wyvrn is absent",
	 host_capabilities_reports_the_same_shape_when_wyvrn_is_absent},
	{"listing wyvrn events passes the filters and defaults components",
	 listing_wyvrn_events_passes_the_filters_and_defaults_components},
	{"listing wyvrn events answers an empty list when wyvrn is absent",
	 listing_wyvrn_events_answers_an_empty_list_when_wyvrn_is_absent},
	{"setting a wyvrn event resolves the id it was given",
	 setting_a_wyvrn_event_resolves_the_id_it_was_given},
	{"setting a wyvrn event treats three shapes as stop",
	 setting_a_wyvrn_event_treats_three_shapes_as_stop},
	{"setting a wyvrn event walks the fallback chain",
	 setting_a_wyvrn_event_walks_the_fallback_chain},
	{"setting a wyvrn event stops walking after sixteen links",
	 setting_a_wyvrn_event_stops_walking_after_sixteen_links},
	{"setting a wyvrn event answers false when wyvrn is absent",
	 setting_a_wyvrn_event_answers_false_when_wyvrn_is_absent},
	{"the preview decorations persist whether or not they took effect",
	 the_preview_decorations_persist_whether_or_not_they_took_effect},
	{"hiding a preview decoration always answers true",
	 hiding_a_preview_decoration_always_answers_true},
};

//
// As next door: a CefValue read of the wrong type is a bare throw, which
// outside a catch is std::terminate, and the Debug CRT answers that with a
// message box. A test executable waiting on a dialog hangs CI instead of
// failing it.
//
void fail_loudly_rather_than_popping_a_dialog()
{
#ifdef _WIN32
	_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
	_CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
	_CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
	_CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
	SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
#endif

	std::set_terminate([]() {
		std::fprintf(stderr,
			     "FAIL: std::terminate -- most likely a CefValue "
			     "read of the wrong type, which does a bare "
			     "throw\n");
		std::fflush(stderr);
		std::_Exit(2);
	});
}

} // namespace

int main()
{
	fail_loudly_rather_than_popping_a_dialog();

	for (const Test &t : tests) {
		std::printf("  %s\n", t.name);
		std::fflush(stdout);

		t.fn();
	}

	if (failures()) {
		std::fprintf(stderr, "\n%d check(s) failed\n", failures());
		return 1;
	}

	std::printf("\nall checks passed\n");

	return 0;
}
