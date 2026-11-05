//
// Behavioural tests for the pure string helpers.
//
// These are the first tests over code that was inside SEUtils.cpp. Nothing
// here is new or mirrored: the test includes the production header and the
// build compiles the production .cpp beside it. That is only possible because
// the split gave this cluster a header of its own -- SEUtils.hpp drags in
// obs.h, obs-module.h, curl and eight Qt headers, so no test could include it
// at all, and the ~50 pure symbols behind it were unreachable however simple
// they were.
//
// Everything asserted below was read off the implementations rather than
// assumed, including the places where they are blunter than their names
// suggest.
//

#include "streamelements/SEUtils/SEStringUtils.hpp"

#include <cstdio>
#include <map>
#include <string>

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

/* ------------------------------------------------------------- encoding */

static void test_utf8_round_trip()
{
	const char *cases[] = {
		"",
		"plain ascii",
		"\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D", // shalom, Hebrew
		"\xD0\xBF\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82", // privet
		"\xE2\x9C\x93 \xE2\x80\x94 \xC2\xA9", // check, em dash, (c)
		"\xF0\x9F\x8E\xA5", // movie camera: a surrogate pair on Windows
	};

	for (const char *utf8 : cases) {
		const std::string original = utf8;
		const std::string back = wstring_to_utf8(
			utf8_to_wstring(original));

		check(back == original,
		      "utf8 -> wide -> utf8 did not round-trip");
	}

	//
	// Both directions short-circuit on empty rather than running the
	// converter, so this is the one input where the implementation
	// branches.
	//
	check(utf8_to_wstring("").empty(), "empty utf8 must give empty wide");
	check(wstring_to_utf8(std::wstring()).empty(),
	      "empty wide must give empty utf8");

	//
	// One case pinned exactly, so a change of encoding (rather than a
	// change of round-tripping) is visible: U+2713 is three UTF-8 bytes
	// and one code unit on every platform we build for.
	//
	const std::wstring wide = utf8_to_wstring("\xE2\x9C\x93");
	check(wide.size() == 1 && wide[0] == 0x2713,
	      "U+2713 must decode to a single code unit of 0x2713");
}

/* ------------------------------------------------------------ guid ids */

static void test_clean_guid_string_only_removes_hyphens()
{
	check(clean_guid_string("8f2c1d4e-0a7b-4c63-9f11-2b7d5e6a8c90") ==
		      "8f2c1d4e0a7b4c639f112b7d5e6a8c90",
	      "clean_guid_string must strip every hyphen");

	//
	// It is a hyphen removal and nothing else: the braces a registry
	// GUID comes wrapped in survive, and so does any other character.
	//
	check(clean_guid_string("{8f2c1d4e-0a7b}") == "{8f2c1d4e0a7b}",
	      "clean_guid_string must leave braces alone");
	check(clean_guid_string("no hyphens here") == "no hyphens here",
	      "clean_guid_string must not touch anything else");
	check(clean_guid_string("").empty(),
	      "clean_guid_string of an empty string is empty");
}

/* ---------------------------------------------------------- query string */

