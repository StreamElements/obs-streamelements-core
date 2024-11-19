#include "StreamElementsVideoComposition.hpp"
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "deps/json11/json11.hpp"

#include "StreamElementsUtils.hpp"
#include "StreamElementsObsSceneManager.hpp"
#include "StreamElementsMessageBus.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "audio-wrapper-source.h"

#include "StreamElementsObsSceneManager.hpp"

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

static void
dispatch_scene_changed_event(StreamElementsVideoCompositionBase *self,
			     obs_scene_t *scene)
{
	auto source = obs_scene_get_source(scene);

	if (!source)
		return;

	const char *sourceName = obs_source_get_name(source);

	if (!sourceName)
		return;

	json11::Json json = json11::Json::object{
		{"name", sourceName},
		{"videoCompositionId", self->GetId()},
		{"width", (int)obs_source_get_width(source)},
		{"height", (int)obs_source_get_height(source)}};

	std::string name = "hostActiveSceneChanged";
	std::string args = json.dump();

	dispatch_js_event(name, args);
	dispatch_external_event(name, args);
}

static void
dispatch_scene_list_changed_event(StreamElementsVideoCompositionBase *self)
{
	json11::Json json = json11::Json::object{
		{"videoCompositionId", self->GetId()},
	};

	std::string name = "hostSceneListChanged";
	std::string args = json.dump();

	dispatch_js_event(name, args);
	dispatch_external_event(name, args);
}

static void
dispatch_scene_item_list_changed_event(StreamElementsVideoCompositionBase *self,
				       obs_scene_t *scene)
{
	auto source = obs_scene_get_source(scene);

	if (!source)
		return;

	const char *sourceName = obs_source_get_name(source);

	if (!sourceName)
		return;

	json11::Json json = json11::Json::object{
		{"sceneId", GetIdFromPointer(source)},
		{"videoCompositionId", self->GetId()},
	};

	std::string name = "hostSceneItemListChanged";
	std::string args = json.dump();

	dispatch_js_event(name, args);
	dispatch_external_event(name, args);
}

static void SerializeObsVideoEncoders(StreamElementsVideoCompositionBase* composition, CefRefPtr<CefDictionaryValue>& root)
{
	auto info = composition->GetCompositionInfo(nullptr);

	auto streamingVideoEncoder = info->GetStreamingVideoEncoder();

	// OBS Native composition doesn't have an encoder until we start streaming
	if (!streamingVideoEncoder)
		return;

	auto streamingVideoEncoders = CefListValue::Create();
	streamingVideoEncoders->SetDictionary(
		0, SerializeObsEncoder(streamingVideoEncoder));
	root->SetList("streamingVideoEncoders", streamingVideoEncoders);

	// Recording

	auto recordingVideoEncoder = info->GetRecordingVideoEncoder();

	if (recordingVideoEncoder &&
	    recordingVideoEncoder != streamingVideoEncoder) {
		auto recordingVideoEncoders = CefListValue::Create();

		recordingVideoEncoders->SetDictionary(
			0, SerializeObsEncoder(recordingVideoEncoder));

		root->SetList("recordingVideoEncoders", recordingVideoEncoders);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Composition base
////////////////////////////////////////////////////////////////////////////////

void StreamElementsVideoCompositionBase::handle_obs_frontend_event(
	enum obs_frontend_event event, void* data)
{
	StreamElementsVideoCompositionBase *self =
		static_cast < StreamElementsVideoCompositionBase*>(data);

	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP)
		self->HandleObsSceneCollectionCleanup();
}

void StreamElementsVideoCompositionBase::SetName(std::string name)
{
	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		if (m_name == name)
			return;

		m_name = name;
	}

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

static std::string GetUniqueSceneNameInternal(std::string name, std::vector<obs_scene_t*>& scenes)
{
	std::string result(name);

		auto hasSceneName = [&](std::string name) -> bool {
		for (auto scene : scenes) {
			if (stricmp(name.c_str(),
				    obs_source_get_name(
					    obs_scene_get_source(scene))) == 0)
				return true;
		}

		return false;
	};

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique) {
		isUnique = true;

		if (hasSceneName(result)) {
			isUnique = false;
		}

		if (!isUnique) {
			++sequence;

			char buf[32];
			sprintf(buf, "%d", sequence);
			result = name + " ";
			result += buf;
		}
	}

	return result;
}

