//
// Behavioural tests for the WYVRN configuration parser.
//
// The parser is deliberately free of libobs and Qt, so this compiles and runs
// without an obs-studio tree - which is the whole reason the scan was factored
// that way. Everything asserted here was established by reading the 146 config
// files a stock Razer install puts on disk; the fixtures below are reduced from
// real ones.
//
// The failure this suite exists to prevent is not a crash. It is a scan that
// looks like it worked: wrong-cased filenames skipped, 124 harmless files
// reported as errors, or a malformed entry taking the whole enumeration with it.
//

#include "streamelements/StreamElementsRazerWyvrnConfig.hpp"

#include <cstdio>
#include <string>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

/* --------------------------------------------------------------- fixtures */

// Reduced from `007 First Light/Wyvrn.config`, keeping the shape verbatim:
// several targets on one haptic event, Gain written as an int, Loop as an int.
static const char *kRealConfig = R"JSON({
  "ExternalCommands": [
    {
      "External_Command_ID": "Interact_Environment",
      "Chroma_Events": [
        { "Chroma_Effect": "Interact_Environment", "Interrupt": false }
      ],
      "Haptic_Events": [
        {
          "Haptic_Effect": "Interact_Environment",
          "Loop": 1,
          "Mixing": "Merge",
          "Priority": "High",
          "Targeting": [
            { "Gain": 1, "Spatialization": "Global", "Target": "Chest" },
            { "Gain": 1, "Spatialization": "Global", "Target": "Hand" },
            { "Gain": 1, "Spatialization": "Global", "Target": "Waist" }
          ]
        }
      ]
    },
    {
      "External_Command_ID": "Crouch_On",
      "Chroma_Events": [
        { "Chroma_Effect": "Crouch_On", "Interrupt": true }
      ],
      "Haptic_Events": []
    }
  ]
})JSON";

// The other 124 of 146 files: a completely different schema, no events at all.
static const char *kAudioProfileConfig = R"JSON({
  "audioToHapticProfile": {
    "associatedGuids": ["ec16c25f-ce88-36b5-3dc8-6d8de6496b26"],
    "profile": "Controlled",
    "bands": [ { "minFrequency": 30, "maxFrequency": 180, "gain": 0.68 } ]
  },
  "muteOnLoad": false
})JSON";

// FallbackCommands hold regular expressions, not names.
static const char *kFallbackConfig = R"JSON({
  "FallbackCommands": [
    {
      "External_Command_ID": ".*Damage.*",
      "Haptic_Events": [
        { "Haptic_Effect": "Generic_Hit",
          "Targeting": [ { "Gain": 0.7, "Spatialization": "Left", "Target": "Chest" } ] }
      ]
    }
  ]
})JSON";

/* ------------------------------------------------------------------ tests */

static void test_filename_matching_is_case_insensitive()
{
	// All three casings are on disk: wyvrn.config x121, Wyvrn.config x12,
	// WYVRN.config x13. A case-sensitive match finds 13 of 146 and loses the
	// rest without saying anything.
	check(IsRazerWyvrnConfigFileName("wyvrn.config"), "lowercase accepted");
	check(IsRazerWyvrnConfigFileName("Wyvrn.config"),
	      "mixed case accepted");
	check(IsRazerWyvrnConfigFileName("WYVRN.config"), "uppercase accepted");
	check(IsRazerWyvrnConfigFileName("WyVrN.CoNfIg"),
	      "any casing accepted");

	check(!IsRazerWyvrnConfigFileName("wyvrn.json"),
	      "a different extension is not a config");
	check(!IsRazerWyvrnConfigFileName("notwyvrn.config"),
	      "a longer name is not a config");
	check(!IsRazerWyvrnConfigFileName(""), "an empty name is not a config");
}

