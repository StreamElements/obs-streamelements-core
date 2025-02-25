#include "StreamElementsVideoCompositionManager.hpp"

#include <QDialog>
#include <QVBoxLayout>
#include "StreamElementsVideoCompositionViewWidget.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "StreamElementsBrowserDialog.hpp"

static void dispatch_external_event(std::string name, std::string args)
{
	std::string externalEventName =
		name.c_str() + 4; /* remove 'host' prefix */
	externalEventName[0] =
		tolower(externalEventName[0]); /* lower case first letter */

	StreamElementsMessageBus::GetInstance()->NotifyAllExternalEventListeners(
		StreamElementsMessageBus::DEST_ALL_EXTERNAL,
		StreamElementsMessageBus::SOURCE_APPLICATION, "OBS",
		externalEventName,
		CefParseJSON(args, JSON_PARSER_ALLOW_TRAILING_COMMAS));
}

static void dispatch_js_event(std::string name, std::string args)
{
	if (!StreamElementsGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", name, args);
}

StreamElementsVideoCompositionManager::StreamElementsVideoCompositionManager()
{
	m_nativeVideoComposition = StreamElementsObsNativeVideoComposition::Create();

	m_videoCompositionsMap[m_nativeVideoComposition->GetId()] = m_nativeVideoComposition;
}

StreamElementsVideoCompositionManager::~StreamElementsVideoCompositionManager()
{
	//Reset();

	m_videoCompositionsMap.clear();
}

void StreamElementsVideoCompositionManager::Reset()
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	std::list<std::string> keys_to_delete;
	
	for (auto pair : m_videoCompositionsMap) {
		if (!pair.second->IsObsNativeComposition())
			keys_to_delete.push_back(pair.first);
	}
	
	for (auto key : keys_to_delete) {
		m_videoCompositionsMap.erase(key);
	}

	dispatch_js_event("hostVideoCompositionListChanged", "null");
	dispatch_external_event("hostVideoCompositionListChanged", "null");
}

void StreamElementsVideoCompositionManager::
DeserializeExistingCompositionProperties(CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	std::string id = root->GetString("id");

	auto videoComposition = GetVideoCompositionById(id);

	if (!videoComposition.get())
		return;

	if (root->HasKey("name") && root->GetType("name") == VTYPE_STRING) {
		videoComposition->SetName(root->GetString("name"));
	}

	videoComposition->SerializeComposition(output);
}

void StreamElementsVideoCompositionManager::DeserializeComposition(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	if (!root->HasKey("name") || root->GetType("name") != VTYPE_STRING)
		return;

	if (!root->HasKey("streamingVideoEncoders"))
		return;

	std::string id = root->GetString("id");

	if (m_videoCompositionsMap.count(id))
		return;

	std::string referencedView = "custom";

	if (root->HasKey("view") &&
	    root->GetType("view") == VTYPE_STRING) {
		referencedView = root->GetString("view");
	}

	bool isNativeObsView = referencedView == "obs";

	std::string name = root->GetString("name");

	try {
		auto recordingVideoEncoders = CefValue::Create();
		recordingVideoEncoders->SetNull();

		if (root->HasKey("recordingVideoEncoders"))
			recordingVideoEncoders =
				root->GetValue("recordingVideoEncoders");

		auto streamingVideoEncoders =
			root->GetValue("streamingVideoEncoders");

		std::shared_ptr<StreamElementsVideoCompositionBase>
			composition = nullptr;

		if (isNativeObsView) {
			composition =
				StreamElementsObsNativeVideoCompositionWithCustomEncoders::
					Create(id, name, streamingVideoEncoders,
					       recordingVideoEncoders);
		} else {
			if (!root->HasKey("videoFrame") ||
			    root->GetType("videoFrame") != VTYPE_DICTIONARY)
				return;

			auto videoFrame = root->GetDictionary("videoFrame");

			if (!videoFrame->HasKey("width") ||
			    videoFrame->GetType("width") != VTYPE_INT)
				return;

			if (!videoFrame->HasKey("height") ||
			    videoFrame->GetType("height") != VTYPE_INT)
				return;

			composition =
				StreamElementsCustomVideoComposition::Create(
					id, name, videoFrame->GetInt("width"),
					videoFrame->GetInt("height"),
					streamingVideoEncoders,
					recordingVideoEncoders);
		}

		if (!composition)
			return;

		m_videoCompositionsMap[id] = composition;

		composition->SerializeComposition(output);

		dispatch_js_event("hostVideoCompositionListChanged", "null");
		dispatch_external_event("hostVideoCompositionListChanged",
					"null");
	} catch (...) {
		// Creation failed
		return;
	}
}

