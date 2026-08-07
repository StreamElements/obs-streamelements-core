// Behavioural demonstration of the IsMatchingIndex bug in
// streamelements/StreamElementsOutput.hpp.
//
// The production VideoEncoderTemplate class is nested inside
// StreamElementsOutputBase and pulls in libobs/Qt transitively. To
// exercise the matching-index logic without that build burden we mirror
// the relevant slice of the class here and verify it behaves correctly.
//
// The source-invariant test in test_source_invariants.cpp asserts that
// the real production file uses the same correct pattern.

#include <cassert>
#include <cstddef>
#include <cstdio>

namespace mirror {

// Minimal mirror of the production logic. The real class also handles a
// CefValue dictionary case (returning false when m_obsEncoderInfo is a
// dict); we keep that branch but represent it with a plain bool here
// since CefValue is not relevant to the index-matching logic.
class VideoEncoderTemplate {
public:
	std::size_t m_index;
	bool m_obsEncoderInfoIsDict;

	VideoEncoderTemplate(std::size_t index)
		: m_index(index), m_obsEncoderInfoIsDict(false)
	{
	}

	// Faithfully reproduces the structure of the production method.
	bool IsMatchingIndex(std::size_t index)
	{
		if (m_obsEncoderInfoIsDict)
			return false;

		return m_index == index;
	}
};

} // namespace mirror

static int failures = 0;

static void check(bool cond, const char *msg)
{
	if (!cond) {
		std::fprintf(stderr, "FAIL: %s\n", msg);
		++failures;
	}
}

int main()
{
	// 1. A template for index 5 matches index 5, not index 3.
	{
		mirror::VideoEncoderTemplate t(5);
		check(t.IsMatchingIndex(5) == true,
		      "template(5) should match query 5");
		check(t.IsMatchingIndex(3) == false,
		      "template(5) should not match query 3");
	}

	// 2. Calling IsMatchingIndex must not mutate m_index. This is the
	//    behaviour the original `return m_index = index;` violated:
	//    every call rewrote m_index, silently corrupting the slot id.
	{
		mirror::VideoEncoderTemplate t(7);
		(void)t.IsMatchingIndex(2);
		check(t.m_index == 7,
		      "IsMatchingIndex must not modify m_index");
		(void)t.IsMatchingIndex(0);
		check(t.m_index == 7,
		      "IsMatchingIndex(0) must not modify m_index");
	}

	// 3. Index 0 must be matchable. With the buggy `=` form, querying
	//    index 0 always returned false (assignment to 0 is falsy),
	//    so a template for slot 0 could never be matched.
	{
		mirror::VideoEncoderTemplate t(0);
		check(t.IsMatchingIndex(0) == true,
		      "template(0) should match query 0 (regression check)");
	}

	// 4. The dictionary-encoder branch always returns false regardless
	//    of index, and must also not mutate state.
	{
		mirror::VideoEncoderTemplate t(4);
		t.m_obsEncoderInfoIsDict = true;
		check(t.IsMatchingIndex(4) == false,
		      "dict-encoder template must not match by index");
		check(t.m_index == 4, "dict-encoder branch must not mutate m_index");
	}

	if (failures) {
		std::fprintf(stderr, "%d assertion(s) failed\n", failures);
		return 1;
	}
	std::puts("test_video_encoder_template_demo: all assertions passed");
	return 0;
}
