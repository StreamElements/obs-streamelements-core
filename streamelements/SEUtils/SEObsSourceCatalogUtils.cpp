#include "SEObsSourceCatalogUtils.hpp"

#include "SEObsDataUtils.hpp"
#include "SEStringUtils.hpp"

#include "../SETrace.hpp"

// OBSDataAutoRelease and friends, the RAII wrappers
#include <obs.hpp>
#include <obs-module.h>

#include <util/platform.h>

#include <map>
#include <shared_mutex>
#include <string>
#include <vector>
#include <cstdint>

static inline const char *safe_str(const char *s,
				   const char *defaultValue = "(NULL)")
{
	if (s == NULL)
		return defaultValue;
	else
		return s;
}

static void SerializeAvailableSourceTypes(
	CefRefPtr<CefValue> &output, uint32_t requireAnyOfOutputFlagsMask,
	std::vector<obs_source_type> requiredSourceTypes,
	bool serializeProperties)
{
	static std::shared_mutex mutex;
	static std::map<std::string, CefRefPtr<CefDictionaryValue>> cache;

	std::unique_lock guard(mutex);

	std::map<std::string, bool> existingSourceIds;

	// Response codec collection (array)
	CefRefPtr<CefListValue> list = CefListValue::Create();

	// Response codec collection is our root object
	output->SetList(list);

	auto add = [&](std::string sourceId, const char *unversioned_id,
		       obs_canvas_t *canvas) -> void {
		existingSourceIds[sourceId] = true;

		// Get source caps
		uint32_t sourceCaps =
			obs_get_source_output_flags(sourceId.c_str());

		// Check if the source is disabled, if so - skip it
		if ((sourceCaps & OBS_SOURCE_CAP_DISABLED) != 0)
			return;

		// If source has video / audio / any of the required caps
		if ((sourceCaps & requireAnyOfOutputFlagsMask) == 0L)
			return;

		if (cache.count(sourceId)) {
			// Append dictionary to response list
			list->SetDictionary(list->GetSize(), cache[sourceId]);

			return;
		}

		OBSSourceAutoRelease source = nullptr;

		if (canvas && sourceId == "scene") {
			obs_scene_t *scene =
				SETRACE_NOREF(obs_canvas_scene_create(
					canvas,
					CreateGloballyUniqueIdString().c_str()));

			source = SETRACE_SCOPEREF(obs_scene_get_source(scene));
		} else {
			// We need all of this create-release dance since some 3rd party sources do not support obs_get_source_properties :(
			OBSDataAutoRelease settings =
				SETRACE_SCOPEREF(obs_data_create());

			source = SETRACE_SCOPEREF(obs_source_create_private(
				sourceId.c_str(),
				CreateGloballyUniqueIdString().c_str(),
				settings));
		}

		if (!source)
			return;

		obs_source_type sourceType = obs_source_get_type(source);

		if (!unversioned_id) {
			unversioned_id = obs_source_get_unversioned_id(source);
		}

		bool hasRequiredType = false;

		for (auto requiredType : requiredSourceTypes) {
			if (requiredType == sourceType) {
				hasRequiredType = true;

				break;
			}
		}

		if (!hasRequiredType)
			return;

		// Create source response dictionary
		CefRefPtr<CefDictionaryValue> dic =
			CefDictionaryValue::Create();

		SerializeObsSource(source, dic, false, serializeProperties);

		// Append dictionary to response list
		list->SetDictionary(list->GetSize(), dic);

		// Add to cache for reuse
		cache[sourceId] = dic;
	};

	// Iterate over all required source types
	for (auto requiredType : requiredSourceTypes) {
		if (requiredType == OBS_SOURCE_TYPE_SCENE &&
		    (requireAnyOfOutputFlagsMask & OBS_SOURCE_VIDEO) ==
			    OBS_SOURCE_VIDEO) {

			obs_video_info ovi;

			if (obs_get_video_info(&ovi)) {
				OBSCanvasAutoRelease canvas = SETRACE_SCOPEREF(
					obs_canvas_create_private(
						CreateGloballyUniqueIdString()
							.c_str(),
						&ovi, PROGRAM | EPHEMERAL));

				add("scene", "scene", canvas);
			}

			break;
		}

		// Iterate over all required source type
		for (size_t idx = 0;; ++idx) {
			// Filled by obs_enum_input_types() call below
			const char *sourceId;

			if (requiredType == OBS_SOURCE_TYPE_INPUT) {
				const char *unversioned_id;

				if (!obs_enum_input_types2(idx, &sourceId,
							   &unversioned_id))
					break;

				add(sourceId, unversioned_id, nullptr);
			} else if (requiredType == OBS_SOURCE_TYPE_FILTER) {
				if (!obs_enum_filter_types(idx, &sourceId))
					break;

				add(sourceId, nullptr, nullptr);
			} else if (requiredType == OBS_SOURCE_TYPE_TRANSITION) {
				if (!obs_enum_transition_types(idx, &sourceId))
					break;

				add(sourceId, nullptr, nullptr);
			} else {
				break;
			}
		}
	}
}

