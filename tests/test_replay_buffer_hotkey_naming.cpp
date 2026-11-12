//
// Behavioural tests for the Backtrack "Save replay" hotkey naming rules.
//
// Header-only and free of libobs, so it compiles anywhere. The file stem is
// where an output id coming from the web becomes a file name, so the tests
// pin down that it can never escape the config directory.
//

#include "streamelements/SEReplayBufferHotkeyNaming.hpp"

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

static void test_name_is_stable_and_scoped_by_id()
{
	check(SEReplayBufferHotkeyName("abc") ==
		      "SELive.Backtrack.SaveReplay.abc",
	      "name carries the output id");
	check(SEReplayBufferHotkeyName("a") != SEReplayBufferHotkeyName("b"),
	      "different outputs get different hotkey names");
	check(SEReplayBufferHotkeyName("a") == SEReplayBufferHotkeyName("a"),
	      "same output always gets the same hotkey name");
}

static void test_uuid_stem_is_unchanged()
{
	const std::string uuid = "ffa6d5e6-87ef-4fc0-95ec-228a5e29299d";

	check(SEReplayBufferHotkeyFileStem(uuid) == uuid,
	      "a UUID is a valid stem as it is");
}

static void test_stem_cannot_escape_the_directory()
{
	const std::string stem =
		SEReplayBufferHotkeyFileStem("../../etc/passwd\\x:y");

	check(stem.find('/') == std::string::npos, "no forward slash");
	check(stem.find('\\') == std::string::npos, "no backslash");
	check(stem.find(':') == std::string::npos, "no drive separator");
	check(stem.find("..") == std::string::npos ||
		      stem.find('/') == std::string::npos,
	      "dots cannot form a parent reference");
	check(stem == "______etc_passwd_x_y",
	      "unsafe characters are replaced one for one");
}

static void test_stem_is_never_empty_and_is_bounded()
{
	check(SEReplayBufferHotkeyFileStem("") == "_", "empty id has a stem");
	check(SEReplayBufferHotkeyFileStem(std::string(1000, 'a')).size() ==
		      128,
	      "stem is capped at 128 characters");
}

int main()
{
	test_name_is_stable_and_scoped_by_id();
	test_uuid_stem_is_unchanged();
	test_stem_cannot_escape_the_directory();
	test_stem_is_never_empty_and_is_bounded();

	if (failures) {
		std::fprintf(stderr, "%d check(s) failed\n", failures);
		return 1;
	}

	std::printf("all checks passed\n");
	return 0;
}
