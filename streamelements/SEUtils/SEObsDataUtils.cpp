#include "SEObsDataUtils.hpp"

#include "SEStringUtils.hpp"

#include "../SETrace.hpp"

// OBSDataAutoRelease and friends, the RAII wrappers
#include <obs.hpp>
#include <obs-module.h>

#include <util/platform.h>

#include <algorithm>
#include <map>
#include <mutex>
#include <cstdint>
#include <regex>
#include <shared_mutex>

static inline const char *safe_str(const char *s,
				   const char *defaultValue = "(NULL)")
{
	if (s == NULL)
		return defaultValue;
	else
		return s;
}

bool DeserializeObsSourceFilters(obs_source_t *source,
				 CefRefPtr<CefValue> filtersValue)
{
	if (!filtersValue.get())
		return false;

	if (filtersValue->GetType() != VTYPE_LIST)
		return false;

	auto filtersList = filtersValue->GetList();

	std::map<int, OBSSourceAutoRelease> filtersMap;
	int maxOrder = -1;
	std::vector<int> orderIndexes;

	bool result = true;

	for (size_t i = 0; i < filtersList->GetSize(); ++i) {
		if (filtersList->GetType(i) != VTYPE_DICTIONARY)
			return false;

		auto d = filtersList->GetDictionary(i);

		if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
			return false;

		std::string sourceType = d->GetString("class");
		std::string sourceName =
			d->HasKey("name") && d->GetType("name") == VTYPE_STRING
				? d->GetString("name")
				: obs_source_get_display_name(
					  sourceType.c_str());

		int order = d->HasKey("order") &&
					    d->GetType("order") == VTYPE_INT
				    ? order = d->GetInt("order")
				    : filtersList->GetSize() - i - 1;

		if (order < 0)
			return false;

		if (order > maxOrder)
			maxOrder = order;

		if (filtersMap.count(order)) {
			// duplicate "order" value
			result = false;
			break;
		}

		orderIndexes.push_back(order);

		OBSDataAutoRelease settings =
			SETRACE_SCOPEREF(obs_data_create());

		if (d->HasKey("settings") &&
		    d->GetType("settings") == VTYPE_DICTIONARY) {
			if (!DeserializeObsData(d->GetValue("settings"),
						settings))
				return false;
		}

		filtersMap[order] = SETRACE_SCOPEREF(obs_source_create_private(
			sourceType.c_str(), sourceName.c_str(), settings));

		if (!filtersMap[order]) {
			// Filter not created
			result = false;
			break;
		}
	}

	if (result) {
		// Sort order indexes ascending
		std::sort(orderIndexes.begin(), orderIndexes.end());

		// Remove existing filters if any
		obs_source_enum_filters(
			source,
			[](obs_source_t *source, obs_source_t *filter,
			   void *params) {
				obs_source_filter_remove(source, filter);
			},
			nullptr);

		// Add new filters
		for (auto i : orderIndexes) {
			obs_source_filter_add(source, filtersMap[i]);
		}

		// Set new filters order
		for (auto i : orderIndexes) {
			obs_source_filter_set_index(source, filtersMap[i], i);
		}
	}

	return result;
}
CefRefPtr<CefListValue> SerializeObsSourceFilters(obs_source_t *source,
						  bool serializeProperties)
{
	if (!source)
		return CefListValue::Create();

	struct local_filters_context_t {
		CefRefPtr<CefListValue> items = CefListValue::Create();
		std::vector<obs_source_t *> filters;

		~local_filters_context_t()
		{
			for (const auto &filter : filters) {
				obs_source_release(SETRACE_DECREF(filter));
			}
		}
	};

	local_filters_context_t context;
	obs_source_enum_filters(
		source,
		[](obs_source_t *source, obs_source_t *filter,
		   void *params) -> void {
			auto context =
				static_cast<local_filters_context_t *>(params);

			context->filters.push_back(
				SETRACE_ADDREF(obs_source_get_ref(filter)));
		},
		&context);

	for (size_t i = 0; i < context.filters.size(); ++i) {
		auto d = CefDictionaryValue::Create();

		SerializeObsSource(context.filters[i], d, true,
				   serializeProperties);

		context.items->SetDictionary(context.items->GetSize(), d);
	}

	return context.items;
}
void SerializeObsSource(obs_source_t *source, CefRefPtr<CefDictionaryValue> dic,
			bool isExistingSource, bool serializeProperties)
{
	if (!source)
		return;

	if (!dic.get())
		return;

	obs_source_type sourceType = obs_source_get_type(source);
	std::string sourceId = safe_str(obs_source_get_id(source));
	std::string unversioned_id =
		safe_str(obs_source_get_unversioned_id(source));

	uint32_t sourceCaps = obs_get_source_output_flags(sourceId.c_str());

	// Set codec dictionary properties
	dic->SetString("class", sourceId);

	dic->SetString("unversionedClass", unversioned_id);
	dic->SetBool("isDeprecated", (sourceCaps & OBS_SOURCE_DEPRECATED) != 0);

	if (isExistingSource) {
		dic->SetString("id", GetIdFromPointer(source));
		dic->SetString("name", safe_str(obs_source_get_name(source)));
	} else {
		dic->SetString("id", sourceId);
		dic->SetString("name", safe_str(obs_source_get_display_name(
					       sourceId.c_str())));
	}

	dic->SetString("className",
		       safe_str(obs_source_get_display_name(sourceId.c_str())));

	dic->SetBool("hasVideo",
		     (sourceCaps & OBS_SOURCE_VIDEO) == OBS_SOURCE_VIDEO);
	dic->SetBool("hasAudio",
		     (sourceCaps & OBS_SOURCE_AUDIO) == OBS_SOURCE_AUDIO);

	// Compare sourceId to known video capture devices
	dic->SetBool("isVideoCaptureDevice",
		     sourceId == "dshow_input" || sourceId == "decklink-input");

	// Compare sourceId to known game capture source
	dic->SetBool("isGameCaptureDevice", sourceId == "game_capture");

	// Compare sourceId to known browser source
	dic->SetBool("isBrowserSource", sourceId == "browser_source");

	dic->SetBool("isSceneSource", obs_source_is_scene(source));

	dic->SetBool("isGroupSource", obs_source_is_group(source));

	dic->SetBool("isFilterSource", sourceType == OBS_SOURCE_TYPE_FILTER);
	dic->SetBool("isInputSource", sourceType == OBS_SOURCE_TYPE_INPUT);
	dic->SetBool("isTransitionSource",
		     sourceType == OBS_SOURCE_TYPE_TRANSITION);
	dic->SetBool("isSceneSource", sourceType == OBS_SOURCE_TYPE_SCENE);

	OBSDataAutoRelease settings =
		SETRACE_SCOPEREF(obs_source_get_settings(source));

	if (isExistingSource) {
		dic->SetValue("settings", SerializeObsData(settings));
	}

	OBSDataAutoRelease defaultSettings =
		SETRACE_SCOPEREF(obs_get_source_defaults(sourceId.c_str()));

	dic->SetValue("defaultSettings", SerializeObsData(defaultSettings));

	if (serializeProperties) {
		auto properties = SETRACE_ADDREF(obs_source_properties(source));

		if (properties) {
			obs_properties_apply_settings(properties, settings);

			auto propertiesVal = CefValue::Create();
			SerializeObsProperties(properties, propertiesVal);

			dic->SetValue("properties", propertiesVal);

			obs_properties_destroy(SETRACE_DECREF(properties));
		} else {
			dic->SetList("properties", CefListValue::Create());
		}
	}

	if (!isExistingSource)
		return;

	if (sourceType == OBS_SOURCE_TYPE_FILTER) {
		auto parentSource = obs_filter_get_parent(source);

		if (parentSource) {
			dic->SetInt("order", obs_source_filter_get_index(
						     parentSource, source));
		}
	}

	if (sourceType == OBS_SOURCE_TYPE_FILTER)
		return;

	dic->SetList("filters",
		     SerializeObsSourceFilters(source, serializeProperties));
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
void SerializeAvailableInputSourceTypes(
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
void SerializeExistingInputSources(
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
bool SerializeObsProperty(obs_property_t *prop, CefRefPtr<CefValue> &output)
{
	if (!prop)
		return false;

	if (!obs_property_visible(prop))
		return false;

	auto safe_str = [](const char *input) -> std::string {
		if (!input)
			return "";
		else
			return input;
	};

	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();

	obs_property_type type_id = obs_property_get_type(prop);
	std::string dataType = "";
	std::string controlType = "";
	std::string controlMode = "";
	std::string valueFormat = "";
	std::string propDefault = "";

	switch (type_id) {
	case OBS_PROPERTY_BOOL:
		dataType = "bool";
		controlType = "checkbox";
		controlMode = "";
		break;

	case OBS_PROPERTY_INT:
		dataType = "integer";
		controlType = "number";

		switch (obs_property_int_type(prop)) {
		case OBS_NUMBER_SCROLLER:
			controlMode = "scroller";
			break;
		case OBS_NUMBER_SLIDER:
			controlMode = "slider";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_FLOAT:
		dataType = "float";
		controlType = "number";

		switch (obs_property_float_type(prop)) {
		case OBS_NUMBER_SCROLLER:
			controlMode = "scroller";
			break;
		case OBS_NUMBER_SLIDER:
			controlMode = "slider";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_TEXT:
		dataType = "string";
		controlType = "text";
		controlMode = "";

		switch (obs_property_text_type(prop)) {
		case OBS_TEXT_DEFAULT:
			controlMode = "text";
			break;
		case OBS_TEXT_PASSWORD:
			controlMode = "password";
			break;
		case OBS_TEXT_MULTILINE:
			controlMode = "textarea";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_PATH:
		dataType = "string";
		controlType = "path";

		switch (obs_property_path_type(prop)) {
		case OBS_PATH_FILE:
			controlMode = "open";
			break;
		case OBS_PATH_FILE_SAVE:
			controlMode = "save";
			break;
		case OBS_PATH_DIRECTORY:
			controlMode = "folder";
			break;
		default:
			return false;
		}

		valueFormat = safe_str(obs_property_path_filter(prop));
		propDefault = safe_str(obs_property_path_default_path(prop));
		break;

	case OBS_PROPERTY_LIST:
		controlType = "select";

		switch (obs_property_list_type(prop)) {
		case OBS_COMBO_TYPE_EDITABLE:
			controlMode = "dynamic";
			break;
		case OBS_COMBO_TYPE_LIST:
			controlMode = "static";
			break;
		default:
			return false;
		}

		switch (obs_property_list_format(prop)) {
		case OBS_COMBO_FORMAT_INT:
			dataType = "integer";
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			dataType = "float";
			break;
		case OBS_COMBO_FORMAT_STRING:
			dataType = "string";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_COLOR:
		dataType = "string";
		controlType = "text";
		controlMode = "color";
		break;

	case OBS_PROPERTY_BUTTON:
		return false;

	case OBS_PROPERTY_FONT:
		dataType = "font";
		controlType = "font";
		controlMode = "";
		break;

	case OBS_PROPERTY_EDITABLE_LIST:
		dataType = "array";
		controlType = "list";
		controlMode = "dynamic";
		valueFormat = safe_str(obs_property_editable_list_filter(prop));
		propDefault =
			safe_str(obs_property_editable_list_default_path(prop));
		break;

	case OBS_PROPERTY_FRAME_RATE:
		dataType = "frame_rate";
		controlType = "frame_rate";
		controlMode = "";
		break;

	case OBS_PROPERTY_GROUP:
		dataType = "group";
		controlType = "group";

		switch (obs_property_group_type(prop)) {
		case OBS_GROUP_NORMAL:
			controlMode = "normal";
			break;

		case OBS_GROUP_CHECKABLE:
			controlMode = "checkable";
			break;

		default:
			return false;
		}
		break;

	default:
		return false;
	}

	root->SetString("name", safe_str(obs_property_name(prop)));
	root->SetString("label", safe_str(obs_property_description(prop)));
	root->SetString("description",
			safe_str(obs_property_long_description(prop)));
	root->SetString("dataType", dataType);
	root->SetString("controlType", controlType);

	if (controlMode.size())
		root->SetString("controlMode", controlMode);

	if (propDefault.size())
		root->SetString("defaultValue", propDefault);

	if (valueFormat.size())
		root->SetString("valueFormat", valueFormat);

	if (type_id == OBS_PROPERTY_LIST) {
		const size_t count = obs_property_list_item_count(prop);
		const obs_combo_format comboFormat =
			obs_property_list_format(prop);

		CefRefPtr<CefListValue> items = CefListValue::Create();

		for (size_t index = 0; index < count; ++index) {
			CefRefPtr<CefDictionaryValue> item =
				CefDictionaryValue::Create();

			item->SetString("name",
					safe_str(obs_property_list_item_name(
						prop, index)));

			item->SetBool("enabled",
				      !obs_property_list_item_disabled(prop,
								       index));

			switch (comboFormat) {
			case OBS_COMBO_FORMAT_INT:
				//
				// obs_property_list_item_int returns long long
				// and CefDictionaryValue::SetInt takes an int.
				// The truncation predates this file; the cast
				// only makes it visible, now that this code is
				// no longer shielded by a warning suppression
				// leaking from elsewhere in the old
				// translation unit's include chain.
				//
				item->SetInt("value",
					     (int)obs_property_list_item_int(
						     prop, index));
				break;
			case OBS_COMBO_FORMAT_FLOAT:
				item->SetDouble("value",
						obs_property_list_item_float(
							prop, index));
				break;
			case OBS_COMBO_FORMAT_STRING:
				item->SetString(
					"value",
					safe_str(obs_property_list_item_string(
						prop, index)));
				break;
			default:
				continue;
			}

			items->SetDictionary(items->GetSize(), item);
		}

		root->SetList("items", items);
	} else if (type_id == OBS_PROPERTY_EDITABLE_LIST) {
		/* Nothing to do */
	} else if (type_id == OBS_PROPERTY_GROUP) {
		obs_properties_t *props = obs_property_group_content(
			prop); // Does not increment refcount

		if (props) {
			CefRefPtr<CefListValue> items = CefListValue::Create();

			obs_property_t *itemProp = obs_properties_first(props);

			do {
				if (!itemProp)
					break;

				CefRefPtr<CefValue> item = CefValue::Create();

				if (SerializeObsProperty(itemProp, item)) {
					items->SetValue(items->GetSize(), item);
				}
			} while (obs_property_next(&prop));

			root->SetList("items", items);
		}
	}

	output->SetDictionary(root);

	return true;
}
bool SerializeObsProperties(obs_properties_t *props,
			    CefRefPtr<CefValue> &output)
{
	auto list = CefListValue::Create();

	obs_property_t *prop = obs_properties_first(props);

	do {
		if (!prop)
			break;

		CefRefPtr<CefValue> item = CefValue::Create();

		if (SerializeObsProperty(prop, item)) {
			list->SetValue(list->GetSize(), item);
		}
	} while (obs_property_next(&prop));

	output->SetList(list);

	return true;
}
bool DeserializeObsData(CefRefPtr<CefValue> input, obs_data_t *data)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	std::string json = CefWriteJSON(input, JSON_WRITER_DEFAULT);

	OBSDataAutoRelease parsed_data =
		SETRACE_SCOPEREF(obs_data_create_from_json(json.c_str()));

	if (!parsed_data)
		return false;

	obs_data_apply(data, parsed_data);

	return true;
}
CefRefPtr<CefValue> SerializeObsData(obs_data_t *data)
{
	if (data) {
		static std::shared_mutex mutex;

		std::unique_lock guard(
			mutex); // obs_data_get_json is not thread_safe for the same data container

		const char *json =
			SETRACE_NOREF(obs_data_get_json_with_defaults(data));

		if (json) {
			return CefParseJSON(json,
					    JSON_PARSER_ALLOW_TRAILING_COMMAS);
		}
	}

	auto result = CefValue::Create();

	result->SetNull();

	return result;
}
uint32_t GetInt32FromAlignmentId(std::string alignment)
{
	uint32_t result = 0;

	if (std::regex_search(alignment, std::regex("left")))
		result |= OBS_ALIGN_LEFT;

	if (std::regex_search(alignment, std::regex("right")))
		result |= OBS_ALIGN_RIGHT;

	if (std::regex_search(alignment, std::regex("top")))
		result |= OBS_ALIGN_TOP;

	if (std::regex_search(alignment, std::regex("bottom")))
		result |= OBS_ALIGN_BOTTOM;

	return result;
}
std::string GetAlignmentIdFromInt32(uint32_t a)
{
	std::string h = "center";
	std::string v = "center";

	if (a & OBS_ALIGN_LEFT) {
		h = "left";
	} else if (a & OBS_ALIGN_RIGHT) {
		h = "right";
	}

	if (a & OBS_ALIGN_TOP) {
		v = "top";
	} else if (a & OBS_ALIGN_BOTTOM) {
		v = "bottom";
	}

	if (h == v) {
		return "center";
	} else {
		return v + "_" + h;
	}
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