static void SerializeExistingSources(
	CefRefPtr<CefValue> &output, uint32_t requireAnyOfOutputFlagsMask,
	uint32_t requireOutputFlagsMask,
	std::vector<obs_source_type> requireSourceTypes,
	bool serializeProperties)
{
	struct local_context_t {
		CefRefPtr<CefListValue> list;
		uint32_t requireAnyOfOutputFlagsMask;
		uint32_t requireOutputFlagsMask;
		std::vector<obs_source_type> requireSourceTypes;
		bool serializeProperties;
	};

	local_context_t local_context;

	local_context.list = CefListValue::Create();
	local_context.requireAnyOfOutputFlagsMask = requireAnyOfOutputFlagsMask;
	local_context.requireOutputFlagsMask = requireOutputFlagsMask;
	local_context.requireSourceTypes = requireSourceTypes;
	local_context.serializeProperties = serializeProperties;

	auto process = [](void *param, obs_source_t *source) -> bool {
		auto context = (local_context_t *)param;

		if (obs_source_removed(source)) {
			// Skip sources which have been marked removed
			return true;
		}

		// Get source caps
		uint32_t sourceCaps = obs_source_get_output_flags(source);

		obs_source_type sourceType = obs_source_get_type(source);

		bool hasRequiredSourceType = false;
		for (auto requireType : context->requireSourceTypes) {
			if (requireType == sourceType) {
				hasRequiredSourceType = true;
				break;
			}
		}

		// If source has video
		if ((sourceCaps & context->requireOutputFlagsMask) ==
			    context->requireOutputFlagsMask &&
		    (sourceCaps & context->requireAnyOfOutputFlagsMask) != 0L &&
		    hasRequiredSourceType) {
			// Create source response dictionary
			CefRefPtr<CefDictionaryValue> dic =
				CefDictionaryValue::Create();

			SerializeObsSource(source, dic, true,
					   context->serializeProperties);

			// Append dictionary to response list
			context->list->SetDictionary(context->list->GetSize(),
						     dic);
		} else {
			blog(LOG_INFO, "Skipping source '%s' ('%s')",
			     obs_source_get_id(source),
			     obs_source_get_unversioned_id(source));
		}

		return true;
	};

	for (auto sourceType : local_context.requireSourceTypes) {
		switch (sourceType) {
		case OBS_SOURCE_TYPE_SCENE:
			obs_enum_scenes(process, &local_context);
			break;
		default:
			break;
		}
	}

	// Iterate over all sources
	obs_enum_sources(process, &local_context);

	// Response codec collection is our root object
	output->SetList(local_context.list);
}

//
// The nine shapes the host API asks for.
//
// Every OBS_SOURCE_* constant in the plug-in's API surface used to be
// written at the call site, which is what kept SEApiMessageHandler.cpp
// bound to libobs for 27 of its tokens. They live here now, one
// combination per function, and each handler body is a single call
// (CORE-757).
//

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &output)
{
	SerializeAvailableSourceTypes(output,
				      OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO,
				      {OBS_SOURCE_TYPE_INPUT,
				       OBS_SOURCE_TYPE_SCENE},
				      false);
}