static void test_real_config_parses_completely()
{
	auto events = ParseRazerWyvrnConfig(kRealConfig, "007 First Light");

	check(events.size() == 2, "both commands are returned");
	if (events.size() != 2)
		return;

	const auto &first = events[0];
	check(first.id == "Interact_Environment",
	      "id is the External_Command_ID");
	check(first.source == "007 First Light", "source is recorded");
	check(first.kind == "exact", "ExternalCommands entries are exact");

	check(first.chroma.size() == 1, "the chroma component is parsed");
	check(first.chroma[0].effect == "Interact_Environment",
	      "Chroma_Effect is carried");
	check(first.chroma[0].interrupt == false, "Interrupt false is carried");

	check(first.haptics.size() == 1, "the haptic component is parsed");
	check(first.haptics[0].effect == "Interact_Environment",
	      "Haptic_Effect is carried");
	check(first.haptics[0].loop == 1, "Loop is carried");
	check(first.haptics[0].mixing == "merge", "Mixing is carried");
	check(first.haptics[0].priority == "high", "Priority is carried");

	// Targeting is the half that says *where* on the body an event lands,
	// and one event routinely drives several regions.
	check(first.haptics[0].targeting.size() == 3,
	      "all three body targets are parsed");
	if (first.haptics[0].targeting.size() == 3) {
		check(first.haptics[0].targeting[0].target == "chest",
		      "first target is Chest");
		check(first.haptics[0].targeting[1].target == "hand",
		      "second target is Hand");
		check(first.haptics[0].targeting[2].target == "waist",
		      "third target is Waist");
		check(first.haptics[0].targeting[0].spatialization == "global",
		      "spatialization is carried");
		// Gain is written as `1`, not `1.0`, throughout the real files.
		check(first.haptics[0].targeting[0].gain == 1.0,
		      "an integer Gain is read as a double");
	}

	check(events[1].chroma.size() == 1 && events[1].chroma[0].interrupt,
	      "Interrupt true is carried");
	check(events[1].haptics.empty(),
	      "an empty Haptic_Events list yields no components");
}

static void test_event_less_config_is_normal_not_an_error()
{
	// 124 of the 146 files on a stock machine look like this. Treating them
	// as failures would put 124 diagnostics in the log on every scan.
	auto events =
		ParseRazerWyvrnConfig(kAudioProfileConfig, "Apex Legends");

	check(events.empty(), "an audio-to-haptic profile declares no events");
}

static void test_fallback_commands_are_tagged_distinctly()
{
	auto events = ParseRazerWyvrnConfig(kFallbackConfig, "Some Game");

	check(events.size() == 1, "the fallback entry is returned");
	if (events.empty())
		return;

	// The id is a regex, so a caller must never present it as fireable.
	check(events[0].kind == "fallbackPattern",
	      "FallbackCommands entries are tagged as patterns, not exact names");
	check(events[0].id == ".*Damage.*", "the pattern is carried verbatim");
	check(CountRazerWyvrnEventsOfKind(events, "exact") == 0,
	      "a fallback entry is not counted as exact");
	check(CountRazerWyvrnEventsOfKind(events, "fallbackPattern") == 1,
	      "the fallback entry is counted as a pattern");

	check(events[0].haptics.size() == 1 &&
		      events[0].haptics[0].targeting.size() == 1,
	      "targeting inside a fallback entry is parsed");
	if (!events[0].haptics.empty() &&
	    !events[0].haptics[0].targeting.empty()) {
		check(events[0].haptics[0].targeting[0].spatialization ==
			      "left",
		      "a non-Global spatialization is carried");
		check(events[0].haptics[0].targeting[0].gain == 0.7,
		      "a fractional Gain is carried");
	}
}

static void test_malformed_input_yields_nothing_rather_than_throwing()
{
	// One unreadable file among 146 must not abort the scan, so every one of
	// these is an empty result rather than an exception.
	check(ParseRazerWyvrnConfig("", "x").empty(), "empty input");
	check(ParseRazerWyvrnConfig("{not json", "x").empty(), "invalid JSON");
	check(ParseRazerWyvrnConfig("[]", "x").empty(),
	      "a top-level array is not a config");
	check(ParseRazerWyvrnConfig("null", "x").empty(), "a null document");
	check(ParseRazerWyvrnConfig("{}", "x").empty(), "an empty object");
	check(ParseRazerWyvrnConfig(R"({"ExternalCommands": {}})", "x").empty(),
	      "ExternalCommands of the wrong type");
	check(ParseRazerWyvrnConfig(R"({"ExternalCommands": [1, 2, 3]})", "x")
		      .empty(),
	      "non-object entries are skipped");
}

static void test_entries_without_an_id_are_dropped()
{
	// There is nothing to name, so the entry cannot be used whatever else it
	// carries. Returning it would put a blank row in a 4,000-entry list.
	auto events = ParseRazerWyvrnConfig(
		R"({"ExternalCommands":[
		     {"Chroma_Events":[{"Chroma_Effect":"X"}]},
		     {"External_Command_ID":"","Chroma_Events":[]},
		     {"External_Command_ID":"Good"}]})",
		"x");

	check(events.size() == 1, "only the entry with an id survives");
	if (!events.empty())
		check(events[0].id == "Good",
		      "the surviving entry is the named one");
}

