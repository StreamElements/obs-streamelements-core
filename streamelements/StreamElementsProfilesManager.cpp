#include "StreamElementsProfilesManager.hpp"
#include "StreamElementsUtils.hpp"
#include <string.h>
#include <obs.h>
#include <obs-frontend-api.h>
#include <util/config-file.h>
#include <util/platform.h>

StreamElementsProfilesManager::StreamElementsProfilesManager() {}

StreamElementsProfilesManager::~StreamElementsProfilesManager() {}

void StreamElementsProfilesManager::SerializeAllProfiles(
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

void StreamElementsProfilesManager::SerializeCurrentProfile(
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

bool StreamElementsProfilesManager::DeserializeCurrentProfileById(
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

	std::string actualId = "";

	char **profiles = obs_frontend_get_profiles();

	for (size_t index = 0; profiles[index] && !actualId.size(); ++index) {
		if (strcasecmp(profiles[index], id.c_str()) == 0) {
			actualId = profiles[index];
			break;
		}
	}

	bfree(profiles);

	if (!actualId.size())
		return false;

	obs_frontend_set_current_profile(actualId.c_str());

	return true;
}
