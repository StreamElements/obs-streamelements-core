#include "SEHostUpdateManager.hpp"

#include "SETrace.hpp"
#include "SEUtils/SEEnvConfigUtils.hpp"

#include <obs.h>

#include <string>

namespace {

//
// The two signals the updater listens for, and the three flags they carry.
// Spelled once, here, which is the point of the class.
//
const char *const kCheckForUpdatesSilent =
	"streamelements_request_check_for_updates_silent";
const char *const kCheckForUpdatesInteractive =
	"streamelements_request_check_for_updates";

void emit_check_for_updates(const char *signal, bool allowDowngrade,
			    bool forceInstall, bool allowUseLastResponse)
{
	calldata_t *cd = SETRACE_ADDREF(calldata_create());

	calldata_set_bool(cd, "allow_downgrade", allowDowngrade);
	calldata_set_bool(cd, "force_install", forceInstall);
	calldata_set_bool(cd, "allow_use_last_response", allowUseLastResponse);

	signal_handler_signal(obs_get_signal_handler(), signal, cd);

	calldata_destroy(SETRACE_DECREF(cd));
}

} // namespace

SEHostUpdateManager::SEHostUpdateManager() {}

SEHostUpdateManager::~SEHostUpdateManager() {}

void SEHostUpdateManager::SerializeReleaseGroupProperties(
	CefRefPtr<CefValue> &output)
{
	const std::string quality =
		ReadProductEnvironmentConfigurationString("Quality");
	const std::string manifestUrl =
		ReadProductEnvironmentConfigurationString("ManifestUrl");

	//
	// Both or nothing, as the handler body had it: a dictionary with one
	// of the two filled in would be worse than null for a page deciding
	// whether this installation is on a managed channel at all.
	//
	if (quality.size() && manifestUrl.size()) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("quality", quality.c_str());
		d->SetString("manifestUrl", manifestUrl.c_str());

		output->SetDictionary(d);
	} else {
		output->SetNull();
	}
}

void SEHostUpdateManager::DeserializeReleaseGroupProperties(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("quality") || d->GetType("quality") != VTYPE_STRING ||
	    !d->HasKey("manifestUrl") ||
	    d->GetType("manifestUrl") != VTYPE_STRING)
		return;

	const std::string quality = d->GetString("quality");
	const std::string manifestUrl = d->GetString("manifestUrl");

	output->SetBool(WriteProductEnvironmentConfigurationStrings(
		{{"", "Quality", quality}, {"", "ManifestUrl", manifestUrl}}));
}

void SEHostUpdateManager::RequestCheckForUpdates(bool allowDowngrade,
						 bool forceInstall,
						 bool allowUseLastResponse)
{
	emit_check_for_updates(kCheckForUpdatesSilent, allowDowngrade,
			       forceInstall, allowUseLastResponse);
}

void SEHostUpdateManager::RequestCheckForUpdatesInteractive()
{
	//
	// All three false, which is what the menu action passed. Note
	// allow_use_last_response in particular: the silent path a page asks
	// for defaults it to true, and a person who picked "check for
	// updates" from a menu is asking for a fresh answer.
	//
	emit_check_for_updates(kCheckForUpdatesInteractive, false, false,
			       false);
}