void SerializeAvailableVideoInputSourceTypes(CefRefPtr<CefValue> &output)
{
	SerializeAvailableSourceTypes(output, OBS_SOURCE_VIDEO,
				      {OBS_SOURCE_TYPE_INPUT,
				       OBS_SOURCE_TYPE_SCENE},
				      false);
}

void SerializeAvailableAudioInputSourceTypes(CefRefPtr<CefValue> &output)
{
	SerializeAvailableSourceTypes(output, OBS_SOURCE_AUDIO,
				      {OBS_SOURCE_TYPE_INPUT,
				       OBS_SOURCE_TYPE_SCENE},
				      false);
}

void SerializeAvailableFilterSourceTypes(CefRefPtr<CefValue> &output)
{
	SerializeAvailableSourceTypes(output,
				      OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO,
				      {OBS_SOURCE_TYPE_FILTER}, false);
}

void SerializeAvailableVideoFilterSourceTypes(CefRefPtr<CefValue> &output)
{
	SerializeAvailableSourceTypes(output, OBS_SOURCE_VIDEO,
				      {OBS_SOURCE_TYPE_FILTER}, false);
}

void SerializeAvailableAudioFilterSourceTypes(CefRefPtr<CefValue> &output)
{
	SerializeAvailableSourceTypes(output, OBS_SOURCE_AUDIO,
				      {OBS_SOURCE_TYPE_FILTER}, false);
}

void SerializeExistingInputSources(CefRefPtr<CefValue> &output)
{
	SerializeExistingSources(output,
				 OBS_SOURCE_VIDEO | OBS_SOURCE_AUDIO, 0L,
				 {OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_TYPE_SCENE},
				 false);
}

void SerializeExistingVideoInputSources(CefRefPtr<CefValue> &output)
{
	SerializeExistingSources(output, OBS_SOURCE_VIDEO, 0L,
				 {OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_TYPE_SCENE},
				 false);
}

void SerializeExistingAudioInputSources(CefRefPtr<CefValue> &output)
{
	SerializeExistingSources(output, OBS_SOURCE_AUDIO, 0L,
				 {OBS_SOURCE_TYPE_INPUT, OBS_SOURCE_TYPE_SCENE},
				 false);
}

void SerializeObsSourceProperties(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return;

	std::string id = d->GetString("class");

	OBSDataAutoRelease settings = SETRACE_SCOPEREF(obs_data_create());

	if (d->HasKey("settings")) {
		if (!DeserializeObsData(d->GetValue("settings"), settings))
			return;
	}

	OBSSourceAutoRelease source = SETRACE_SCOPEREF(
		obs_source_create_private(id.c_str(), id.c_str(), settings));

	if (!source)
		return;

	auto root = CefDictionaryValue::Create();

	SerializeObsSource(source, root, false, true);

	output->SetDictionary(root);
}

void SerializeLoadedObsModules(CefRefPtr<CefValue> &output)
{
	struct local_context {
		CefRefPtr<CefListValue> list = CefListValue::Create();
	};

	local_context context;

	obs_enum_modules(
		[](void *param, obs_module_t *module) -> void {
			auto context = static_cast<local_context *>(param);

			// auto libHandle = obs_get_module_lib(module);

			std::string file_name =
				safe_str(obs_get_module_file_name(module));

			std::string binary_path = safe_str(
				obs_get_module_binary_path(module), "");

			char *binaryAbsPath =
				os_get_abs_path_ptr(binary_path.c_str());
			binary_path = safe_str(binaryAbsPath, "");
			bfree(binaryAbsPath);

			std::string name =
				safe_str(obs_get_module_name(module), "");
			std::string author =
				safe_str(obs_get_module_author(module), "");
			std::string desc = safe_str(
				obs_get_module_description(module), "");

			auto item = CefDictionaryValue::Create();

			item->SetString("binaryFileName", file_name);
			item->SetString("name", name);
			item->SetString("binaryFilePath", binary_path);
			item->SetString("author", author);
			item->SetString("description", desc);

			context->list->SetDictionary(context->list->GetSize(),
						     item);
		},
		&context);

	output->SetList(context.list);
}
