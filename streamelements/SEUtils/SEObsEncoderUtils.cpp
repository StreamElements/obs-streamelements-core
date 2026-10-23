#include "SEObsEncoderUtils.hpp"

#include "SEObsDataUtils.hpp"
#include "SEStringUtils.hpp"

#include "../SETrace.hpp"

// OBSEncoderAutoRelease and friends, the RAII wrappers
#include <obs.hpp>
#include <obs-frontend-api.h>
#include <obs-module.h>

#include <map>
#include <vector>
#include <cstdint>

CefRefPtr<CefValue> SerializeObsEncoderProperties(std::string id,
						  obs_data_t *settings)
{
	auto result = CefValue::Create();

	result->SetNull();

	auto props = SETRACE_ADDREF(obs_get_encoder_properties(id.c_str()));

	if (props) {
		if (settings)
			obs_properties_apply_settings(props, settings);

		SerializeObsProperties(props, result);

		obs_properties_destroy(SETRACE_DECREF(props));
	}

	return result;
}
void SerializeObsTransition(std::string videoCompositionId, obs_source_t *t,
			    int durationMilliseconds,
			    CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!t)
		return;

	auto d = CefDictionaryValue::Create();

	std::string id = obs_source_get_id(t);
	d->SetString("class", id);
	d->SetString("videoCompositionId", videoCompositionId);

	auto settings = SETRACE_ADDREF(obs_source_get_settings(t));

	auto props = SETRACE_ADDREF(obs_source_properties(t));

	auto propsValue = CefValue::Create();
	obs_properties_apply_settings(props, settings);
	SerializeObsProperties(props, propsValue);
	obs_properties_destroy(SETRACE_DECREF(props));

	auto settingsValue = SerializeObsData(settings);

	obs_data_release(SETRACE_DECREF(settings));

	d->SetValue("properties", propsValue);
	d->SetValue("settings", settingsValue);

	auto defaultsData = SETRACE_ADDREF(obs_get_source_defaults(id.c_str()));
	d->SetValue("defaultSettings", SerializeObsData(defaultsData));
	obs_data_release(SETRACE_DECREF(defaultsData));

	d->SetString("label", obs_source_get_display_name(id.c_str()));

	uint32_t cx, cy;
	obs_transition_get_size(t, &cx, &cy);
	d->SetInt("width", cx);
	d->SetInt("height", cy);

	auto scaleType = obs_transition_get_scale_type(t);
	switch (scaleType) {
	case OBS_TRANSITION_SCALE_MAX_ONLY:
		d->SetString("scaleType", "maxOnly");
		break;
	case OBS_TRANSITION_SCALE_ASPECT:
		d->SetString("scaleType", "aspect");
		break;
	case OBS_TRANSITION_SCALE_STRETCH:
		d->SetString("scaleType", "stretch");
		break;
	default:
		d->SetString("scaleType", "unknown");
		break;
	}

	d->SetString("alignment",
		     GetAlignmentIdFromInt32(obs_transition_get_alignment(t)));

	d->SetInt("durationMilliseconds", durationMilliseconds);

	output->SetDictionary(d);
}
obs_source_t *GetExistingObsTransition(std::string lookupId)
{
	obs_source_t *result = nullptr;

	obs_frontend_source_list l = {};
	obs_frontend_get_transitions(&l);

	for (size_t idx = 0; idx < l.sources.num; ++idx) {
		auto source = l.sources.array[idx];

		std::string id = obs_source_get_id(source);

		if (id == lookupId) {
			result = SETRACE_ADDREF(obs_source_get_ref(source));

			break;
		}
	}

	obs_frontend_source_list_free(&l);

	return result;
}
bool DeserializeObsTransition(CefRefPtr<CefValue> input,
			      obs_source_t **out_transition,
			      int *durationMilliseconds, bool useExisting)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	auto d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return false;

	std::string id = d->GetString("class");

	OBSDataAutoRelease settings = nullptr;
	OBSSourceAutoRelease transition = nullptr;

	if (d->HasKey("settings")) {
		settings = SETRACE_SCOPEREF(obs_data_create());

		DeserializeObsData(d->GetValue("settings"), settings);
	}

	if (useExisting) {
		transition = GetExistingObsTransition(id);

		if (!transition) {
			return false;
		}
	} else {
		transition = SETRACE_SCOPEREF(obs_source_create_private(
			id.c_str(), id.c_str(), settings));
	}

	if (settings) {
		obs_source_update(transition, settings);
	}

	if (d->HasKey("width") && d->GetType("width") == VTYPE_INT &&
	    d->HasKey("height") && d->GetType("height") == VTYPE_INT) {
		int cx = d->GetInt("width");
		int cy = d->GetInt("height");

		if (cx > 0 && cy > 0) {
			obs_transition_set_size(transition, cx, cy);
		}
	}

	if (d->HasKey("scaleType") && d->GetType("scaleType") == VTYPE_STRING) {
		auto scaleType = d->GetString("scaleType");

		if (scaleType == "maxOnly") {
			obs_transition_set_scale_type(
				transition, OBS_TRANSITION_SCALE_MAX_ONLY);
		} else if (scaleType == "aspect") {
			obs_transition_set_scale_type(
				transition, OBS_TRANSITION_SCALE_ASPECT);
		} else if (scaleType == "stretch") {
			obs_transition_set_scale_type(
				transition, OBS_TRANSITION_SCALE_STRETCH);
		} else {
			return false;
		}
	}

	if (d->HasKey("alignment") && d->GetType("alignment") == VTYPE_STRING) {
		obs_transition_set_alignment(
			transition, GetInt32FromAlignmentId(
					    d->GetString("alignment").c_str()));
	}

	if (obs_transition_fixed(transition)) {
		*durationMilliseconds = 0;
	} else if (d->HasKey("durationMilliseconds") &&
		   d->GetType("durationMilliseconds") == VTYPE_INT) {
		*durationMilliseconds = d->GetInt("durationMilliseconds");
	} else {
		*durationMilliseconds = 300; // OBS FE default
	}

	*out_transition = SETRACE_ADDREF(obs_source_get_ref(transition));

	return true;
}
CefRefPtr<CefDictionaryValue> SerializeObsEncoder(obs_encoder_t *e)
{
	auto result = CefDictionaryValue::Create();

	auto id = obs_encoder_get_id(e);
	result->SetString("class", id);

	auto name = obs_encoder_get_name(e);
	result->SetString("name", name ? name : id);

	auto displayName = obs_encoder_get_display_name(obs_encoder_get_id(e));
	result->SetString("label",
			  displayName ? displayName : (name ? name : id));

	auto codec = obs_encoder_get_codec(e);

	if (codec)
		result->SetString("codec", obs_encoder_get_codec(e));
	else
		result->SetNull("codec");

	auto encoder_type = obs_encoder_get_type(e);

	if (encoder_type == OBS_ENCODER_VIDEO) {
		result->SetString("type", "video");

		if (obs_encoder_parent_video(e) ||
		    obs_encoder_scaling_enabled(e)) {
			result->SetInt("width", obs_encoder_get_width(e));
			result->SetInt("height", obs_encoder_get_height(e));
		}
	} else if (encoder_type == OBS_ENCODER_AUDIO) {
		result->SetString("type", "audio");

		result->SetInt("audioMix", obs_encoder_get_mixer_index(e));
	}

	auto settings = SETRACE_ADDREF(obs_encoder_get_settings(e));

	result->SetValue("settings", SerializeObsData(settings));

	result->SetValue("properties",
			 SerializeObsEncoderProperties(obs_encoder_get_id(e),
						       settings));

	auto defaultSettings = SETRACE_ADDREF(obs_encoder_get_defaults(e));

	if (defaultSettings) {
		result->SetValue("defaultSettings",
				 SerializeObsData(defaultSettings));

		obs_data_release(SETRACE_DECREF(defaultSettings));
	}

	obs_data_release(SETRACE_DECREF(settings));

	return result;
}
static obs_encoder_t *DeserializeObsEncoder(obs_encoder_type type,
					    CefRefPtr<CefValue> input,
					    int mixer_idx)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	auto d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return nullptr;

	std::string id = d->GetString("class");
	std::string name = id;

	if (d->HasKey("name") && d->GetType("name") == VTYPE_STRING)
		name = d->GetString("name");

	OBSDataAutoRelease settings = SETRACE_SCOPEREF(obs_data_create());

	if (d->HasKey("settings")) {
		if (!DeserializeObsData(d->GetValue("settings"), settings))
			return nullptr;
	}

	if (mixer_idx < 0) {
		if (d->HasKey("audioMix") &&
		    d->GetType("audioMix") == VTYPE_INT)
			mixer_idx = d->GetInt("audioMix");
	}

	if (mixer_idx < 0 || mixer_idx >= MAX_AUDIO_MIXES)
		mixer_idx = 0;

	if (type == OBS_ENCODER_VIDEO) {
		auto encoder = SETRACE_ADDREF(obs_video_encoder_create(
			id.c_str(), name.c_str(), settings, nullptr));

		if (encoder && d->HasKey("width") &&
		    d->GetType("width") == VTYPE_INT && d->HasKey("height") &&
		    d->GetType("height") == VTYPE_INT) {
			int width = d->GetInt("width");
			int height = d->GetInt("height");

			if (width > 0 && height > 0) {
				obs_encoder_set_scaled_size(encoder, width,
							    height);
			}
		}

		return encoder;
	}

	else if (type == OBS_ENCODER_AUDIO)
		return SETRACE_ADDREF(obs_audio_encoder_create(
			id.c_str(), name.c_str(), settings,
			static_cast<size_t>(mixer_idx), nullptr));
	else
		return nullptr;
}
obs_encoder_t *DeserializeObsVideoEncoder(CefRefPtr<CefValue> input)
{
	return DeserializeObsEncoder(OBS_ENCODER_VIDEO, input, 0);
}
obs_encoder_t *DeserializeObsAudioEncoder(CefRefPtr<CefValue> input,
					  int mixer_idx)
{
	return DeserializeObsEncoder(OBS_ENCODER_VIDEO, input, mixer_idx);
}
