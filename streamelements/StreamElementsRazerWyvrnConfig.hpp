#pragma once

//
// Parsing of Razer WYVRN configuration files.
//
// This header is deliberately free of libobs, Qt and OBS headers. It depends on
// nothing but the standard library and the local CefValue data model, so the
// ctest suite can compile it directly and exercise it against real config files
// without an obs-studio tree. Everything that needs the filesystem or OBS lives
// in StreamElementsRazerWyvrnManager.
//
// WYVRN has no enumeration API: the set of events a machine can render is
// whatever the `wyvrn.config` files under the Interhaptics install declare. Those
// files were read rather than assumed, and three properties of the real data
// drive this code:
//
//   * Filenames appear as `wyvrn.config`, `Wyvrn.config` and `WYVRN.config`.
//     Matching must be case-insensitive, or a literal match finds 13 of 146.
//
//   * Only 22 of 146 files declare `ExternalCommands`. The other 124 are an
//     unrelated schema (`audioToHapticProfile` / `bands` / `muteOnLoad`) with no
//     events in them at all. A file with no `ExternalCommands` is NORMAL, not
//     malformed, and must not produce a diagnostic.
//
//   * `FallbackCommands` entries are regular-expression PATTERNS, not event
//     names. They are reported, but tagged so a caller never offers one as
//     something to fire.
//

#include <string>
#include <vector>

#include "deps/cef-stub/cef_value.hpp"

/* ========================================================================= */

//
// One Chroma effect attached to an event, from the config's `Chroma_Events`.
//
struct StreamElementsRazerWyvrnChromaComponent {
	// `Chroma_Effect`: the base filename of the .chroma assets, without the
	// device suffix. "Aim_On" yields "Aim_On_Keyboard.chroma" and friends.
	std::string effect;

	bool interrupt = false;
};

//
// One haptic effect attached to an event, from the config's `Haptic_Events`.
//
struct StreamElementsRazerWyvrnHapticTarget {
	// Head, Chest, Waist, Hand, Leg, All, Top, Down. Passed through as
	// written: the data contains both `waist` and the misspelling `Wasit`,
	// and silently normalising them here would hide that from the caller.
	std::string target;

	// Global, Left or Right.
	std::string spatialization;

	double gain = 1.0;
};

struct StreamElementsRazerWyvrnHapticComponent {
	// `Haptic_Effect`: the base filename of the .haps asset.
	std::string effect;

	int loop = 0;
	std::string mixing;
	std::string priority;

	// Where on the body this lands. Usually several entries.
	std::vector<StreamElementsRazerWyvrnHapticTarget> targeting;
};

//
// One event a caller may name.
//
struct StreamElementsRazerWyvrnEventInfo {
	// `External_Command_ID`.
	std::string id;

	// The hapticFolders subfolder the config came from, e.g. "007 First
	// Light". Not part of the event's identity - two applications may declare
	// the same id - but it is how a caller filters a 4,000-entry list down to
	// something browsable.
	std::string source;

	// "exact" for ExternalCommands, "fallbackPattern" for FallbackCommands.
	// A fallback entry is a regex, so it is not directly fireable.
	std::string kind;

	std::vector<StreamElementsRazerWyvrnChromaComponent> chroma;
	std::vector<StreamElementsRazerWyvrnHapticComponent> haptics;
};

/* ========================================================================= */

//
// True when `fileName` is a WYVRN config, compared case-insensitively.
//
// Takes a bare filename, not a path.
//
bool IsRazerWyvrnConfigFileName(const std::string &fileName);

//
// Parse one configuration document.
//
// `sourceName` is recorded on every event produced and is normally the
// containing folder's name.
//
// Returns an empty vector for a document that declares no events - including the
// 124-of-146 case where the file is an audio-to-haptic profile with an entirely
// different schema. That is not an error and callers must not treat it as one.
// Malformed JSON likewise yields an empty vector rather than throwing, because
// one unreadable file among 146 must not abort a scan.
//
std::vector<StreamElementsRazerWyvrnEventInfo>
ParseRazerWyvrnConfig(const std::string &json, const std::string &sourceName);

//
// Split a parsed document into the two command kinds, for callers that want to
// know whether anything was recognised at all without inspecting every entry.
//
size_t CountRazerWyvrnEventsOfKind(
	const std::vector<StreamElementsRazerWyvrnEventInfo> &events,
	const std::string &kind);
