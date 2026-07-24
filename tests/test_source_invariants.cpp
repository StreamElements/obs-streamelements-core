// Source-invariant regression gate.
//
// Reads production source files and asserts that specific buggy
// patterns identified in the crash-bug review are absent. These checks
// run in milliseconds and require no OBS / Qt / CEF dependencies.
//
// Each invariant references the bug it guards against; if a future edit
// re-introduces the pattern, the test fails with a clear message.

#include "source_paths.hpp"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <regex>
#include <sstream>
#include <string>

static int failures = 0;

static std::string slurp(const std::string &relpath)
{
	std::string full = std::string(se_tests::kRepoRoot) + "/" + relpath;
	std::ifstream in(full);
	if (!in) {
		std::fprintf(stderr, "FATAL: cannot open %s\n", full.c_str());
		std::exit(2);
	}
	std::stringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

static std::size_t count_matches(const std::string &haystack,
				 const std::regex &re)
{
	auto begin = std::sregex_iterator(haystack.begin(), haystack.end(), re);
	auto end = std::sregex_iterator();
	return static_cast<std::size_t>(std::distance(begin, end));
}

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

// --- C2: IsMatchingIndex must compare with ==, not assign with =.
//
// The original line was:  return m_index = index;
// which assigns and returns truthy/falsy of the assigned value rather
// than comparing. The fix is:  return m_index == index;
static void check_c2_video_encoder_template_match()
{
	auto src = slurp("streamelements/StreamElementsOutput.hpp");

	// The buggy assignment form must be gone. Match `m_index` followed
	// by `=` with no neighbouring `=` (i.e. real assignment, not `==`).
	std::regex bad(R"(return\s+m_index\s*=\s*[^=])");
	check(count_matches(src, bad) == 0,
	      "C2: StreamElementsOutput.hpp must not assign to m_index in IsMatchingIndex "
	      "(should be `return m_index == index;`)");

	// And the correct comparison form must be present.
	std::regex good(R"(return\s+m_index\s*==\s*index\s*;)");
	check(count_matches(src, good) >= 1,
	      "C2: StreamElementsOutput.hpp must contain `return m_index == index;`");
}

// --- C4: self-assignment of cefClientId in batchInvokeSeries leaves
// the field holding uninitialised heap memory which is then routed back
// to CEF clients as a callback id.
static void check_c4_no_self_assign_cefclientid()
{
	auto src = slurp("streamelements/StreamElementsApiMessageHandler.cpp");

	std::regex bad(R"(context\s*->\s*cefClientId\s*=\s*context\s*->\s*cefClientId)");
	check(count_matches(src, bad) == 0,
	      "C4: StreamElementsApiMessageHandler.cpp must not contain "
	      "`context->cefClientId = context->cefClientId` (self-assign)");
}

// --- C5: CefParseJSON returns null on malformed input. The result must
// not be passed straight into CefListValue::SetValue without a null
// check, because that dereferences a null CefRefPtr.
static void check_c5_cefparsejson_null_guarded()
{
	auto src = slurp("streamelements/StreamElementsApiMessageHandler.cpp");

	// The fix guards parsedValue against null before SetValue.
	// We assert one of two acceptable patterns:
	//   (a) an `if (!parsedValue ...) ...` near a CefParseJSON call, or
	//   (b) the parsed value is replaced with a default null CefValue.
	std::regex parseCall(R"(CefParseJSON\s*\()");
	check(count_matches(src, parseCall) >= 1,
	      "C5: expected at least one CefParseJSON call site to validate");

	// Acceptable guard: a null check on parsedValue between the
	// CefParseJSON call and the SetValue call.
	std::regex guarded(
		R"(CefParseJSON[\s\S]{0,400}?if\s*\(\s*!\s*parsedValue\b)");
	check(count_matches(src, guarded) >= 1,
	      "C5: CefParseJSON result must be null-checked before being passed to SetValue");
}

// --- C6: audio encoder bounds check must use `>=`, not `>`. The array
// has MAX_AUDIO_MIXES slots, valid indices are [0, MAX_AUDIO_MIXES).
static void check_c6_audio_encoder_bounds()
{
	auto src = slurp("streamelements/StreamElementsAudioComposition.cpp");

	std::regex bad(R"(if\s*\(\s*index\s*>\s*MAX_AUDIO_MIXES\s*\))");
	check(count_matches(src, bad) == 0,
	      "C6: audio encoder bound check must use `>=`, not `>` "
	      "(`if (index > MAX_AUDIO_MIXES)` allows one-past-end access)");

	std::regex good(R"(if\s*\(\s*index\s*>=\s*MAX_AUDIO_MIXES\s*\))");
	check(count_matches(src, good) >= 2,
	      "C6: expected `if (index >= MAX_AUDIO_MIXES)` in both streaming "
	      "and recording audio encoder accessors");
}

// --- C10: the `setCurrentProfile` API handler was registered twice, so
// the first registration was silently overwritten. Assert it now
// appears exactly once.
static void check_c10_no_duplicate_handler_setcurrentprofile()
{
	auto src = slurp("streamelements/StreamElementsApiMessageHandler.cpp");

	std::regex reg(R"(API_HANDLER_BEGIN\(\"setCurrentProfile\"\))");
	std::size_t count = count_matches(src, reg);
	if (count != 1) {
		std::fprintf(stderr,
			     "FAIL: C10: setCurrentProfile is registered %zu time(s); expected exactly 1\n",
			     count);
		++failures;
	}
}

int main()
{
	check_c2_video_encoder_template_match();
	check_c4_no_self_assign_cefclientid();
	check_c5_cefparsejson_null_guarded();
	check_c6_audio_encoder_bounds();
	check_c10_no_duplicate_handler_setcurrentprofile();

	if (failures) {
		std::fprintf(stderr, "%d source invariant(s) violated\n",
			     failures);
		return 1;
	}
	std::puts("test_source_invariants: all invariants hold");
	return 0;
}
