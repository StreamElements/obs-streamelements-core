//
// The host API method bodies -- the 208 API_HANDLER_BEGIN blocks in
// SEApiMessageHandler.cpp, which until now had no coverage of any kind.
//
// The production translation unit is compiled and linked as it ships. What
// stands in for something is the services it dereferences: thirteen
// interfaces are faked and fifteen concrete services have their definitions
// supplied here, both reporting into one journal -- see
// fake_api_services.hpp for why those are two different mechanisms.
//
// What that buys, and it is the point of the whole exercise: a body can be
// asked what it calls, in what order, with which arguments, and what JSON it
// hands back to the page. A live CDP probe can see the last of those four
// and nothing else -- it cannot tell DEST_ALL_LOCAL from DEST_ALL, or a dock
// whose url was omitted from one whose url was empty, and it cannot reach a
// branch that needs a service to fail.
//
// Two things are deliberately not here. The bodies' own correctness against
// OBS is not testable at all from this side -- an interface makes the
// CONSUMER testable, not the service. And three handlers exist to take the
// process down (crashProgram, crashProgramFastFail, deadlockProgram), so the
// sweep below leaves them alone and says so.
//

#include "api_handler_harness.hpp"
#include "fake_api_services.hpp"

#include "streamelements/SEApiMessageHandler.hpp"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <memory>
#include <string>
#include <vector>

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

using namespace se_harness;

namespace {

// --- the fixture ------------------------------------------------------

//
// A real SEApiMessageHandler over a fully populated container. Fresh per
// test, and the journal is reset with it, so nothing one body did is
// visible to the next.
//
struct Fixture {
	se_fakes::FakeApiServices w;
	std::shared_ptr<RecordingSink> sink = std::make_shared<RecordingSink>();
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

		world() = World();
		logged().clear();
		posted().clear();

		handler->OnProcessMessageReceived(client(), context_created(),
						  0);

		sink->sent.clear();
	}

	se_fakes::Journal &journal() const { return se_fakes::journal(); }