void StreamElementsVideoCompositionManager::SerializeAllCompositions(
	CefRefPtr<CefValue>& output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (auto kv : m_videoCompositionsMap) {
		auto composition = CefValue::Create();

		kv.second->SerializeComposition(composition);

		d->SetValue(kv.second->GetId(), composition);
	}

	output->SetDictionary(d);
}

void StreamElementsVideoCompositionManager::RemoveCompositionsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	if (input->GetType() != VTYPE_LIST)
		return;

	auto list = input->GetList();

	std::map<std::string, bool> map;

	// Check if all IDs are valid
	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return;

		std::string id = list->GetString(i);

		if (!id.size())
			return;

		if (!m_videoCompositionsMap.count(id))
			return;

		if (!m_videoCompositionsMap[id]->CanRemove())
			return;

		map[id] = true;
	}

	// Remove all valid IDs
	for (auto kv : map) {
		m_videoCompositionsMap.erase(kv.first);
	}

	dispatch_js_event("hostVideoCompositionListChanged", "null");
	dispatch_external_event("hostVideoCompositionListChanged", "null");

	output->SetBool(true);
}

void StreamElementsVideoCompositionManager::SerializeAvailableEncoderClasses(
	obs_encoder_type type,
	CefRefPtr<CefValue>& output)
{
	auto root = CefListValue::Create();

	const char *id;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); ++i) {
		if (obs_get_encoder_type(id) != type)
			continue;

		auto d = CefDictionaryValue::Create();

		d->SetString("class", id);
		d->SetString("label", obs_encoder_get_display_name(id));
		d->SetString("codec", obs_get_encoder_codec(id));
		d->SetValue("properties", SerializeObsEncoderProperties(id));

		obs_encoder_t *encoder = nullptr;

		if (type == OBS_ENCODER_VIDEO)
			encoder = obs_video_encoder_create(id, id, nullptr,
							   nullptr);
		else if (type == OBS_ENCODER_AUDIO)
			encoder = obs_audio_encoder_create(id, id, nullptr, 0,
							   nullptr);

		if (encoder) {
			auto defaultSettings = obs_encoder_get_defaults(encoder);

			if (defaultSettings) {
				d->SetValue("defaultSettings",
					    SerializeObsData(defaultSettings));

				obs_data_release(defaultSettings);
			}

			obs_encoder_release(encoder);
		}

		root->SetDictionary(root->GetSize(), d);
	}

	output->SetList(root);
}

void StreamElementsVideoCompositionManager::SerializeAvailableTransitionClasses(
	CefRefPtr<CefValue>& output)
{
	auto list = CefListValue::Create();

	/*
	const char *id;
	for (size_t idx = 0; obs_enum_transition_types(idx, &id); ++idx) {
		auto d = CefDictionaryValue::Create();

		auto props = obs_get_source_properties(id);
		auto propsValue = CefValue::Create();
		SerializeObsProperties(props, propsValue);
		obs_properties_destroy(props);

		auto defaultsData = obs_get_source_defaults(id);
		d->SetValue("defaultSettings", SerializeObsData(defaultsData));
		obs_data_release(defaultsData);

		d->SetString("class", id);
		d->SetValue("properties", propsValue);

		d->SetString("label", obs_source_get_display_name(id));


		list->SetDictionary(list->GetSize(), d);
	}
	*/

	obs_frontend_source_list l = {};
	obs_frontend_get_transitions(&l);

	for (size_t idx = 0; idx < l.sources.num; ++idx) {
		auto source = l.sources.array[idx];

		std::string id = obs_source_get_id(source);

		auto d = CefDictionaryValue::Create();

		auto props = obs_get_source_properties(id.c_str());
		auto propsValue = CefValue::Create();
		SerializeObsProperties(props, propsValue);
		obs_properties_destroy(props);

		auto defaultsData = obs_get_source_defaults(id.c_str());
		d->SetValue("defaultSettings", SerializeObsData(defaultsData));
		obs_data_release(defaultsData);

		d->SetString("class", id);
		d->SetValue("properties", propsValue);

		d->SetString("label", obs_source_get_display_name(id.c_str()));

		list->SetDictionary(list->GetSize(), d);
	}

	obs_frontend_source_list_free(&l);

	output->SetList(list);
}