std::string
StreamElementsVideoCompositionBase::GetUniqueSceneName(std::string name)
{
	std::string result(name);

	std::vector<obs_scene_t *> scenes;
	GetAllScenes(scenes);

	return GetUniqueSceneNameInternal(name, scenes);
}

bool StreamElementsVideoCompositionBase::SafeRemoveScene(
	obs_scene_t *sceneToRemove)
{
	if (!sceneToRemove)
		return false;

	if (sceneToRemove == GetCurrentScene()) {
		std::vector<obs_scene_t *> scenes;
		GetAllScenes(scenes);

		if (scenes.size() <= 1)
			return false;

		bool success = false;
		// Change to first available scene
		for (auto scene : scenes) {
			if (scene != sceneToRemove) {
				success = SetCurrentScene(scene);

				if (success)
					break;
			}
		}

		if (!success)
			return false;
	}

	return RemoveScene(sceneToRemove);
}

obs_sceneitem_t *StreamElementsVideoCompositionBase::GetSceneItemById(
	std::string id, obs_scene_t **result_scene, bool addRef)
{
	if (!id.size())
		return nullptr;

	auto searchPtr = GetPointerFromId(id.c_str());

	if (!searchPtr)
		return nullptr;

	std::vector<obs_scene_t *> scenes;
	GetAllScenes(scenes);

	obs_sceneitem_t *result = nullptr;

	for (auto scene : scenes) {
		ObsSceneEnumAllItems(scene,
				     [&](obs_sceneitem_t *sceneitem) -> bool {
					     if (searchPtr == sceneitem) {
						     result = sceneitem;

						     /* Found what we're looking for, stop iteration */
						     return false;
					     }

					     /* Continue or stop iteration */
					     return !result;
				     });

		if (result) {
			if (result_scene) {
				*result_scene = scene;
			}

			break;
		}
	}

	if (result && addRef) {
		obs_sceneitem_addref(result);
	}

	return result;
}

obs_sceneitem_t *
StreamElementsVideoCompositionBase::GetSceneItemById(std::string id,
						     bool addRef)
{
	return GetSceneItemById(id, nullptr, addRef);
}

obs_sceneitem_t *StreamElementsVideoCompositionBase::GetSceneItemByName(
	std::string name, obs_scene_t **result_scene, bool addRef)
{
	if (!name.size())
		return nullptr;

	std::vector<obs_scene_t *> scenes;
	GetAllScenes(scenes);

	obs_sceneitem_t *result = nullptr;

	for (auto scene : scenes) {
		ObsSceneEnumAllItems(
			scene, [&](obs_sceneitem_t *sceneitem) -> bool {
				if (stricmp(obs_source_get_name(
						    obs_sceneitem_get_source(
							    sceneitem)),
					    name.c_str()) == 0) {
					result = sceneitem;

					/* Found what we're looking for, stop iteration */
					return false;
				}

				/* Continue or stop iteration */
				return !result;
			});

		if (result) {
			if (result_scene) {
				*result_scene = scene;
			}

			break;
		}
	}

	if (result && addRef) {
		obs_sceneitem_addref(result);
	}

	return result;
}

obs_sceneitem_t *
StreamElementsVideoCompositionBase::GetSceneItemByName(std::string name,
						       bool addRef)
{
	return GetSceneItemByName(name, nullptr, addRef);
}

void StreamElementsVideoCompositionBase::SerializeTransition(
	CefRefPtr<CefValue>& output)
{
	SerializeObsTransition(GetId(), GetTransition(),
			       GetTransitionDurationMilliseconds(), output);
}