static void test_parse_query_string()
{
	{
		std::map<std::string, std::string> r;
		check(ParseQueryString("a=1&b=2&c=3", r), "must return true");
		check(r.size() == 3 && r["a"] == "1" && r["b"] == "2" &&
			      r["c"] == "3",
		      "three pairs must parse");
	}
	{
		//
		// A key with no '=' is kept with an empty value, which is what
		// the overlay URLs rely on for flag-style parameters.
		//
		std::map<std::string, std::string> r;
		ParseQueryString("flag&a=1", r);
		check(r.size() == 2 && r.count("flag") == 1 &&
			      r["flag"].empty() && r["a"] == "1",
		      "a valueless key must survive with an empty value");
	}
	{
		// Split at the FIRST '=' only, so a value may contain more.
		std::map<std::string, std::string> r;
		ParseQueryString("a=b=c", r);
		check(r.size() == 1 && r["a"] == "b=c",
		      "only the first '=' separates key from value");
	}
	{
		// Last occurrence wins; there is no multi-value support.
		std::map<std::string, std::string> r;
		ParseQueryString("a=1&a=2", r);
		check(r.size() == 1 && r["a"] == "2",
		      "a repeated key must take its last value");
	}
	{
		std::map<std::string, std::string> r;
		check(ParseQueryString("", r), "empty input must return true");
		check(r.empty(), "empty input must add nothing");
	}
	{
		//
		// Pinned because it is surprising rather than because it is
		// wanted: separators with nothing between them produce an
		// empty key, and the function has no notion of percent
		// decoding at all.
		//
		std::map<std::string, std::string> r;
		ParseQueryString("a=1&&b=2", r);
		check(r.count("") == 1,
		      "an empty segment yields an empty key");

		std::map<std::string, std::string> r2;
		ParseQueryString("a=%20b", r2);
		check(r2["a"] == "%20b",
		      "values are not percent-decoded");
	}
}

/* -------------------------------------------------------- pointer ids */

static void test_pointer_id_round_trip()
{
	int anchor = 0;
	const void *ptr = &anchor;

	const std::string id = GetIdFromPointer(ptr);

	check(id.compare(0, 4, "ptr(") == 0 && id.back() == ')',
	      "a pointer id must be spelled ptr(...)");
	check(GetPointerFromId(id.c_str()) == ptr,
	      "a pointer must survive the round-trip through its id");

	check(GetPointerFromId("not a pointer id") == nullptr,
	      "an unparsable id must give nullptr, not a wild pointer");
	check(GetPointerFromId("") == nullptr,
	      "an empty id must give nullptr");

	check(GetIdFromPointer(nullptr) !=
		      GetIdFromPointer(reinterpret_cast<const void *>(1)),
	      "distinct pointers must not share an id");
}

/* ------------------------------------------------------- format string */

static void test_format_string()
{
	check(FormatString("%s-%d", "x", 7) == "x-7",
	      "FormatString must interpolate like printf");
	check(FormatString("no args") == "no args",
	      "FormatString must pass a plain string through");

	const std::string long_ish =
		FormatString("%s", std::string(400, 'x').c_str());
	check(long_ish.size() == 400,
	      "FormatString must handle output up to its 512-byte buffer");

	//
	// The grow-and-retry path, which used to be undefined behaviour: the
	// retry re-read a va_list the first vsnprintf had consumed, and asked
	// for the original 512 bytes rather than the size it had just been
	// told it needed. So anything from 512 bytes up both read a dead list
	// and truncated.
	//
	// 511, 512 and 513 bracket the boundary, because the condition that
	// decides whether to grow is the one that was wrong.
	//
	for (size_t len : {(size_t)511, (size_t)512, (size_t)513,
			   (size_t)5000}) {
		const std::string padding(len, 'y');
		const std::string out =
			FormatString("%s", padding.c_str());

		char msg[128];
		std::snprintf(msg, sizeof(msg),
			      "FormatString truncated or corrupted an output "
			      "of %zu bytes",
			      len);

		check(out == padding, msg);
	}

	//
	// Arguments after the long one, so a retry that re-walked a consumed
	// list would produce something other than this rather than merely
	// truncating.
	//
	const std::string tail = FormatString(
		"%s|%d|%s", std::string(600, 'z').c_str(), 42, "end");

	check(tail == std::string(600, 'z') + "|42|end",
	      "FormatString lost or mangled the arguments after a 600-byte "
	      "one, so the retry is not reading an untouched va_list");
}

int main()
{
	test_utf8_round_trip();
	test_clean_guid_string_only_removes_hyphens();
	test_parse_query_string();
	test_pointer_id_round_trip();
	test_format_string();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::puts("test_string_utils: all checks passed");
	return 0;
}
