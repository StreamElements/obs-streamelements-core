//
// Behavioural tests for profile-name resolution.
//
// The bug these exist for was found live: getAllProfiles reported
// {"id":"TWITCH_Warlock","name":"TWITCH Warlock"}, and handing that id back
// to setCurrentProfile answered `false` -- for a profile that exists, with
// no error anywhere. The directory name and the OBS name are different
// strings and the handler compared only the second.
//
// The production .cpp is compiled here beside the test; the header is
// theirs. Nothing is mirrored, so the sanitisation rule asserted below is
// the rule the plug-in actually applies.
//

#include "streamelements/SEUtils/SEProfileNameUtils.hpp"

#include <cstdio>
#include <string>
#include <vector>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

/* ------------------------------------------------- the sanitisation rule */

static void test_directory_name_replaces_spaces()
{
	// Measured on this machine's live profile directory list.
	check(SEProfileDirectoryName("TWITCH Warlock") == "TWITCH_Warlock",
	      "a space becomes an underscore -- 'TWITCH Warlock' is TWITCH_Warlock on disk");
	check(SEProfileDirectoryName("STREAMING PS4") == "STREAMING_PS4",
	      "second real-world case, from the same directory listing");
	check(SEProfileDirectoryName("Other") == "Other",
	      "a name that needs no change is returned unchanged");
	check(SEProfileDirectoryName("Twitch Auth") == "Twitch_Auth",
	      "case is preserved; only the separator changes");
}

static void test_directory_name_replaces_filesystem_characters()
{
	check(SEProfileDirectoryName("a/b") == "a_b", "a forward slash");
	check(SEProfileDirectoryName("a\\b") == "a_b", "a backslash");
	check(SEProfileDirectoryName("a:b") == "a_b", "a colon");
	check(SEProfileDirectoryName("a*b?c") == "a_b_c",
	      "wildcard characters, each replaced");
	check(SEProfileDirectoryName("a<b>c") == "a_b_c",
	      "angle brackets, both replaced");
	check(SEProfileDirectoryName("a: b|c") == "a__b_c",
	      "colon, space and pipe -- three replacements in one name");
}

static void test_directory_name_handles_empty()
{
	check(SEProfileDirectoryName("").empty(),
	      "empty in, empty out -- no invented underscore");
}

/* ------------------------------------------------------------- matching */

static void test_both_spellings_match()
{
	check(SEProfileMatches("TWITCH Warlock", "TWITCH Warlock"),
	      "the name matches the name");
	check(SEProfileMatches("TWITCH Warlock", "TWITCH_Warlock"),
	      "the directory matches -- this is the bug");
	check(SEProfileMatches("TWITCH Warlock", "twitch_warlock"),
	      "case-insensitively, because a page is not the authority on Windows capitals");
	check(SEProfileMatches("TWITCH Warlock", "twitch warlock"),
	      "a lowercased name still matches");
}

static void test_mismatches_are_refused()
{
	check(!SEProfileMatches("TWITCH Warlock", ""),
	      "an empty request is not a profile");
	check(!SEProfileMatches("", "TWITCH Warlock"),
	      "an empty candidate is not a profile");
	check(!SEProfileMatches("TWITCH Warlock", "Other"),
	      "a different profile does not match");
	check(!SEProfileMatches("TWITCH Warlock", "TWITCH_Warlock "),
	      "a trailing space is a different request, not a near miss");
	check(!SEProfileMatches("TWITCH Warlock", "TWITCHWarlock"),
	      "removing the space is not the same as replacing it");
}

/* ------------------------------------------------------------ resolving */

static const std::vector<std::string> kRealProfiles = {
	"HauntersHangout",
	"Main",
	"Other",
	"Profile 2",
	"Profile 3",
	"RESTREAMIO",
	"STREAMING PS4",
	"TWITCH Warlock",
	"Twitch Auth",
	"Untitled",
	"\xD1\x80\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9",
};

static void test_resolve_accepts_the_id_it_published()
{
	// The exact round trip that failed live.
	check(SEResolveProfileName(kRealProfiles, "TWITCH_Warlock") ==
		      "TWITCH Warlock",
	      "the id getAllProfiles published resolves to the name OBS wants");
	check(SEResolveProfileName(kRealProfiles, "STREAMING_PS4") ==
		      "STREAMING PS4",
	      "same for the other space-containing profile");
	check(SEResolveProfileName(kRealProfiles, "Other") == "Other",
	      "a profile whose id equals its name still resolves");
	check(SEResolveProfileName(kRealProfiles, "\xD1\x80\xD1\x83\xD1\x81"
						  "\xD1\x81\xD0\xBA\xD0\xB8"
						  "\xD0\xB9") ==
		      "\xD1\x80\xD1\x83\xD1\x81\xD1\x81\xD0\xBA\xD0\xB8\xD0\xB9",
	      "a non-ASCII name round-trips byte for byte");
}

static void test_resolve_prefers_the_name()
{
	// A profile actually called "TWITCH_Warlock" beside "TWITCH Warlock".
	// Asking for the underscored name must return the underscored one --
	// otherwise the answer depends on the order OBS happened to list
	// them in, which is the kind of bug that only shows up on one
	// machine.
	const std::vector<std::string> both = {"TWITCH Warlock",
					       "TWITCH_Warlock"};

	check(SEResolveProfileName(both, "TWITCH_Warlock") == "TWITCH_Warlock",
	      "an exact name beats another profile's directory spelling");
	check(SEResolveProfileName(both, "TWITCH Warlock") == "TWITCH Warlock",
	      "and the spaced spelling still reaches the spaced profile");
}

static void test_resolve_refuses_unknown_and_empty()
{
	check(SEResolveProfileName(kRealProfiles, "NoSuchProfile").empty(),
	      "an unknown id resolves to nothing, so the caller answers false");
	check(SEResolveProfileName(kRealProfiles, "").empty(),
	      "an empty request resolves to nothing");
	check(SEResolveProfileName(std::vector<std::string>(), "Other").empty(),
	      "an empty list resolves to nothing");
	check(SEResolveProfileName(kRealProfiles, "new canvas 1").empty(),
	      "one of this machine's canvas names, which is not a profile");
}

int main()
{
	test_directory_name_replaces_spaces();
	test_directory_name_replaces_filesystem_characters();
	test_directory_name_handles_empty();

	test_both_spellings_match();
	test_mismatches_are_refused();

	test_resolve_accepts_the_id_it_published();
	test_resolve_prefers_the_name();
	test_resolve_refuses_unknown_and_empty();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all profile-name checks passed\n");
	return 0;
}
