#include "SEAudioCompositionManager.hpp"
#include "SEWebsocketApiServer.hpp"
#include "SEGlobalStateManager.hpp"

//
// One of five identical copies of this translation; it lives on
// SEMessageBus now, so this takes the bus it is announcing through.
//
static void dispatch_external_event(std::shared_ptr<SEMessageBus> messageBus,
				    std::string name, std::string args)
{
	if (!messageBus)
		return;

	messageBus->NotifyExternalHostEvent(name, args);
}

//
// Takes the api server as a borrowed raw pointer: the caller is a member
// whose own reference outlives the call, and this neither owns it nor
// extends its life.
//
static void dispatch_js_event(SEWebsocketApiServer *apiServer, std::string name,
			      std::string args)
{
	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", name, args);
}

SEAudioCompositionManager::SEAudioCompositionManager(
	std::shared_ptr<SEWebsocketApiServer> apiServer,
	std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
	std::shared_ptr<SEMessageBus> messageBus)
	: m_apiServer(apiServer),
	  m_hostEventDispatcher(hostEventDispatcher),
	  m_messageBus(messageBus)
{
	//
	// Inside this constructor, so the weak self-handle in the services is
	// empty -- as on the video side, and for the same reason.
	//
	m_nativeAudioComposition =
		SEObsNativeAudioComposition::Create(CompositionServices());

	m_audioCompositionsMap[m_nativeAudioComposition->GetId()] =
		m_nativeAudioComposition;
}

SEAudioCompositionManager::~SEAudioCompositionManager()
{
	Reset();

	m_audioCompositionsMap.clear();
}

void SEAudioCompositionManager::Reset()
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	std::list<std::string> keys_to_delete;

	for (auto it : m_audioCompositionsMap) {
		if (!it.second->IsObsNativeComposition())
			keys_to_delete.push_back(it.first);
	}

	for (auto key : keys_to_delete) {
		m_audioCompositionsMap.erase(key);
	}

	dispatch_js_event(m_apiServer.get(), "hostAudioCompositionListChanged",
			  "null");
	dispatch_external_event(m_messageBus, "hostAudioCompositionListChanged", "null");
}

void SEAudioCompositionManager::
	DeserializeExistingCompositionProperties(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output)
{
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

void SEAudioCompositionManager::DeserializeComposition(
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

		auto composition = SECustomAudioComposition::Create(
			id, root->GetString("name"),
			root->GetValue("streamingAudioEncoder"),
			recordingAudioEncoders, CompositionServices());

		if (!composition)
			return;

		m_audioCompositionsMap[id] = composition;

		composition->SerializeComposition(output);

		dispatch_js_event(m_apiServer.get(),
				  "hostAudioCompositionListChanged", "null");
		dispatch_external_event(m_messageBus, "hostAudioCompositionListChanged",
					"null");
	} catch (...) {
		// Creation failed
		return;
	}
}

void SEAudioCompositionManager::SerializeAllCompositions(
	CefRefPtr<CefValue> &output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (auto kv : m_audioCompositionsMap) {
		auto composition = CefValue::Create();

		kv.second->SerializeComposition(composition);

		d->SetValue(kv.second->GetId(), composition);
	}

	output->SetDictionary(d);
}

void SEAudioCompositionManager::RemoveCompositionsByIds(
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

	dispatch_js_event(m_apiServer.get(), "hostAudioCompositionListChanged",
			  "null");
	dispatch_external_event(m_messageBus, "hostAudioCompositionListChanged", "null");

	output->SetBool(true);
}

void SEAudioCompositionManager::SerializeAvailableEncoderClasses(
	obs_encoder_type type, CefRefPtr<CefValue> &output)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	auto root = CefListValue::Create();

	const char *id;
	for (size_t i = 0; obs_enum_encoder_types(i, &id); ++i) {
		if (obs_get_encoder_type(id) != type)
			continue;

		if (m_availableEncoderClassesCache.count(id)) {
			root->SetDictionary(root->GetSize(),
					    m_availableEncoderClassesCache[id]);
			continue;
		}

		auto d = CefDictionaryValue::Create();

		d->SetString("class", id);
		d->SetString("label", obs_encoder_get_display_name(id));
		d->SetString("codec", obs_get_encoder_codec(id));
		d->SetValue("properties", SerializeObsEncoderProperties(id));

		obs_encoder_t *encoder = nullptr;

		if (type == OBS_ENCODER_VIDEO)
			encoder = SETRACE_ADDREF(obs_video_encoder_create(
				id, id, nullptr, nullptr));
		else if (type == OBS_ENCODER_AUDIO)
			encoder = SETRACE_ADDREF(obs_audio_encoder_create(
				id, id, nullptr, 0, nullptr));

		if (encoder) {
			auto defaultSettings = SETRACE_ADDREF(
				obs_encoder_get_defaults(encoder));

			if (defaultSettings) {
				d->SetValue("defaultSettings",
					    SerializeObsData(defaultSettings));

				obs_data_release(
					SETRACE_DECREF(defaultSettings));
			}

			obs_encoder_release(SETRACE_DECREF(encoder));
		}

		m_availableEncoderClassesCache[id] = d;

		root->SetDictionary(root->GetSize(), d);
	}

	output->SetList(root);
}
