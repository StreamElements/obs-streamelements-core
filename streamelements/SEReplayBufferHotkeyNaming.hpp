#pragma once

//
// Naming rules for the "Save replay" hotkey of a SE.Live Backtrack output.
//
// Kept free of libobs, Qt and CEF on purpose, so tests/ can include it: the
// hotkey name is the identity OBS stores bindings under, and the file stem is
// the one place an output id from the web is turned into a file name.
//

#include <cstddef>
#include <string>

// Name OBS registers the hotkey under. Stable per output id, so bindings
// survive the output being recreated.
inline std::string SEReplayBufferHotkeyName(const std::string &outputId)
{
	return "SELive.Backtrack.SaveReplay." + outputId;
}

// File stem for the persisted bindings of one output. Output ids arrive from
// the web, so anything that is not [A-Za-z0-9_-] is replaced, which also rules
// out path separators and "..". Never empty, never longer than 128 chars.
inline std::string SEReplayBufferHotkeyFileStem(const std::string &outputId)
{
	constexpr size_t kMaxLength = 128;

	std::string stem;
	stem.reserve(outputId.size());

	for (char c : outputId) {
		const bool ok = (c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') || c == '_' || c == '-';

		stem.push_back(ok ? c : '_');

		if (stem.size() >= kMaxLength)
			break;
	}

	if (stem.empty())
		stem = "_";

	return stem;
}
