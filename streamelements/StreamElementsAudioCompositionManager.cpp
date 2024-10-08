#include "StreamElementsAudioCompositionManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"

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
	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", name, args);
}

StreamElementsAudioCompositionManager::StreamElementsAudioCompositionManager()
{
	m_nativeAudioComposition =
		StreamElementsObsNativeAudioComposition::Create();

	m_audioCompositionsMap[m_nativeAudioComposition->GetId()] =
		m_nativeAudioComposition;
}

StreamElementsAudioCompositionManager::~StreamElementsAudioCompositionManager()
{
	Reset();

	m_audioCompositionsMap.clear();
}

void StreamElementsAudioCompositionManager::Reset()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	for (auto it = m_audioCompositionsMap.cbegin();
	     it != m_audioCompositionsMap.cend(); ++it) {
		if (!it->second->IsObsNativeComposition())
			m_audioCompositionsMap.erase(it->first);
	}

	dispatch_js_event("hostAudioCompositionListChanged", "null");
	dispatch_external_event("hostAudioCompositionListChanged", "null");
}

void StreamElementsAudioCompositionManager::
	DeserializeExistingCompositionProperties(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	std::string id = root->GetString("id");

	auto audioComposition = GetAudioCompositionById(id);

	if (!audioComposition.get())
		return;

	if (root->HasKey("name") && root->GetType("name") == VTYPE_STRING) {
		audioComposition->SetName(root->GetString("name"));
	}

	audioComposition->SerializeComposition(output);
}

void StreamElementsAudioCompositionManager::DeserializeComposition(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	if (!root->HasKey("name") || root->GetType("name") != VTYPE_STRING)
		return;

	if (!root->HasKey("streamingAudioEncoder"))
		return;

	std::string id = root->GetString("id");

	if (m_audioCompositionsMap.count(id))
		return;

	try {
		auto recordingAudioEncoders = CefValue::Create();
		recordingAudioEncoders->SetNull();

		if (root->HasKey("recordingAudioEncoder"))
			recordingAudioEncoders =
				root->GetValue("recordingAudioEncoder");

		auto composition = StreamElementsCustomAudioComposition::Create(
			id, root->GetString("name"),
			root->GetValue("streamingAudioEncoder"),
			recordingAudioEncoders);

		if (!composition)
			return;

		m_audioCompositionsMap[id] = composition;

		composition->SerializeComposition(output);

		dispatch_js_event("hostAudioCompositionListChanged", "null");
		dispatch_external_event("hostAudioCompositionListChanged",
					"null");
	} catch (...) {
		// Creation failed
		return;
	}
}

void StreamElementsAudioCompositionManager::SerializeAllCompositions(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (auto kv : m_audioCompositionsMap) {
		auto composition = CefValue::Create();

		kv.second->SerializeComposition(composition);

		d->SetValue(kv.second->GetId(), composition);
	}

	output->SetDictionary(d);
}

void StreamElementsAudioCompositionManager::RemoveCompositionsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

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

		if (!m_audioCompositionsMap.count(id))
			return;

		if (!m_audioCompositionsMap[id]->CanRemove())
			return;

		map[id] = true;
	}

	// Remove all valid IDs
	for (auto kv : map) {
		m_audioCompositionsMap.erase(kv.first);
	}

	dispatch_js_event("hostAudioCompositionListChanged", "null");
	dispatch_external_event("hostAudioCompositionListChanged", "null");

	output->SetBool(true);
}

void StreamElementsAudioCompositionManager::SerializeAvailableEncoderClasses(
	obs_encoder_type type, CefRefPtr<CefValue> &output)
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
			auto defaultSettings =
				obs_encoder_get_defaults(encoder);

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