void StreamElementsVideoCompositionBase::DeserializeTransition(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	obs_source_t *transition;
	int durationMs;

	if (!DeserializeObsTransition(input, &transition, &durationMs,
				      IsObsNativeComposition()))
		return;

	SetTransition(transition);
	SetTransitionDurationMilliseconds(durationMs);

	SerializeObsTransition(GetId(), transition, durationMs, output);

	obs_source_release(transition);
}


////////////////////////////////////////////////////////////////////////////////
// OBS Main Composition
////////////////////////////////////////////////////////////////////////////////

class StreamElementsDefaultVideoCompositionInfo
	: public StreamElementsVideoCompositionBase::CompositionInfo {
private:
	StreamElementsVideoCompositionEventListener *m_listener;

public:
	StreamElementsDefaultVideoCompositionInfo(
		std::shared_ptr<StreamElementsVideoCompositionBase> owner,
		StreamElementsVideoCompositionEventListener* listener)
		: StreamElementsVideoCompositionBase::CompositionInfo(
			  owner, listener),
		  m_listener(listener)
	{
	}

	virtual ~StreamElementsDefaultVideoCompositionInfo() {}

public:
	virtual bool IsObsNative() { return true; }

	virtual obs_encoder_t *GetStreamingVideoEncoder() {
		auto output = obs_frontend_get_streaming_output();

		auto result = obs_output_get_video_encoder(output);

		obs_output_release(output);

		return result;
	}

	virtual obs_encoder_t* GetRecordingVideoEncoder() {
		auto output = obs_frontend_get_recording_output();

		auto result = obs_output_get_video_encoder(output);

		obs_output_release(output);

		return result;
	}

	virtual video_t *GetVideo() { return obs_get_video(); }

	virtual void GetVideoBaseDimensions(uint32_t *videoWidth,
					    uint32_t *videoHeight)
	{
		obs_video_info ovi;
		vec2 size;
		vec2_zero(&size);

		if (obs_get_video_info(&ovi)) {
			size.x = float(ovi.base_width);
			size.y = float(ovi.base_height);
		}

		*videoWidth = size.x;
		*videoHeight = size.y;
	}

	virtual void Render() { obs_render_main_texture(); }
};

std::shared_ptr<
	StreamElementsVideoCompositionBase::CompositionInfo>
StreamElementsObsNativeVideoComposition::GetCompositionInfo(
	StreamElementsVideoCompositionEventListener* listener)
{
	return std::make_shared<StreamElementsDefaultVideoCompositionInfo>(
		shared_from_this(), listener);
}

obs_scene_t *
StreamElementsObsNativeVideoComposition::AddScene(std::string requestName)
{
	return obs_scene_create(GetUniqueSceneName(requestName).c_str());
}

bool StreamElementsObsNativeVideoComposition::RemoveScene(
	obs_scene_t *sceneToRemove)
{
	std::vector<obs_scene_t *> scenes;

	GetAllScenes(scenes);

	for (auto item : scenes) {
		if (item == sceneToRemove) {
			obs_source_remove(obs_scene_get_source(sceneToRemove));

			return true;
		}
	}

	return false;
}