static void test_partial_entries_do_not_lose_their_siblings()
{
	// A component missing its effect name is unusable, but the event around
	// it and the events after it are still fine. Dropping the whole file on
	// one bad component is the failure mode worth guarding.
	auto events = ParseRazerWyvrnConfig(
		R"({"ExternalCommands":[
		     {"External_Command_ID":"A",
		      "Chroma_Events":[{"Interrupt":true},{"Chroma_Effect":"Real"}],
		      "Haptic_Events":[{"Targeting":[{"Target":"Head"}]},
		                       {"Haptic_Effect":"RealHaptic"}]},
		     {"External_Command_ID":"B"}]})",
		"x");

	check(events.size() == 2, "both events survive a malformed component");
	if (events.size() != 2)
		return;

	check(events[0].chroma.size() == 1 &&
		      events[0].chroma[0].effect == "Real",
	      "the nameless chroma component is dropped, the real one kept");
	check(events[0].haptics.size() == 1 &&
		      events[0].haptics[0].effect == "RealHaptic",
	      "the nameless haptic component is dropped, the real one kept");
}

static void test_targeting_oddities_in_the_shipped_data()
{
	// The real configs contain a lowercase `waist` and the misspelling
	// `Wasit`. The parser passes targets through as written rather than
	// normalising: a consumer that wants to map them to body regions should
	// see what the data actually says.
	auto events = ParseRazerWyvrnConfig(
		R"({"ExternalCommands":[{"External_Command_ID":"T",
		     "Haptic_Events":[{"Haptic_Effect":"E","Targeting":[
		       {"Target":"waist"},{"Target":"Wasit"},
		       {"Target":"All","Gain":1.0},{"Spatialization":"Left"}]}]}]})",
		"x");

	check(events.size() == 1, "the event parses");
	if (events.empty() || events[0].haptics.empty())
		return;

	const auto &targeting = events[0].haptics[0].targeting;

	// Four entries in, one has no Target and is dropped.
	check(targeting.size() == 3, "a target-less entry is dropped");
	if (targeting.size() != 3)
		return;

	// Both spellings occur in the shipped data. Normalisation folds the case
	// split onto one value, but leaves the misspelling as its own value --
	// correcting it would hide a data problem behind a value the caller
	// cannot tell apart from a real one.
	check(targeting[0].target == "waist",
	      "a lowercase target survives normalisation unchanged");
	check(targeting[1].target == "wasit",
	      "the misspelling is camelCased but not corrected");
	check(targeting[2].spatialization == "global",
	      "a missing Spatialization defaults to Global");
}

static void test_enum_values_are_camel_cased()
{
	// The rule is "lowercase the first character, leave the rest", which
	// covers every value the shipped configurations contain.
	check(RazerWyvrnCamelCaseEnum("Chest") == "chest",
	      "a single word becomes lowercase");
	check(RazerWyvrnCamelCaseEnum("VeryHigh") == "veryHigh",
	      "two words become camelCase");
	check(RazerWyvrnCamelCaseEnum("KeyboardExtended") == "keyboardExtended",
	      "the extended keyboard suffix becomes camelCase");
	check(RazerWyvrnCamelCaseEnum("ChromaLink") == "chromaLink",
	      "ChromaLink becomes chromaLink");

	// "Waist" and "waist" both occur in the wild; they must land on one
	// value so a caller can switch on it.
	check(RazerWyvrnCamelCaseEnum("Waist") ==
		      RazerWyvrnCamelCaseEnum("waist"),
	      "a case split in the source data folds into one value");

	// A genuine misspelling stays distinct -- correcting it here would
	// hide a data problem behind a value the caller cannot tell apart
	// from a real one.
	check(RazerWyvrnCamelCaseEnum("Wasit") !=
		      RazerWyvrnCamelCaseEnum("Waist"),
	      "a misspelling is not silently mapped onto the correct value");

	check(RazerWyvrnCamelCaseEnum("").empty(), "empty stays empty");
}
int main()
{
	test_filename_matching_is_case_insensitive();
	test_real_config_parses_completely();
	test_event_less_config_is_normal_not_an_error();
	test_fallback_commands_are_tagged_distinctly();
	test_malformed_input_yields_nothing_rather_than_throwing();
	test_entries_without_an_id_are_dropped();
	test_partial_entries_do_not_lose_their_siblings();
	test_targeting_oddities_in_the_shipped_data();
	test_enum_values_are_camel_cased();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::puts("test_razer_wyvrn_config: all checks passed");
	return 0;
}
