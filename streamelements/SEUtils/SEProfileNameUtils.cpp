#include "SEProfileNameUtils.hpp"

#include <cctype>

//
// What a profile name cannot contain in a directory name.
//
// The set is the union of what Windows rejects, what OBS's sanitisation
// visibly replaces, and the two path separators. Space is here because
// "TWITCH Warlock" -> TWITCH_Warlock is the case this whole file exists
// for; it is not a filesystem rule but it is the rule OBS actually applies.
//
static bool IsReplacedInDirectoryName(unsigned char c)
{
	if (c == ' ' || c == '/' || c == '\\')
		return true;

	static const char *const forbidden = "<>:\"|?*";
	for (const char *p = forbidden; *p; ++p) {
		if (c == (unsigned char)*p)
			return true;
	}

	return false;
}

static bool EqualsIgnoreCase(const std::string &a, const std::string &b)
{
	if (a.size() != b.size())
		return false;

	for (size_t i = 0; i < a.size(); ++i) {
		const unsigned char ca = (unsigned char)a[i];
		const unsigned char cb = (unsigned char)b[i];

		if (std::tolower(ca) != std::tolower(cb))
			return false;
	}

	return true;
}

std::string SEProfileDirectoryName(const std::string &profileName)
{
	std::string result;
	result.reserve(profileName.size());

	for (size_t i = 0; i < profileName.size(); ++i) {
		const unsigned char c = (unsigned char)profileName[i];

		result.push_back(IsReplacedInDirectoryName(c) ? '_' : (char)c);
	}

	return result;
}

bool SEProfileMatches(const std::string &available,
		      const std::string &requested)
{
	if (requested.empty() || available.empty())
		return false;

	if (EqualsIgnoreCase(available, requested))
		return true;

	return EqualsIgnoreCase(SEProfileDirectoryName(available), requested);
}

std::string SEResolveProfileName(const std::vector<std::string> &available,
				 const std::string &requested)
{
	if (requested.empty())
		return std::string();

	//
	// The name first, over every profile, before any directory spelling.
	//
	// Two profiles can in principle disagree only by that spelling -- a
	// real profile called "TWITCH_Warlock" next to one called "TWITCH
	// Warlock" -- and in that case the page asked for a name that exists
	// and must get it. Searching in this order makes the answer depend
	// on what was asked for rather than on which lives earlier in the
	// list.
	//
	for (const std::string &name : available) {
		if (EqualsIgnoreCase(name, requested))
			return name;
	}

	for (const std::string &name : available) {
		if (EqualsIgnoreCase(SEProfileDirectoryName(name), requested))
			return name;
	}

	return std::string();
}
