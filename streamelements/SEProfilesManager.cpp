#include "SEProfilesManager.hpp"
#include "SEUtils.hpp"
#include "SEUtils/SEProfileNameUtils.hpp"
#include <string.h>
#include <vector>
#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

SEProfilesManager::SEProfilesManager() {}

SEProfilesManager::~SEProfilesManager() {}

void SEProfilesManager::SerializeAllProfiles(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefListValue> list = CefListValue::Create();

	std::map<std::string, std::string> profiles;
	ReadListOfObsProfiles(profiles);

	for (auto profile : profiles) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", profile.first);
		d->SetString("name", profile.second);

		list->SetDictionary(list->GetSize(), d);
	}

	output->SetList(list);
}

void SEProfilesManager::SerializeCurrentProfile(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	char *currentProfile = obs_frontend_get_current_profile();

	std::string id = currentProfile;

	std::map<std::string, std::string> profiles;
	ReadListOfObsProfiles(profiles);

	for (auto profile : profiles) {
		if (profile.second == currentProfile) {
			id = profile.first;
			break;
		}
	}

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetString("id", id);
	d->SetString("name", currentProfile);

	bfree(currentProfile);

	output->SetDictionary(d);
}

bool SEProfilesManager::DeserializeCurrentProfileById(
	CefRefPtr<CefValue> input)
{
	if (obs_frontend_streaming_active() || obs_frontend_recording_active())
		return false;

	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
		return false;

	std::string id = d->GetString("id").ToString();

	if (!id.size())
		return false;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	//
	// `id` is whatever the page kept from getAllProfiles, and that is the
	// profile's DIRECTORY name, not the name OBS answers to. Both are
	// accepted: resolving them is SEResolveProfileName's whole job, and
	// it returns the name obs_frontend_set_current_profile wants.
	//
	// Before this, a page that round-tripped what it was given got
	// `false` for any profile whose name OBS had to change to make a
	// directory -- "TWITCH Warlock" is TWITCH_Warlock on disk, and the
	// two were compared as unrelated strings.
	//
	std::vector<std::string> available;

	char **profiles = obs_frontend_get_profiles();

	for (size_t index = 0; profiles[index]; ++index)
		available.push_back(profiles[index]);

	bfree(profiles);

	const std::string actualId = SEResolveProfileName(available, id);

	if (actualId.empty())
		return false;

	obs_frontend_set_current_profile(actualId.c_str());

	return true;
}