void StreamElementsObsNativeVideoComposition::SetTransition(
	obs_source_t *transition)
{
	obs_frontend_set_current_transition(transition);

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

void StreamElementsObsNativeVideoComposition::SetTransitionDurationMilliseconds(
	int duration)
{
	obs_frontend_set_transition_duration(duration);

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

obs_source_t* StreamElementsObsNativeVideoComposition::GetTransition()
{
	auto source = obs_frontend_get_current_transition();

	obs_source_release(source);

	return source;
}

bool StreamElementsObsNativeVideoComposition::SetCurrentScene(
	obs_scene_t* scene)
{
	if (!scene)
		return false;

	std::vector<obs_scene_t *> scenes;

	GetAllScenes(scenes);

	for (auto item : scenes) {
		if (item == scene) {
			obs_frontend_set_current_scene(
				obs_scene_get_source(scene));

			return true;
		}
	}

	return false;
}

void StreamElementsObsNativeVideoComposition::GetAllScenes(
	std::vector<obs_scene_t*>& result)
{
	result.clear();

	struct obs_frontend_source_list frontendScenes = {};

	obs_frontend_get_scenes(&frontendScenes);

	size_t removedCount = 0;

	for (size_t idx = 0;
	     idx < frontendScenes.sources.num && removedCount + 1 < frontendScenes.sources.num;
	     ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *source = frontendScenes.sources.array[idx];

		auto scene = obs_scene_from_source(source);

		if (scene) {
			result.push_back(scene);
		}
	}

	obs_frontend_source_list_free(&frontendScenes);
}

void StreamElementsObsNativeVideoComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	obs_video_info ovi;
	vec2 size;
	vec2_zero(&size);

	if (obs_get_video_info(&ovi)) {
		size.x = float(ovi.base_width);
		size.y = float(ovi.base_height);
	}

	auto root = CefDictionaryValue::Create();

	root->SetString("id", GetId());
	root->SetString("name", GetName());

	root->SetBool("isObsNativeComposition", true);
	root->SetBool("canRemove", CanRemove());

	auto videoFrame = CefDictionaryValue::Create();

	videoFrame->SetInt("width", int(size.x));
	videoFrame->SetInt("height", int(size.y));

	root->SetDictionary("videoFrame", videoFrame);

	SerializeObsVideoEncoders(this, root);

	output->SetDictionary(root);
}

obs_scene_t* StreamElementsObsNativeVideoComposition::GetCurrentScene()
{
	auto source = obs_frontend_get_current_scene();
	auto scene = obs_scene_from_source(source);

	obs_source_release(source);

	return scene;
}

////////////////////////////////////////////////////////////////////////////////
// Custom Views
////////////////////////////////////////////////////////////////////////////////

class StreamElementsCustomVideoCompositionInfo
	: public StreamElementsVideoCompositionBase::CompositionInfo {
private:
	StreamElementsVideoCompositionEventListener *m_listener;

	video_t *m_video = nullptr;
	obs_encoder_t *m_streamingVideoEncoder = nullptr;
	obs_encoder_t *m_recordingVideoEncoder = nullptr;
	obs_view_t *m_videoView = nullptr;
	uint32_t m_baseWidth = 1920;
	uint32_t m_baseHeight = 1080;

public:
	StreamElementsCustomVideoCompositionInfo(
		std::shared_ptr<StreamElementsVideoCompositionBase> owner,
		StreamElementsVideoCompositionEventListener *listener,
		video_t *video, uint32_t baseWidth, uint32_t baseHeight,
		obs_encoder_t *streamingVideoEncoder,
		obs_encoder_t *recordingVideoEncoder, obs_view_t *videoView)
		: StreamElementsVideoCompositionBase::CompositionInfo(owner,
								      listener),
		  m_video(video),
		  m_baseWidth(baseWidth),
		  m_baseHeight(baseHeight),
		  m_streamingVideoEncoder(streamingVideoEncoder),
		  m_recordingVideoEncoder(recordingVideoEncoder),
		  m_videoView(videoView),
		  m_listener(listener)
	{
	}

	virtual ~StreamElementsCustomVideoCompositionInfo() {}

public:
	virtual bool IsObsNative() { return false; }

	virtual obs_encoder_t *GetStreamingVideoEncoder()
	{
		return m_streamingVideoEncoder;
	}

	virtual obs_encoder_t *GetRecordingVideoEncoder()
	{
		return m_recordingVideoEncoder ? m_recordingVideoEncoder : m_streamingVideoEncoder;
	}

	virtual video_t *GetVideo() { return m_video; }

	virtual void GetVideoBaseDimensions(uint32_t *videoWidth,
					    uint32_t *videoHeight)
	{
		*videoWidth = m_baseWidth;
		*videoHeight = m_baseHeight;
	}

	virtual void Render() { obs_view_render(m_videoView); }
};

// ctor only usable by this class
StreamElementsCustomVideoComposition::StreamElementsCustomVideoComposition(
	StreamElementsCustomVideoComposition::Private, std::string id,
	std::string name, uint32_t baseWidth, uint32_t baseHeight,
	std::string streamingVideoEncoderId, obs_data_t *streamingVideoEncoderSettings,
	obs_data_t *streamingVideoEncoderHotkeyData)
	: StreamElementsVideoCompositionBase("custom_video_composition",  id, name),
	  m_baseWidth(baseWidth),
	  m_baseHeight(baseHeight),
	  m_transitionDurationMs(0)
{
	obs_video_info ovi;

	if (!obs_get_video_info(&ovi))
		throw std::exception("obs_get_video_info() failed", 1);

	m_streamingVideoEncoder = obs_video_encoder_create(
		streamingVideoEncoderId.c_str(), (name + ": streaming video encoder").c_str(),
		streamingVideoEncoderSettings, streamingVideoEncoderHotkeyData);

	if (!m_streamingVideoEncoder)
		throw std::exception("obs_video_encoder_create() failed", 2);

	m_transition = obs_source_create_private(
		"cut_transition", (name + ": transition").c_str(), nullptr);

	if (!m_transition) {
		obs_encoder_release(m_streamingVideoEncoder);
		throw std::exception("obs_source_create_private(cut_transition) failed", 2);
	}

	ovi.base_width = m_baseWidth;
	ovi.base_height = m_baseHeight;
	ovi.output_width = m_baseWidth;
	ovi.output_height = m_baseHeight;

	m_view = obs_view_create();
	m_video = obs_view_add2(m_view, &ovi);

	obs_view_set_source(m_view, 0, m_transition);

	obs_encoder_set_video(m_streamingVideoEncoder, m_video);

	auto currentScene = obs_scene_create_private(GetUniqueSceneName("Scene").c_str());
	m_currentScene = obs_scene_get_ref(currentScene);
	m_scenes.push_back(currentScene);

	m_signalHandlerData = new SESignalHandlerData(nullptr, this);
	m_signalHandlerData->AddRef();

	add_scene_signals(currentScene, m_signalHandlerData);
	add_source_signals(obs_scene_get_source(currentScene),
			   m_signalHandlerData);

	obs_transition_set(m_transition, obs_scene_get_source(currentScene));

	auto source = obs_scene_get_source(currentScene);
	if (source) {
		obs_source_inc_showing(source);
		obs_source_inc_active(source);
	}

	m_audioWrapperSource = obs_source_create_private(
		AUDIO_WRAPPER_SOURCE_ID, AUDIO_WRAPPER_SOURCE_ID, nullptr);

	auto aw = (struct audio_wrapper_info *)obs_obj_get_data(
		m_audioWrapperSource);

	aw->param = this;
	aw->target = [](void *param) -> obs_source_t* {
		StreamElementsCustomVideoComposition *self =
			reinterpret_cast<StreamElementsCustomVideoComposition *>(
				param);

		std::shared_lock<decltype(self->m_mutex)> lock(self->m_mutex);

		if (!self->m_transition)
			return nullptr;

		// This will always return the most up-to-date value of transition which is always the root of our video composition
		return obs_source_get_ref(self->m_transition);
	};

	// Assign audio wrapper source to first free audio channel
	for (uint32_t channel = 0; channel < MAX_CHANNELS; ++channel) {
		auto source = obs_get_output_source(channel);

		if (!source) {
			obs_set_output_source(channel, m_audioWrapperSource);

			break;
		} else {
			obs_source_release(source);
		}
	}
}

void StreamElementsCustomVideoComposition::SetRecordingEncoder(
	std::string recordingVideoEncoderId,
	obs_data_t *recordingVideoEncoderSettings,
	obs_data_t *recordingVideoEncoderHotkeyData)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	m_recordingVideoEncoder = obs_video_encoder_create(
		recordingVideoEncoderId.c_str(),
		(GetName() + ": recording video encoder").c_str(),
		recordingVideoEncoderSettings, recordingVideoEncoderHotkeyData);
}

