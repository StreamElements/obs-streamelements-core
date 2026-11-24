#pragma once

//
// Resolving what a page sends as a profile id to the name OBS itself
// answers to.
//
// The host API deliberately hands out two different spellings of every
// profile and then has to accept both back. `getAllProfiles` reports
//
//     id    the profile's directory under basic/profiles, e.g. TWITCH_Warlock
//     name  the [General] Name inside its basic.ini, e.g. "TWITCH Warlock"
//
// while obs_frontend_get_profiles() and obs_frontend_set_current_profile()
// only ever speak the NAME. So a page that round-trips what it was given --
// getAllProfiles, pick one, setCurrentProfile -- sends the directory back,
// and the switch used to fail for every profile whose name OBS had to
// change to make a directory: any space, or anything else the filesystem
// will not take.
//
// Accepting the directory spelling in DeserializeCurrentProfileById is the
// fix. The matching lives here rather than there because
// SEPathUtils.cpp includes libobs and cef-headers, and this has to be
// reachable from tests/, which link neither.
//
// Deliberately a pure function over strings, so the interesting cases --
// case-insensitivity, a directory that matches no name, two profiles whose
// directory names differ only by case -- are ordinary test input rather
// than something that needs OBS running.
//

#include <string>
#include <vector>

//
// Given one of OBS's profile names (`available`, in OBS's own order) and a
// requested name, return true when they are the same profile.
//
// `requested` is matched against the name first, then against the
// directory OBS's writer would have produced for each name. The second is
// the one that fixes the round trip; the first keeps working for every
// page that already sends the name, which is what the docs say to send.
//
// Case-insensitive both ways, because the page is not the authority on how
// a Windows directory got its capitals.
//
bool SEProfileMatches(const std::string &available,
		      const std::string &requested);

//
// The profile name out of `available` that `requested` refers to, or an
// empty string when none does.
//
// Returns the NAME, not the request, so a caller can hand the result
// straight to obs_frontend_set_current_profile(). An empty request matches
// nothing: it is not a profile, and treating it as one would turn a
// malformed host call into a profile switch.
//
std::string SEResolveProfileName(const std::vector<std::string> &available,
				 const std::string &requested);

//
// OBS's directory name for a profile name.
//
// OBS writes the directory by running the name through its own
// sanitisation; the observable rule on Windows is that the separators and
// other characters the filesystem rejects become underscores, and spaces
// become underscores too -- which is how "TWITCH Warlock" and
// "STREAMING PS4" end up as TWITCH_Warlock and STREAMING_PS4 next to an
// untouched "Other".
//
// Exposed so the rule is asserted in one place rather than assumed in two.
//
std::string SEProfileDirectoryName(const std::string &profileName);
