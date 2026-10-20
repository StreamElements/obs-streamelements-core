//
// Behavioural tests for the StreamElements overlay URL helpers.
//
// Pure: one std::regex and one string concatenation, so the production .cpp
// compiles beside the test with nothing else linked.
//
// Neither function has a caller yet. They are tested rather than deleted
// because they describe an external system's URL scheme rather than
// anything internal, and because what they actually accept is looser than
// the pattern reads -- which is worth having written down before the first
// caller arrives and assumes otherwise.
//

#include "streamelements/SEUtils/SEOverlayUrlUtils.hpp"

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

static bool parses(const char *url, std::string &overlayId,
		   std::string &accountId)
{
	overlayId.clear();
	accountId.clear();

	return ParseStreamElementsOverlayURL(url, overlayId, accountId);
}

/* ---------------------------------------------------------------- parsing */

static void test_a_well_formed_url_parses()
{
	std::string overlayId, accountId;

	check(parses("https://streamelements.com/overlay/5f3a/acct-9",
		     overlayId, accountId),
	      "a well-formed overlay URL did not parse");

	check(overlayId == "5f3a", "the overlay id is wrong");
	check(accountId == "acct-9", "the account id is wrong");
}

static void test_the_shapes_that_must_not_parse()
{
	const char *rejected[] = {
		"",
		"https://streamelements.com/overlay/5f3a",
		"https://streamelements.com/overlay/5f3a/acct-9/",
		"https://streamelements.com/overlay/5f3a/acct-9/extra",
		"https://streamelements.com/overlay//acct-9",
		"https://streamelements.com/overlay/5f3a/",
		"http://streamelements.com/overlay/5f3a/acct-9",
		"HTTPS://streamelements.com/overlay/5f3a/acct-9",
		"https://streamelements.com/overlays/5f3a/acct-9",
		"https://evil.example/overlay/5f3a/acct-9",
		" https://streamelements.com/overlay/5f3a/acct-9",
	};

	std::string overlayId, accountId;

	for (const char *url : rejected) {
		char msg[256];
		std::snprintf(msg, sizeof(msg),
			      "this parsed as an overlay URL and must not: '%s'",
			      url);

		check(!parses(url, overlayId, accountId), msg);
	}
}

static void test_a_failed_parse_leaves_the_outputs_alone()
{
	std::string overlayId = "untouched-overlay";
	std::string accountId = "untouched-account";

	check(!ParseStreamElementsOverlayURL("not a url", overlayId,
					     accountId),
	      "'not a url' parsed");

	//
	// The implementation writes the out-parameters only on a match, so a
	// caller that ignores the return value keeps whatever it had rather
	// than getting empty strings. Pinned because either behaviour is
	// defensible and a caller will depend on one of them.
	//
	check(overlayId == "untouched-overlay" &&
		      accountId == "untouched-account",
	      "a failed parse overwrote the out-parameters");
}

/* -------------------------------------------- what it accepts but shouldn't */

static void test_the_host_is_matched_loosely()
{
	std::string overlayId, accountId;

	//
	// The dot in "streamelements.com" is unescaped in the pattern, so it
	// matches any character. This is not a URL validator: anything that
	// uses it to decide whether a URL is ours would accept these.
	//
	check(parses("https://streamelementsXcom/overlay/5f3a/acct-9",
		     overlayId, accountId),
	      "the unescaped dot in the host pattern no longer matches any "
	      "character -- if the pattern was tightened, this expectation "
	      "should be inverted rather than deleted");

	check(parses("https://streamelements-com/overlay/5f3a/acct-9",
		     overlayId, accountId),
	      "same, with a hyphen where the dot is");
}

static void test_the_segments_swallow_a_query_string()
{
	std::string overlayId, accountId;

	//
	// [^/]+ stops at a slash and nothing else, so a query string or a
	// fragment lands inside the account id rather than being separated
	// off. A caller handing one of these straight to an API would be
	// sending the query along with it.
	//
	check(parses("https://streamelements.com/overlay/5f3a/acct-9?theme=dark",
		     overlayId, accountId),
	      "a URL with a query string did not parse");

	check(accountId == "acct-9?theme=dark",
	      "the query string is no longer swallowed by the account id -- "
	      "if that was fixed, this expectation should be updated rather "
	      "than deleted");

	check(parses("https://streamelements.com/overlay/5f3a/acct-9#top",
		     overlayId, accountId),
	      "a URL with a fragment did not parse");

	check(accountId == "acct-9#top",
	      "the fragment is no longer swallowed by the account id");

	//
	// Trailing whitespace goes the same way, for the same reason. A
	// leading space does not: the pattern is anchored at ^https, so
	// that one fails to parse instead of being trimmed.
	//
	check(parses("https://streamelements.com/overlay/5f3a/acct-9 ",
		     overlayId, accountId),
	      "a URL with a trailing space did not parse");

	check(accountId == "acct-9 ",
	      "the trailing space is no longer part of the account id");
}

/* -------------------------------------------------------------- composing */

static void test_the_editor_url()
{
	check(GetStreamElementsOverlayEditorURL("5f3a", "acct-9") ==
		      "https://streamelements.com/overlay/5f3a/editor",
	      "the editor URL is not the overlay path with /editor on it");

	//
	// The account id is a parameter the body never reads. Pinned so the
	// next reader does not have to work out whether that is deliberate:
	// as it stands, two different accounts give the same URL.
	//
	check(GetStreamElementsOverlayEditorURL("5f3a", "acct-9") ==
		      GetStreamElementsOverlayEditorURL("5f3a", "other"),
	      "the account id now changes the editor URL, so the parameter "
	      "has started being used");
}

static void test_parse_then_compose()
{
	std::string overlayId, accountId;

	check(parses("https://streamelements.com/overlay/5f3a/acct-9",
		     overlayId, accountId),
	      "the round-trip input did not parse");

	check(GetStreamElementsOverlayEditorURL(overlayId, accountId) ==
		      "https://streamelements.com/overlay/5f3a/editor",
	      "parsing a URL and composing the editor URL from its parts did "
	      "not give the editor URL for the same overlay");
}

int main()
{
	test_a_well_formed_url_parses();
	test_the_shapes_that_must_not_parse();
	test_a_failed_parse_leaves_the_outputs_alone();
	test_the_host_is_matched_loosely();
	test_the_segments_swallow_a_query_string();
	test_the_editor_url();
	test_parse_then_compose();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);

		return 1;
	}

	std::printf("all overlay URL tests passed\n");

	return 0;
}