StreamElementsCustomVideoComposition::~StreamElementsCustomVideoComposition()
{
	if (m_streamingVideoEncoder) {
		obs_encoder_release(m_streamingVideoEncoder);
		m_streamingVideoEncoder = nullptr;
	}

	if (m_recordingVideoEncoder) {
		obs_encoder_release(m_recordingVideoEncoder);
		m_recordingVideoEncoder = nullptr;
	}

	if (m_view) {
		obs_view_remove(m_view);
		obs_view_destroy(m_view);
		m_view = nullptr;
	}

	m_video = nullptr;

	if (m_audioWrapperSource) {
		// Release output channel
		for (uint32_t channel = 0; channel < MAX_CHANNELS; ++channel) {
			auto source = obs_get_output_source(channel);

			if (source) {
				if (source == m_audioWrapperSource) {
					obs_set_output_source(channel, nullptr);
				}

				obs_source_release(source);
			}
		}

		obs_source_release(m_audioWrapperSource);
		m_audioWrapperSource = nullptr;
	}

	if (m_transition) {
		obs_transition_set(m_transition, nullptr);
		obs_source_release(m_transition);
		m_transition = nullptr;
	}

	while (m_scenes.size()) {
		RemoveScene(m_scenes[0]);

		m_scenes.clear();
	}

	if (m_currentScene) {
		obs_scene_release(m_currentScene);
		m_currentScene = nullptr;
	}

	m_signalHandlerData->Clear();
	m_signalHandlerData->Release();
	m_signalHandlerData = nullptr;
}