	//
	// Dispatch one host API call and run it. Returns the JSON the page
	// would receive, or "<no reply>" if the handler never completed --
	// which is a real outcome and one of the things asserted below.
	//
	std::string Call(const std::string &method,
			 const std::vector<std::string> &jsonArgs = {})
	{
		//
		// Cleared first so that what comes back belongs to THIS call:
		// the sink's find() answers with the earliest match, so a
		// second call on one fixture would otherwise read the first
		// call's reply. The journal is deliberately not cleared --
		// several tests below assert across a sequence of calls.
		//
		sink->sent.clear();

		handler->OnProcessMessageReceived(
			client(), incoming_call(method, jsonArgs, 1), 0);

		drain_posted();

		const SentMessage *reply = sink->find("executeCallback");

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
std::vector<std::string> registered_names()
{
	Fixture f;

	Fixture g("probe");

	(void)f;

	g.handler->OnProcessMessageReceived(client(), context_created(), 0);

	const SentMessage *functions =
		g.sink->find(MSG_BIND_JAVASCRIPT_FUNCTIONS);

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

// --- the surface ------------------------------------------------------

//
// 202 in this build, and the arithmetic is worth writing down because the
// number is build-config-specific and has been misreported as a method count
// more than once:
//
//     208   API_HANDLER_BEGIN blocks in the file
//      -4   behind SE_ENABLE_CENTRAL_WIDGET_DECORATIONS, which is 0
//      -3   behind SE_ENABLE_WYVRN, undefined here
//      +1   batchInvokeSeries, the one hand-written registration
//     ---
//     202
//
// WIN32 is defined in this build, so the one block behind it counts. The
// live figure differs again -- window.host has 210 KEYS, which is the
// registered methods plus four properties.
//
void the_surface_is_exactly_what_the_arithmetic_says()
{
	std::vector<std::string> names = registered_names();

	check_eq(names.size(), 202,
		 "surface: 208 blocks less the seven compiled out, plus the "
		 "hand-written batchInvokeSeries");

	//
	// The registry is a std::map, so a duplicate name would silently
	// replace the earlier registration and the count above would be the
	// only sign. All 208 names are unique today; this is what keeps that
	// true.
	//
	std::vector<std::string> sorted = names;

	std::sort(sorted.begin(), sorted.end());

	check(std::adjacent_find(sorted.begin(), sorted.end()) == sorted.end(),
	      "surface: every registered name is unique");

	for (const char *const want :
	     {"getAllScenes", "addDockingWidget", "broadcastEvent",
	      "getCurrentContainerProperties", "batchInvokeSeries"}) {
		check(std::find(names.begin(), names.end(), want) !=
			      names.end(),
		      std::string("surface: window.host is missing ") + want);
	}
}

//
// The sweep: every method the page is offered, called with no arguments,
// has to answer exactly once. No assertion about WHAT it answers -- the
// point is that it answers at all, and that no body takes the process down
// or dereferences something the container left null.
//
// This is the cheapest broad coverage available and it earns its place: a
// handler that forgets its completion leaves a page promise pending for
// ever, which is invisible in review and indistinguishable from a slow call
// at runtime.
//
void every_handler_answers_exactly_once()
{
	//
	// The three that exist to take the process down, and the one that is
	// known not to answer -- see
	// an_async_dialog_handler_with_no_arguments_never_answers, which
	// pins that as the defect it is.
	//
	const std::vector<std::string> skip = {"crashProgram",
					       "crashProgramFastFail",
					       "deadlockProgram",
					       "showNonModalDialog"};

	std::vector<std::string> names = registered_names();

	std::size_t swept = 0;
	std::size_t silent = 0;

	for (const std::string &name : names) {
		if (std::find(skip.begin(), skip.end(), name) != skip.end())
			continue;

		//
		// Named and flushed before the call, not after: a body that
		// takes the process down cannot report which one it was, and
		// the local CefValue model has more than one way to do that.
		// ctest hides this on a pass.
		//
		std::printf("      sweep: %s\n", name.c_str());
		std::fflush(stdout);

		Fixture f;

		f.Call(name);

		if (f.replies() != 1) {
			++silent;

			check(false, "sweep: " + name + " answered " +
					     std::to_string(f.replies()) +
					     " times, wanted exactly 1");
		}

		++swept;
	}

	check_eq(swept, names.size() - skip.size(),
		 "sweep: every registered method bar the four skipped is "
		 "exercised");

	check_eq(silent, 0,
		 "sweep: no handler may leave the page without an answer");
}

//
// What a page gets from a handler that decided to do nothing. The
// dispatcher pre-sets the result to false before the body runs
// (SEApiMessageHandlerBase.cpp), so "false" is the answer to a rejected
// argument throughout the surface -- not null, and not an error. A body
// that wants null has to say so, and twenty or so do.
//
// Worth a test of its own because every refusal case below depends on it,
// and because a page cannot tell this false from a deliberate one.
//
void a_body_that_does_nothing_answers_false_rather_than_null()
{
	Fixture f;

	check_eq(f.Call("addDockingWidget"), "false",
		 "default: a body that leaves the result alone answers false");

	Fixture g;

	check_eq(g.Call("getVideoCompositionViewOverlayProperties"), "null",
		 "default: a body that opens with result->SetNull() answers "
		 "null, which is the only way to tell the page \"nothing\" "
		 "rather than \"no\"");
}

// --- the arguments a page can send that used to kill the process ------

//
// Every typed getter in the local CefValue model is
// `if (m_type != VTYPE_X) throw;`, and a bare throw outside a catch calls
// std::terminate. So a body reading a page-supplied value without checking
// its type first is a process kill that page JavaScript can trigger with
// one wrong argument.
//
// Three handlers were reachable that way and a fourth pair read index 0 of
// an argument list that could be empty, which is an unchecked
// std::vector subscript. All five are guarded now; these are the
// regression tests, and each one of them terminated the test process
// before the guard went in.
//
void a_wrongly_typed_argument_is_refused_rather_than_fatal()
{
	//
	// args->GetList(0) on a string. Before the guard this reached
	// CefValueImpl::GetList's bare throw.
	//
	{
		Fixture f;

		check_eq(f.Call("removeDockingWidgetsByIds", {"\"a\""}),
			 "false",
			 "removeDockingWidgetsByIds: a string instead of a "
			 "list is refused");

		check_eq(f.Call("removeBackgroundWorkersByIds", {"42"}),
			 "false",
			 "removeBackgroundWorkersByIds: a number instead of a "
			 "list is refused");
	}

	//
	// A list whose ELEMENTS are the wrong type. The element read was
	// unguarded too, so this took the process down even with a list.
	//
	{
		Fixture f;

		f.journal().SetBool("ISEBrowserWidgetManager::RemoveDockWidget",
				    true);

		check_eq(f.Call("removeDockingWidgetsByIds",
				{"[\"a\",7,\"b\"]"}),
			 "true",
			 "removeDockingWidgetsByIds: a mixed list is accepted");

		check_eq(f.journal().Count(
				 "ISEBrowserWidgetManager::RemoveDockWidget"),
			 2,
			 "removeDockingWidgetsByIds: the two strings are "
			 "removed and the number is skipped, rather than the "
			 "batch failing");
	}

	//
	// showCentralWidget read args[0] as a dictionary, and then read
	// "url" and "executeJavaScriptCodeOnLoad" on HasKey alone -- which
	// answers for presence, not type.
	//
	{
		Fixture f;

		check_eq(f.Call("showCentralWidget", {"\"not a dictionary\""}),
			 "false",
			 "showCentralWidget: a string instead of a dictionary "
			 "is refused");

		check_eq(f.Call("showCentralWidget", {"{\"url\":7}"}), "false",
			 "showCentralWidget: a url that is not a string is "
			 "refused -- HasKey said it was there");
	}

	{
		Fixture f;

		f.journal().SetBool("ISEBrowserWidgetManager::"
				    "DestroyCurrentCentralBrowserWidget",
				    false);

		check_eq(f.Call("showCentralWidget",
				{"{\"url\":\"https://example.com/\","
				 "\"executeJavaScriptCodeOnLoad\":7}"}),
			 "true",
			 "showCentralWidget: a well-typed url is accepted "
			 "even when the optional script is the wrong type");

		check_eq(f.journal().Arg("ISEBrowserWidgetManager::"
					 "PushCentralBrowserWidget",
					 1),
			 "",
			 "showCentralWidget: a script of the wrong type is "
			 "dropped, and the empty string is passed instead");
	}

	//
	// The pair that read index 0 of a list that may be empty. This is a
	// different mechanism -- CefListValue::GetValue is an unchecked
	// std::vector subscript, so it is undefined behaviour rather than a
	// throw, and in a release build it reads past the end of the buffer
	// and refcounts whatever it finds.
	//
	{
		Fixture f;

		check_eq(f.Call("getAvailableAudioEncoderClassProperties"),
			 "false",
			 "getAvailableAudioEncoderClassProperties: no "
			 "arguments must not read index 0 of an empty list");

		check_eq(f.Call("getAvailableVideoEncoderClassProperties"),
			 "false",
			 "getAvailableVideoEncoderClassProperties: the same, "
			 "and it was the same commit");

		check_eq(f.journal().calls.size(), 0,
			 "the encoder property handlers: and neither reaches "
			 "the composition manager");
	}
}

// --- docks: three different rules about persisting --------------------

//
// The sixteen dock handlers all end in "update the menu and persist the
// layout", and they do NOT agree about when. Three rules, and the asymmetry
// is the reason these are tested one at a time rather than as a family.
//

//
// Rule one: addDockingWidget persists only if the dock it just asked for
// actually exists afterwards. HasDockWidget is what part 1 put there in
// place of a GetDockWidget that returned a QDockWidget * purely to be
// tested for null, so this is the body that proves the replacement kept the
// behaviour.
//
void adding_a_dock_persists_only_once_the_dock_exists()
{
	{
		Fixture f;

		f.journal().SetString(
			"ISEBrowserWidgetManager::AddDockBrowserWidget",
			"dock-1");
		f.journal().SetBool("ISEBrowserWidgetManager::HasDockWidget",
				    false);

		const std::string reply =
			f.Call("addDockingWidget", {"{\"url\":\"a\"}"});

		check_eq(reply, "false",
			 "addDockingWidget: a dock that did not appear is "
			 "answered with the dispatcher's default, not an id");

		check_eq(f.journal().Count("ISEMenuManager::Update"), 0,
			 "addDockingWidget: the menu is not rebuilt for a "
			 "dock that did not appear");

		check_eq(f.journal().Count("SEStatePersistence::PersistState"),
			 0,
			 "addDockingWidget: nothing is persisted for a dock "
			 "that did not appear");
	}

	{
		Fixture f;

		f.journal().SetString(
			"ISEBrowserWidgetManager::AddDockBrowserWidget",
			"dock-1");
		f.journal().SetBool("ISEBrowserWidgetManager::HasDockWidget",
				    true);

		const std::string reply =
			f.Call("addDockingWidget", {"{\"url\":\"a\"}"});

		check_eq(reply, "\"dock-1\"",
			 "addDockingWidget: the new dock's id goes back to "
			 "the page");

		check_eq(
			f.journal().Arg(
				"ISEBrowserWidgetManager::AddDockBrowserWidget",
				0),
			"{\"url\":\"a\"}",
			"addDockingWidget: the widget description is passed "
			"through unchanged");

		check_eq(f.journal().Arg(
				 "ISEBrowserWidgetManager::HasDockWidget", 0),
			 "dock-1",
			 "addDockingWidget: the id just returned is the id "
			 "checked for");

		check_eq(f.journal().Count("ISEMenuManager::Update"), 1,
			 "addDockingWidget: the menu is rebuilt once");

		check_eq(f.journal().Count("SEStatePersistence::PersistState"),
			 1, "addDockingWidget: the layout is persisted once");

		//
		// Order matters at shutdown: the menu is rebuilt from the
		// live widgets, and the persisted state is what a relaunch
		// reads. Rebuilding after persisting would write a layout
		// the menu does not describe.
		//
		const std::vector<std::string> seq = f.journal().Sequence();

		const auto menu = std::find(seq.begin(), seq.end(),
					    "ISEMenuManager::Update");
		const auto persist =
			std::find(seq.begin(), seq.end(),
				  "SEStatePersistence::PersistState");

		check(menu < persist,
		      "addDockingWidget: the menu is rebuilt before the "
		      "layout is persisted");
	}
}

void adding_a_dock_with_no_arguments_touches_nothing()
{
	Fixture f;

	const std::string reply = f.Call("addDockingWidget");

	check_eq(reply, "false",
		 "addDockingWidget: no arguments leaves the result untouched, "
		 "which the page sees as false");

	check_eq(f.journal().calls.size(), 0,
		 "addDockingWidget: no arguments reaches no service at all");
}

//
// Rule two: removeDockingWidgetsByIds persists unconditionally. It discards
// RemoveDockWidget's return value and answers true for a list of ids that
// matched nothing -- including an empty list. Recorded rather than fixed:
// the page cannot currently tell "removed" from "there was nothing to
// remove", and nothing in the plug-in depends on the difference.
//
void removing_docks_reports_success_without_checking_anything_was_removed()
{
	Fixture f;

	f.journal().SetBool("ISEBrowserWidgetManager::RemoveDockWidget", false);

	const std::string reply =
		f.Call("removeDockingWidgetsByIds", {"[\"ghost\"]"});

	check_eq(reply, "true",
		 "removeDockingWidgetsByIds: an id that matched nothing is "
		 "still reported as success");

	check_eq(f.journal().Count("ISEBrowserWidgetManager::RemoveDockWidget"),
		 1, "removeDockingWidgetsByIds: the removal was attempted");

	check_eq(f.journal().Count("SEStatePersistence::PersistState"), 1,
		 "removeDockingWidgetsByIds: the layout is persisted whether "
		 "or not anything changed");
}

void removing_docks_passes_every_id_and_persists_once()
{
	Fixture f;

	f.journal().SetBool("ISEBrowserWidgetManager::RemoveDockWidget", true);

	const std::string reply =
		f.Call("removeDockingWidgetsByIds", {"[\"a\",\"b\",\"c\"]"});

	check_eq(reply, "true", "removeDockingWidgetsByIds: success");

	check_eq(f.journal().Count("ISEBrowserWidgetManager::RemoveDockWidget"),
		 3, "removeDockingWidgetsByIds: one removal per id");

	std::vector<std::string> removed;

	for (const se_fakes::Call &c : f.journal().calls) {
		if (c.Qualified() ==
		    "ISEBrowserWidgetManager::RemoveDockWidget")
			removed.push_back(c.args.empty() ? "" : c.args[0]);
	}

	check(removed == std::vector<std::string>({"a", "b", "c"}),
	      "removeDockingWidgetsByIds: the ids are removed in the order "
	      "the page listed them");

	check_eq(f.journal().Count("ISEMenuManager::Update"), 1,
		 "removeDockingWidgetsByIds: the menu is rebuilt once for the "
		 "whole batch, not once per id");

	check_eq(f.journal().Count("SEStatePersistence::PersistState"), 1,
		 "removeDockingWidgetsByIds: the layout is persisted once for "
		 "the whole batch");
}

void removing_docks_with_something_that_is_not_a_list_touches_nothing()
{
	Fixture f;

	const std::string reply =
		f.Call("removeDockingWidgetsByIds", {"\"a\""});

	check_eq(reply, "false",
		 "removeDockingWidgetsByIds: a string where a list was "
		 "expected is refused rather than killing the process");

	check_eq(f.journal().calls.size(), 0,
		 "removeDockingWidgetsByIds: and reaches no service");
}

//
// Rule three: the set/toggle handlers persist only if the widget manager
// reports the change took effect. toggleDockingWidgetFloatingById stands
// for the group -- eight handlers share this shape exactly.
//
void a_widget_change_that_did_not_take_effect_is_not_persisted()
{
	{
		Fixture f;

		f.journal().SetBool("ISEBrowserWidgetManager::"
				    "ToggleWidgetFloatingStateById",
				    false);

		const std::string reply = f.Call(
			"toggleDockingWidgetFloatingById", {"\"dock-1\""});

		check_eq(reply, "false",
			 "toggleDockingWidgetFloatingById: a refused toggle "
			 "is reported as false");

		check_eq(f.journal().Count("ISEMenuManager::Update"), 0,
			 "toggleDockingWidgetFloatingById: a refused toggle "
			 "does not rebuild the menu");

		check_eq(f.journal().Count("SEStatePersistence::PersistState"),
			 0,
			 "toggleDockingWidgetFloatingById: a refused toggle "
			 "does not persist");
	}

	{
		Fixture f;

		f.journal().SetBool("ISEBrowserWidgetManager::"
				    "ToggleWidgetFloatingStateById",
				    true);

		const std::string reply = f.Call(
			"toggleDockingWidgetFloatingById", {"\"dock-1\""});

		check_eq(reply, "true",
			 "toggleDockingWidgetFloatingById: an accepted toggle "
			 "is reported as true");

		check_eq(f.journal().Arg("ISEBrowserWidgetManager::"
					 "ToggleWidgetFloatingStateById",
					 0),
			 "dock-1",
			 "toggleDockingWidgetFloatingById: the id is passed "
			 "through");

		check_eq(f.journal().Count("SEStatePersistence::PersistState"),
			 1,
			 "toggleDockingWidgetFloatingById: an accepted toggle "
			 "persists once");
	}
}

//
// A coordinate the page left out is not left alone: it becomes -1 and is
// passed to the widget manager as a real value. So the service, not the
// handler, decides what "unspecified" means -- and a page that sends
// {"left": 10} with no "top" is asking for top = -1 whether it knows it or
// not.
//
void a_coordinate_the_page_omitted_is_passed_through_as_minus_one()
{
	Fixture f;

	f.journal().SetBool("ISEBrowserWidgetManager::SetWidgetPositionById",
			    true);

	f.Call("setDockingWidgetPositionById", {"\"dock-1\"", "{\"left\":10}"});

	check_eq(f.journal().Count(
			 "ISEBrowserWidgetManager::SetWidgetPositionById"),
		 1, "setDockingWidgetPositionById: the position was set");

	check_eq(f.journal().Arg(
			 "ISEBrowserWidgetManager::SetWidgetPositionById", 1),
		 "10",
		 "setDockingWidgetPositionById: the coordinate the page gave "
		 "is passed through");

	check_eq(f.journal().Arg(
			 "ISEBrowserWidgetManager::SetWidgetPositionById", 2),
		 "-1",
		 "setDockingWidgetPositionById: the coordinate the page "
		 "omitted becomes -1 rather than being left to the service");
}

//
// A value of the wrong type is the same as a missing one -- the handler
// checks GetType rather than trusting HasKey. A page sending
// {"left": "10"} gets -1, not 10.
//
void a_coordinate_of_the_wrong_type_is_treated_as_missing()
{
	Fixture f;

	f.journal().SetBool("ISEBrowserWidgetManager::SetWidgetPositionById",
			    true);

	f.Call("setDockingWidgetPositionById",
	       {"\"dock-1\"", "{\"left\":\"10\",\"top\":20}"});

	check_eq(f.journal().Arg(
			 "ISEBrowserWidgetManager::SetWidgetPositionById", 1),
		 "-1",
		 "setDockingWidgetPositionById: a string where an int was "
		 "expected is discarded, not coerced");

	check_eq(f.journal().Arg(
			 "ISEBrowserWidgetManager::SetWidgetPositionById", 2),
		 "20",
		 "setDockingWidgetPositionById: the well-typed sibling is "
		 "still used");
}

// --- the container properties, and the value type part 2 introduced ---

//
// getCurrentContainerProperties is the body that changed shape when
// GetDockBrowserWidgetInfo stopped returning an owned raw pointer. Its two
// branches differ in the SHAPE of the result, not just its contents: with
// no dock there is no "url" key at all.
//
// A live probe cannot see this. The SE.Live panel is always a dock, so the
// four-key answer never appears in a CDP run -- it is the pop-up and
// background-worker containers that get it.
//
void container_properties_omit_the_url_when_there_is_no_dock()
{
	Fixture f("popup");

	f.journal().SetBool("ISEBrowserWidgetManager::GetDockBrowserWidgetInfo",
			    false);

	const std::string reply = f.Call("getCurrentContainerProperties");

	CefRefPtr<CefValue> parsed = CefParseJSON(reply, JSON_PARSER_RFC);
	CefRefPtr<CefDictionaryValue> d = parsed ? parsed->GetDictionary()
						 : nullptr;

	check(d != nullptr, "containerProperties: the answer is a dictionary");

	if (!d)
		return;

	check_eq(d->GetSize(), 4,
		 "containerProperties: four keys without a dock -- url is "
		 "absent, not empty");

	check(!d->HasKey("url"),
	      "containerProperties: url is absent when the container is not a "
	      "dock");

	check_eq(d->GetString("id").ToString(), "probe",
		 "containerProperties: the id is the calling client's target");

	check_eq(d->GetString("dockingArea").ToString(), "none",
		 "containerProperties: a container that is not docked reports "
		 "\"none\"");

	check_eq(d->GetString("type").ToString(), "popup",
		 "containerProperties: the type is the handler's container "
		 "type");

	check_eq(f.journal().Arg(
			 "ISEBrowserWidgetManager::GetDockBrowserWidgetInfo",
			 0),
		 "probe",
		 "containerProperties: the dock is looked up by the calling "
		 "client's target");
}

void container_properties_carry_the_docks_own_area_and_url()
{
	Fixture f("dock");

	SEDockBrowserWidgetInfo info;

	info.m_id = "dock-1";
	info.m_title = "A dock";
	info.m_dockingArea = "left";
	info.m_url = "https://example.com/panel";

	f.journal().SetDockInfo(
		"ISEBrowserWidgetManager::GetDockBrowserWidgetInfo", info);
	f.journal().SetBool("ISEBrowserWidgetManager::GetDockBrowserWidgetInfo",
			    true);

	const std::string reply = f.Call("getCurrentContainerProperties");

	CefRefPtr<CefValue> parsed = CefParseJSON(reply, JSON_PARSER_RFC);
	CefRefPtr<CefDictionaryValue> d = parsed ? parsed->GetDictionary()
						 : nullptr;

	check(d != nullptr, "containerProperties: the answer is a dictionary");

	if (!d)
		return;

	check_eq(d->GetSize(), 5,
		 "containerProperties: five keys once there is a dock");

	check_eq(d->GetString("url").ToString(), "https://example.com/panel",
		 "containerProperties: the url comes off the dock");

	check_eq(d->GetString("dockingArea").ToString(), "left",
		 "containerProperties: the docking area comes off the dock");

	//
	// The id is the CLIENT's target, not the dock's own m_id, even when
	// the two could differ. Worth pinning: they are the same string in
	// production and a future change could quietly swap one for the
	// other.
	//
	check_eq(d->GetString("id").ToString(), "probe",
		 "containerProperties: the id stays the client's target "
		 "rather than the dock's stored id");
}

// --- the message bus flags, which a live probe cannot distinguish -----

//
// broadcastEvent fans one page call out to two different audiences with
// two different payloads, and the difference is invisible from a page:
//
//   local listeners    get the WHOLE argument, under the fixed event name
//                      "hostEventReceived", with browser sources excluded
//   external listeners get only data, under the caller's own event name,
//                      and only external ones
//
// The flags are the part worth a test. They moved onto ISEMessageBus as
// static constexpr in part 2, ten out-of-line definitions were deleted, and
// a wrong value there compiles perfectly and simply stops delivering. The
// live probe proved delivery happens; only this can prove to whom.
//
void broadcasting_an_event_splits_local_and_external_audiences()
{
	Fixture f;

	const std::string reply =
		f.Call("broadcastEvent",
		       {"{\"name\":\"myEvent\",\"data\":{\"k\":1}}"});

	check_eq(reply, "true", "broadcastEvent: accepted");

	check_eq(f.journal().Count(
			 "ISEMessageBus::NotifyAllLocalEventListeners"),
		 1, "broadcastEvent: local listeners are notified once");

	check_eq(f.journal().Count(
			 "ISEMessageBus::NotifyAllExternalEventListeners"),
		 1, "broadcastEvent: external listeners are notified once");

	const std::string local = "ISEMessageBus::NotifyAllLocalEventListeners";

	//
	// DEST_ALL_LOCAL (0x0000FFFF) with DEST_BROWSER_SOURCE (0x4) cleared.
	// A browser source is excluded because it would receive its own
	// broadcast back.
	//
	check_eq(f.journal().Arg(local, 0), "65531",
		 "broadcastEvent: local delivery is every local destination "
		 "except browser sources");

	check_eq(f.journal().Arg(local, 1), "web",
		 "broadcastEvent: the source is SOURCE_WEB");

	check_eq(f.journal().Arg(local, 2), "urn:streamelements:internal:probe",
		 "broadcastEvent: the source url names the calling container");

	check_eq(f.journal().Arg(local, 3), "hostEventReceived",
		 "broadcastEvent: local listeners get the fixed envelope "
		 "name, not the caller's event name");

	check_eq(f.journal().Arg(local, 4),
		 "{\"data\":{\"k\":1},\"name\":\"myEvent\"}",
		 "broadcastEvent: local listeners get the whole argument");

	const std::string external =
		"ISEMessageBus::NotifyAllExternalEventListeners";

	// DEST_ALL_EXTERNAL, 0xFFFF0000.
	check_eq(f.journal().Arg(external, 0), "4294901760",
		 "broadcastEvent: external delivery is every external "
		 "destination");

	check_eq(f.journal().Arg(external, 3), "myEvent",
		 "broadcastEvent: external listeners get the caller's own "
		 "event name");

	check_eq(f.journal().Arg(external, 4), "{\"k\":1}",
		 "broadcastEvent: external listeners get only the data");
}

void broadcasting_an_event_without_a_name_is_refused()
{
	Fixture f;

	const std::string reply =
		f.Call("broadcastEvent", {"{\"data\":{\"k\":1}}"});

	check_eq(reply, "false",
		 "broadcastEvent: an event with no name is refused");

	check_eq(f.journal().calls.size(), 0,
		 "broadcastEvent: and nothing is delivered to anybody");
}

// --- the handler's own browser widget ---------------------------------

//
// The three handlers that do not go through the container at all: they
// reach SEApiMessageHandlerBase::GetBrowserWidget(), which is the handler's
// OWN widget. That pointer is an ISEVideoCompositionViewHost, the interface
// written last in part 2 and the one that actually took this file to zero
// obs and zero Qt -- no amount of interfacing the container could have
// reached it.
//
// A handler with no widget is the normal case for a background worker, and
// the three bodies disagree about how they say so.
//
void the_overlay_handlers_answer_without_a_widget()
{
	Fixture f;

	//
	// The two disagree, and it is deliberate on one side only:
	// getVideoCompositionViewOverlayProperties opens with an explicit
	// result->SetNull(), so it answers null; removeVideoCompositionViewOverlay
	// leaves the result alone and so inherits the dispatcher's false.
	//
	check_eq(f.Call("removeVideoCompositionViewOverlay"), "false",
		 "removeVideoCompositionViewOverlay: no widget leaves the "
		 "result untouched, which is false");

	check_eq(f.Call("getVideoCompositionViewOverlayProperties"), "null",
		 "getVideoCompositionViewOverlayProperties: no widget is "
		 "answered with null");

	check_eq(f.journal().Count("ISEVideoCompositionViewHost::"
				   "RemoveVideoCompositionView"),
		 0,
		 "the overlay handlers: nothing is called when there is no "
		 "widget");
}

void the_overlay_handlers_reach_the_widget_when_there_is_one()
{
	Fixture f;

	f.handler->SetBrowserWidget(f.w.videoCompositionViewHost.get());

	check_eq(f.Call("removeVideoCompositionViewOverlay"), "true",
		 "removeVideoCompositionViewOverlay: a widget is answered "
		 "with true");

	check_eq(f.journal().Count("ISEVideoCompositionViewHost::"
				   "RemoveVideoCompositionView"),
		 1,
		 "removeVideoCompositionViewOverlay: the widget's view is "
		 "removed once");

	f.journal().SetJson("ISEVideoCompositionViewHost::"
			    "SerializeVideoCompositionView",
			    "{\"videoCompositionId\":\"vc-1\"}");

	check_eq(f.Call("getVideoCompositionViewOverlayProperties"),
		 "{\"videoCompositionId\":\"vc-1\"}",
		 "getVideoCompositionViewOverlayProperties: the widget's "
		 "serialization is what reaches the page");

	//
	// Note the shape: this one answers through an out-parameter rather
	// than a return value, so what the page gets is whatever the widget
	// wrote into the result -- and null if it wrote nothing, because the
	// body opens with result->SetNull().
	//
	f.journal().SetJson("ISEVideoCompositionViewHost::"
			    "DeserializeVideoCompositionView",
			    "true");

	check_eq(f.Call("setVideoCompositionViewOverlayProperties",
			{"{\"videoCompositionId\":\"vc-1\"}"}),
		 "true",
		 "setVideoCompositionViewOverlayProperties: the widget's "
		 "answer reaches the page");

	check_eq(f.journal().Arg("ISEVideoCompositionViewHost::"
				 "DeserializeVideoCompositionView",
				 0),
		 "{\"videoCompositionId\":\"vc-1\"}",
		 "setVideoCompositionViewOverlayProperties: the description "
		 "is passed through unchanged");
}

// --- hotkeys: the behaviour 9dbf8d7 changed ---------------------------

//
// A rejected binding answers null. It used to answer 0, which is a VALID
// obs_hotkey_id -- the page was told a binding with id 0 had been
// registered when nothing had. The fix was to make the service return a
// signed id and the handler treat anything negative as failure, so this
// test is the pin on both halves.
//
void a_rejected_hotkey_binding_answers_null_rather_than_zero()
{
	{
		Fixture f;

		f.journal().SetInt("ISEHotkeyManager::"
				   "DeserializeSingleHotkeyBinding",
				   -1);

		check_eq(f.Call("addHotkeyBinding", {"{\"name\":\"\"}"}),
			 "null",
			 "addHotkeyBinding: a rejected binding is null, not 0 "
			 "-- 0 is a valid hotkey id");
	}

	{
		Fixture f;

		//
		// Zero is the boundary and the whole reason the sign matters:
		// it has to come back as an id.
		//
		f.journal().SetInt("ISEHotkeyManager::"
				   "DeserializeSingleHotkeyBinding",
				   0);

		check_eq(f.Call("addHotkeyBinding", {"{\"name\":\"a\"}"}), "0",
			 "addHotkeyBinding: id 0 is a real binding and must "
			 "reach the page as 0");
	}

	{
		Fixture f;

		f.journal().SetInt("ISEHotkeyManager::"
				   "DeserializeSingleHotkeyBinding",
				   66);

		check_eq(f.Call("addHotkeyBinding", {"{\"name\":\"a\"}"}), "66",
			 "addHotkeyBinding: a real id reaches the page");
	}

	{
		Fixture f;

		check_eq(f.Call("addHotkeyBinding"), "null",
			 "addHotkeyBinding: no arguments is null");

		check_eq(f.journal().calls.size(), 0,
			 "addHotkeyBinding: and the hotkey manager is not "
			 "asked");
	}
}

// --- argument defaulting ----------------------------------------------

//
// Called with nothing, getAllScenes does not skip the argument -- it builds
// a CefValue holding null and passes that. So the service always receives
// an argument, and "no filter" is spelled null rather than absent. Twenty
// or so bodies share this shape.
//
void a_body_with_no_arguments_still_passes_a_null_value()
{
	Fixture f;

	f.journal().SetJson("ISEObsSceneManager::SerializeObsScenes",
			    "[{\"id\":\"scene-1\"}]");

	check_eq(f.Call("getAllScenes"), "[{\"id\":\"scene-1\"}]",
		 "getAllScenes: the scene manager's serialization is what "
		 "reaches the page");

	check_eq(f.journal().Count("ISEObsSceneManager::SerializeObsScenes"), 1,
		 "getAllScenes: the scene manager is asked once");

	check_eq(f.journal().Arg("ISEObsSceneManager::SerializeObsScenes", 0),
		 "null",
		 "getAllScenes: no arguments becomes an explicit null, not a "
		 "missing argument");
}

// --- a handler that can leave a page waiting for ever -----------------

//
// showNonModalDialog is the file's only API_HANDLER_END_ASYNC, which means
// nothing completes the call for it -- the body must do so on every path.
// It does not: with no arguments it falls straight through, no thread is
// started, and complete_callback is never called. The page's promise never
// settles.
//
// Recorded rather than fixed, in the idiom the infrastructure test already
// uses for InvokeApiCallHandlerAsync's silent path: a test that will start
// failing the moment somebody fixes it, with a comment saying why that is
// the good outcome. It is also why the sweep above has to skip this one
// name.
//
void an_async_dialog_handler_with_no_arguments_never_answers()
{
	Fixture f;

	check_eq(f.Call("showNonModalDialog"), "<no reply>",
		 "showNonModalDialog: with no arguments the call is never "
		 "completed, so the page's promise never settles. If this "
		 "test fails because a reply arrived, the defect has been "
		 "fixed -- update the expectation and delete this comment");

	check_eq(f.journal().calls.size(), 0,
		 "showNonModalDialog: and the dialog manager is never asked");
}

// --- the run ----------------------------------------------------------

struct Test {
	const char *name;
	void (*fn)();
};

const Test tests[] = {
	{"the surface is exactly what the arithmetic says",
	 the_surface_is_exactly_what_the_arithmetic_says},
	{"every handler answers exactly once",
	 every_handler_answers_exactly_once},
	{"a body that does nothing answers false rather than null",
	 a_body_that_does_nothing_answers_false_rather_than_null},
	{"a wrongly typed argument is refused rather than fatal",
	 a_wrongly_typed_argument_is_refused_rather_than_fatal},
	{"adding a dock persists only once the dock exists",
	 adding_a_dock_persists_only_once_the_dock_exists},
	{"adding a dock with no arguments touches nothing",
	 adding_a_dock_with_no_arguments_touches_nothing},
	{"removing docks reports success without checking anything was "
	 "removed",
	 removing_docks_reports_success_without_checking_anything_was_removed},
	{"removing docks passes every id and persists once",
	 removing_docks_passes_every_id_and_persists_once},
	{"removing docks with something that is not a list touches nothing",
	 removing_docks_with_something_that_is_not_a_list_touches_nothing},
	{"a widget change that did not take effect is not persisted",
	 a_widget_change_that_did_not_take_effect_is_not_persisted},
	{"a coordinate the page omitted is passed through as minus one",
	 a_coordinate_the_page_omitted_is_passed_through_as_minus_one},
	{"a coordinate of the wrong type is treated as missing",
	 a_coordinate_of_the_wrong_type_is_treated_as_missing},
	{"container properties omit the url when there is no dock",
	 container_properties_omit_the_url_when_there_is_no_dock},
	{"container properties carry the dock's own area and url",
	 container_properties_carry_the_docks_own_area_and_url},
	{"broadcasting an event splits local and external audiences",
	 broadcasting_an_event_splits_local_and_external_audiences},
	{"broadcasting an event without a name is refused",
	 broadcasting_an_event_without_a_name_is_refused},
	{"the overlay handlers answer without a widget",
	 the_overlay_handlers_answer_without_a_widget},
	{"the overlay handlers reach the widget when there is one",
	 the_overlay_handlers_reach_the_widget_when_there_is_one},
	{"a rejected hotkey binding answers null rather than zero",
	 a_rejected_hotkey_binding_answers_null_rather_than_zero},
	{"a body with no arguments still passes a null value",
	 a_body_with_no_arguments_still_passes_a_null_value},
	{"an async dialog handler with no arguments never answers",
	 an_async_dialog_handler_with_no_arguments_never_answers},
};

//
// The local CefValue model answers a type mismatch with a bare `throw;`,
// which outside a catch block is std::terminate. On Windows the Debug CRT
// then raises a message box, and a test executable waiting on a dialog
// hangs CI rather than failing it. So terminate is routed to a readable
// line and a non-zero exit instead.
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