std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
StreamElementsCustomVideoComposition::GetCompositionInfo(
	StreamElementsVideoCompositionEventListener *listener)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	return std::make_shared<StreamElementsCustomVideoCompositionInfo>(
		shared_from_this(), listener, m_video, m_baseWidth, m_baseHeight, m_streamingVideoEncoder, m_recordingVideoEncoder, m_view);
}

obs_scene_t *StreamElementsCustomVideoComposition::GetCurrentScene()
{
	//std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	std::shared_lock<decltype(m_currentSceneMutex)> currentSceneLock;

	return m_currentScene;
}

void StreamElementsCustomVideoComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	auto root = CefDictionaryValue::Create();

	root->SetString("id", GetId());
	root->SetString("name", GetName());

	root->SetBool("isObsNativeComposition", false);
	root->SetBool("canRemove", this->CanRemove());

	auto videoFrame = CefDictionaryValue::Create();

	videoFrame->SetInt("width", m_baseWidth);
	videoFrame->SetInt("height", m_baseHeight);

	root->SetDictionary("videoFrame", videoFrame);

	SerializeObsVideoEncoders(this, root);

	output->SetDictionary(root);
}

std::shared_ptr<StreamElementsCustomVideoComposition>
StreamElementsCustomVideoComposition::Create(
	std::string id, std::string name, uint32_t width, uint32_t height,
       CefRefPtr<CefValue> streamingVideoEncoders, CefRefPtr<CefValue> recordingVideoEncoders)
{
	if (!streamingVideoEncoders.get() ||
	    streamingVideoEncoders->GetType() != VTYPE_LIST)
		return nullptr;

	auto streamingVideoEncodersList = streamingVideoEncoders->GetList();

	// TODO: In the future we want to support multiple encoders per canvas
	if (streamingVideoEncodersList->GetSize() != 1)
		return nullptr;

	std::string streamingVideoEncoderId;
	obs_data_t *streamingVideoEncoderSettings;
	obs_data_t *streamingVideoEncoderHotkeyData = nullptr;

	// For each encoder
	{
		auto encoderRoot = streamingVideoEncodersList->GetDictionary(0);

		if (!encoderRoot->HasKey("class") ||
		    encoderRoot->GetType("class") != VTYPE_STRING)
			return nullptr;

		if (!encoderRoot->HasKey("settings") ||
		    encoderRoot->GetType("settings") != VTYPE_DICTIONARY)
			return nullptr;

		streamingVideoEncoderId = encoderRoot->GetString("class");

		streamingVideoEncoderSettings = obs_data_create();

		if (!DeserializeObsData(encoderRoot->GetValue("settings"),
					streamingVideoEncoderSettings)) {
			obs_data_release(streamingVideoEncoderSettings);

			return nullptr;
		}
	}

	std::exception_ptr exception = nullptr;
	std::shared_ptr<StreamElementsCustomVideoComposition> result = nullptr;

	try {
		result = Create(id, name, width, height,
				streamingVideoEncoderId,
				streamingVideoEncoderSettings,
				streamingVideoEncoderHotkeyData);
	} catch(...) {
		result = nullptr;

		exception = std::current_exception();
	}

	if (streamingVideoEncoderSettings)
		obs_data_release(streamingVideoEncoderSettings);

	if (streamingVideoEncoderHotkeyData)
		obs_data_release(streamingVideoEncoderHotkeyData);

	if (exception)
		std::rethrow_exception(exception);

	// Recording encoders
	if (recordingVideoEncoders.get() &&
	    recordingVideoEncoders->GetType() == VTYPE_LIST) {
		std::string recordingVideoEncoderId;
		obs_data_t *recordingVideoEncoderSettings = nullptr;
		obs_data_t *recordingVideoEncoderHotkeyData = nullptr;

		auto list = streamingVideoEncoders->GetList();

		if (list->GetSize() >= 1) {
			auto encoderRoot = list->GetDictionary(0);

			if (!encoderRoot->HasKey("class") ||
			    encoderRoot->GetType("class") != VTYPE_STRING)
				return nullptr;

			if (!encoderRoot->HasKey("settings") ||
			    encoderRoot->GetType("settings") !=
				    VTYPE_DICTIONARY)
				return nullptr;

			recordingVideoEncoderId =
				encoderRoot->GetString("class");

			recordingVideoEncoderSettings = obs_data_create();

			if (DeserializeObsData(encoderRoot->GetValue("settings"),
					       recordingVideoEncoderSettings)) {
				try {
					result->SetRecordingEncoder(
						recordingVideoEncoderId,
						recordingVideoEncoderSettings,
						recordingVideoEncoderHotkeyData);
				} catch (...) {
					result = nullptr;

					exception = std::current_exception();
				}
			}
		}

		if (recordingVideoEncoderSettings)
			obs_data_release(recordingVideoEncoderSettings);

		if (recordingVideoEncoderHotkeyData)
			obs_data_release(recordingVideoEncoderHotkeyData);

		if (exception)
			std::rethrow_exception(exception);
	}

	return result;
}

void StreamElementsCustomVideoComposition::GetAllScenes(
	std::vector<obs_scene_t *> &result)
{
	result.clear();

	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	for (auto item : m_scenes) {
		result.push_back(item);
	}
}

obs_scene_t *
StreamElementsCustomVideoComposition::AddScene(std::string requestName)
{
	auto scene = obs_scene_create_private(GetUniqueSceneName(requestName).c_str());

	obs_scene_get_ref(scene); // caller will release

	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		m_scenes.push_back(scene);
	}

	auto source = obs_scene_get_source(scene);

	add_source_signals(source, m_signalHandlerData);
	add_scene_signals(scene, m_signalHandlerData);

	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		dispatch_scene_list_changed_event(this);
	}

	return scene;
}

bool StreamElementsCustomVideoComposition::RemoveScene(obs_scene_t* scene)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	for (auto it = m_scenes.cbegin(); it != m_scenes.cend(); ++it) {
		if (scene == *it) {
			m_scenes.erase(it);

			auto source = obs_scene_get_source(scene);

			remove_source_signals(source, m_signalHandlerData);
			remove_scene_signals(scene, m_signalHandlerData);

			obs_scene_release(scene);

			dispatch_scene_list_changed_event(this);

			return true;
		}
	}

	return false;
}

void StreamElementsCustomVideoComposition::SetTransition(
	obs_source_t *transition)
{
	obs_source_t *old_transition = nullptr;
	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		old_transition = m_transition;
		m_transition = obs_source_get_ref(transition);
	}

	obs_transition_swap_begin(transition, old_transition);
	obs_view_set_source(m_view, 0, transition);
	obs_transition_swap_end(transition, old_transition);

	//obs_scene_get_ref(m_currentScene);
	obs_transition_set(old_transition, nullptr);
	obs_source_release(old_transition);

	{
		std::shared_lock<decltype(m_currentSceneMutex)> currentSceneLock(
			m_currentSceneMutex);
		obs_transition_set(transition,
				   obs_scene_get_source(m_currentScene));
	}

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

obs_source_t *StreamElementsCustomVideoComposition::GetTransition()
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	auto transition = m_transition;

	return transition;
}

void StreamElementsCustomVideoComposition::SetTransitionDurationMilliseconds(
	int duration)
{
	m_transitionDurationMs = duration > 0 ? duration : 0;

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

bool StreamElementsCustomVideoComposition::SetCurrentScene(obs_scene_t* scene)
{
	if (!scene)
		return false;

	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	for (auto item : m_scenes) {
		if (scene == item) {
			obs_scene_t *oldCurrentScene = nullptr;

			{
				std::unique_lock<decltype(m_currentSceneMutex)>
					currentSceneLock;

				if (m_currentScene == scene)
					return true;

				{
					auto source = obs_scene_get_source(
						m_currentScene);
					if (source) {
						obs_source_dec_active(source);
						obs_source_dec_showing(source);
					}
				}

				float t = obs_transition_get_time(m_transition);
				bool stillTransitioning = t < 1.0f && t > 0.0f;

				if (stillTransitioning) {
					obs_transition_set(
						m_transition,
						obs_scene_get_source(scene));
				} else {
					obs_transition_start(
						m_transition,
						OBS_TRANSITION_MODE_AUTO,
						GetTransitionDurationMilliseconds(),
						obs_scene_get_source(scene));
				}

				auto source = obs_scene_get_source(scene);
				if (source) {
					obs_source_inc_showing(source);
					obs_source_inc_active(source);
				}

				oldCurrentScene = m_currentScene;
				m_currentScene = obs_scene_get_ref(scene);
			}

			if (oldCurrentScene) {
				obs_scene_release(oldCurrentScene);
			}

			dispatch_scene_changed_event(this, scene);
			dispatch_scene_item_list_changed_event(this, scene);

			return true;
		}
	}

	return false;
}

void StreamElementsCustomVideoComposition::HandleObsSceneCollectionCleanup()
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	// Clean all owned sources here

	for (auto scene : m_scenes) {
		std::vector<obs_sceneitem_t *> scene_items;

		// Get all scene items
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
			   void *param) {
				auto sceneitems = static_cast<std::vector<obs_sceneitem_t *>*>(
					param);

				sceneitems->push_back(sceneitem);

				// Continue iteration
				return true;
			},
			&scene_items);

		for (auto sceneitem : scene_items) {
			obs_sceneitem_remove(sceneitem);
		}

		dispatch_scene_item_list_changed_event(
			this, scene);
	}

	// Clean all owned scenes here

	// Clear current transition
	// obs_transition_set(m_transition, (obs_source_t *)nullptr);

	for (auto scene : m_scenes) {
		auto source = obs_scene_get_source(scene);

		remove_source_signals(source, m_signalHandlerData);
		remove_scene_signals(scene, m_signalHandlerData);

		obs_scene_release(scene);
	}
	m_scenes.clear();

	// Recreate current scene here

	auto currentScene = obs_scene_create_private(
		GetUniqueSceneNameInternal("Scene", m_scenes).c_str());
	m_scenes.push_back(currentScene);

	{
		std::unique_lock<decltype(m_currentSceneMutex)> currentSceneLock;

		if (m_currentScene) {
			obs_scene_release(m_currentScene);
		}

		m_currentScene = obs_scene_get_ref(currentScene);
	}

	obs_transition_set(m_transition, obs_scene_get_source(currentScene));

	auto source = obs_scene_get_source(currentScene);
	if (source) {
		obs_source_inc_showing(source);
		obs_source_inc_active(source);
	}

	add_scene_signals(currentScene, m_signalHandlerData);
	add_source_signals(obs_scene_get_source(currentScene),
			   m_signalHandlerData);

	dispatch_scene_list_changed_event(this);
	dispatch_scene_changed_event(this, currentScene);
}
