#include "StreamElementsVideoComposition.hpp"
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "deps/json11/json11.hpp"

#include "StreamElementsUtils.hpp"
#include "StreamElementsObsSceneManager.hpp"
#include "StreamElementsMessageBus.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "audio-wrapper-source.h"

#include <exception>

#ifndef _WIN32
#include <strings.h>
#endif

static obs_scene_t *scene_create_private_with_custom_size(std::string name,
							  uint32_t width,
							  uint32_t height)
{
	//
	// A newly created scene assumes OBS default output video mix's dimensions as indicated here:
	// https://github.com/obsproject/obs-studio/blob/master/libobs/obs-scene.c#L1458-L1476
	//
	// To mitigate this, we apply custom size to the newly created scene through settings, and
	// apply it via a call to obs_source_load2(), which executes this piece of code:
	// https://github.com/obsproject/obs-studio/blob/master/libobs/obs-scene.c#L1269-L1300
	//
	// This ensures that a scene is always sized the same as our video view base width & height.
	//

	auto scene = obs_scene_create_private(name.c_str());

	if (!scene)
		return nullptr;

	auto source = obs_scene_get_source(scene);

	if (!source) {
		obs_scene_release(scene);

		return nullptr;
	}

	OBSDataAutoRelease settings = obs_source_get_settings(source);

	obs_data_set_bool(settings, "custom_size", true);
	obs_data_set_int(settings, "cx", width);
	obs_data_set_int(settings, "cy", height);

	obs_source_update(source, settings);
	obs_source_load2(source);

	return scene;
}

static void transition_set_defaults(obs_source_t *transition, uint32_t width,
				    uint32_t height)
{
	//obs_transition_set_size(transition, width, height);
	//obs_transition_set_alignment(transition, OBS_ALIGN_TOP | OBS_ALIGN_LEFT);
	//obs_transition_set_scale_type(transition, OBS_TRANSITION_SCALE_ASPECT);
}

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
		{"sceneId", GetIdFromPointer(source)},
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
dispatch_scenes_reset_begin_event(StreamElementsVideoCompositionBase *self)
{
	json11::Json json = json11::Json::object{
		{"videoCompositionId", self->GetId()},
	};

	std::string name = "hostVideoCompositionBeforeScenesReset";
	std::string args = json.dump();

	dispatch_js_event(name, args);
	dispatch_external_event(name, args);
}

static void
dispatch_scenes_reset_end_event(StreamElementsVideoCompositionBase *self)
{
	json11::Json json = json11::Json::object{
		{"videoCompositionId", self->GetId()},
	};

	std::string name = "hostVideoCompositionAfterScenesReset";
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

static bool
ParseEncodersList(CefRefPtr<CefValue> encodersList,
		  std::vector<std::string> &encoderIds,
		  std::vector<OBSDataAutoRelease> &encoderSettings,
		  std::vector<OBSDataAutoRelease> &encoderHotkeyData)
{
	if (!encodersList.get() || encodersList->GetType() != VTYPE_LIST)
		return false;

	auto streamingVideoEncodersList = encodersList->GetList();

	if (streamingVideoEncodersList->GetSize() < 1)
		return false;

	// For each encoder
	for (size_t idx = 0; idx < streamingVideoEncodersList->GetSize();
	     ++idx) {
		auto encoderRoot =
			streamingVideoEncodersList->GetDictionary(idx);

		if (!encoderRoot->HasKey("class") ||
		    encoderRoot->GetType("class") != VTYPE_STRING)
			break;

		if (!encoderRoot->HasKey("settings") ||
		    encoderRoot->GetType("settings") != VTYPE_DICTIONARY)
			break;

		auto settings = obs_data_create();

		if (!DeserializeObsData(encoderRoot->GetValue("settings"),
					settings)) {
			obs_data_release(settings);

			break;
		}

		encoderIds.push_back(encoderRoot->GetString("class"));

		encoderSettings.push_back(settings);
		encoderHotkeyData.push_back(nullptr);
	}

	return encoderIds.size() > 0;
}

static void SerializeObsVideoEncoders(StreamElementsVideoCompositionBase* composition, CefRefPtr<CefDictionaryValue>& root)
{
	auto info = composition->GetCompositionInfo(nullptr, "SerializeObsVideoEncoders");

	auto streamingVideoEncoders = CefListValue::Create();

	for (size_t idx = 0;; ++idx) {
		OBSEncoderAutoRelease streamingVideoEncoder =
			info->GetStreamingVideoEncoderRef(idx);

		// OBS Native composition doesn't have an encoder until we start streaming
		if (!streamingVideoEncoder)
			break;

		streamingVideoEncoders->SetDictionary(
			streamingVideoEncoders->GetSize(),
			SerializeObsEncoder(streamingVideoEncoder));
	}

	if (streamingVideoEncoders->GetSize() == 0)
		return;

	root->SetList("streamingVideoEncoders", streamingVideoEncoders);

	// Recording

	bool allEqual = true;

	auto recordingVideoEncoders = CefListValue::Create();

	for (size_t idx = 0;; ++idx) {
		OBSEncoderAutoRelease recordingVideoEncoder =
			info->GetRecordingVideoEncoderRef(idx);

		if (!recordingVideoEncoder)
			break;

		OBSEncoderAutoRelease streamingVideoEncoder =
			info->GetRecordingVideoEncoderRef(idx);

		if (!streamingVideoEncoder ||
		    streamingVideoEncoder.Get() !=
			    recordingVideoEncoder.Get()) {
			allEqual = false;
		}

		recordingVideoEncoders->SetDictionary(
			recordingVideoEncoders->GetSize(),
			SerializeObsEncoder(recordingVideoEncoder));
	}

	if (recordingVideoEncoders->GetSize() > 0 && !allEqual) {
		root->SetList("recordingVideoEncoders", recordingVideoEncoders);
	}
}

////////////////////////////////////////////////////////////////////////////////
// Composition base
////////////////////////////////////////////////////////////////////////////////

void StreamElementsVideoCompositionBase::handle_obs_frontend_event(
	enum obs_frontend_event event, void* data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	StreamElementsVideoCompositionBase *self =
		static_cast < StreamElementsVideoCompositionBase*>(data);

	switch (event) {
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGING:
		self->HandleObsSceneCollectionCleanup();
		break;
	default:
		break;
	}
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

static std::string GetUniqueSceneNameInternal(std::string name, StreamElementsVideoCompositionBase::scenes_t& scenes)
{
	std::string result(name);

		auto hasSceneName = [&](std::string name) -> bool {
		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			if (strcasecmp(name.c_str(),
				    obs_source_get_name(
					    obs_scene_get_source(*it))) == 0)
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

static std::string
GetUniqueSceneNameInternal(std::string name,
			   std::vector<obs_scene_t*> &scenes)
{
	std::string result(name);

	auto hasSceneName = [&](std::string name) -> bool {
		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			if (strcasecmp(name.c_str(),
				    obs_source_get_name(
					    obs_scene_get_source(*it))) == 0)
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

	scenes_t scenes;
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		GetAllScenesInternal(scenes);
	}

	return GetUniqueSceneNameInternal(name, scenes);
}

bool StreamElementsVideoCompositionBase::SafeRemoveScene(
	obs_scene_t *sceneToRemove)
{
	if (!sceneToRemove)
		return false;

	if (sceneToRemove == GetCurrentScene()) {
		scenes_t scenes;
		GetAllScenesInternal(scenes);

		if (scenes.size() <= 1)
			return false;

		bool success = false;
		// Change to first available scene
		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			if (*it != sceneToRemove) {
				success = SetCurrentScene(*it);

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

	scenes_t scenes;
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		GetAllScenesInternal(scenes);
	}

	obs_sceneitem_t *result = nullptr;

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		ObsSceneEnumAllItems(*it,
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
				*result_scene = *it;
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

	scenes_t scenes;
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		GetAllScenesInternal(scenes);
	}

	obs_sceneitem_t *result = nullptr;

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		ObsSceneEnumAllItems(
			*it, [&](obs_sceneitem_t *sceneitem) -> bool {
				if (strcasecmp(obs_source_get_name(
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
				*result_scene = *it;
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
		StreamElementsVideoCompositionEventListener *listener,
		std::string holder)
		: StreamElementsVideoCompositionBase::CompositionInfo(
			  owner, listener, holder),
		  m_listener(listener)
	{
	}

	virtual ~StreamElementsDefaultVideoCompositionInfo() {}

protected:
	virtual obs_encoder_t *GetStreamingVideoEncoder(size_t index)
	{
		if (!obs_frontend_streaming_active())
			return nullptr;

		auto output = obs_frontend_get_streaming_output();

		if (!output)
			return nullptr;

		auto result = obs_output_get_video_encoder2(output, index);

		obs_output_release(output);

		return result;
	}

	virtual obs_encoder_t *GetRecordingVideoEncoder(size_t index)
	{
		obs_encoder_t *result = nullptr;

		if (!result && obs_frontend_recording_active()) {
			auto output = obs_frontend_get_recording_output();

			if (output) {
				result = obs_output_get_video_encoder2(output, index);

				obs_output_release(output);
			}
		}

		if (!result && obs_frontend_replay_buffer_active()) {
			auto output = obs_frontend_get_replay_buffer_output();

			if (output) {
				result = obs_output_get_video_encoder(output);

				obs_output_release(output);
			}
		}

		if (!result && obs_frontend_streaming_active()) {
			result = GetStreamingVideoEncoder(index);
		}

		return result;
	}

public:
	virtual bool IsObsNative() { return true; }

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
	StreamElementsVideoCompositionEventListener* listener, std::string holder)
{
	return std::make_shared<StreamElementsDefaultVideoCompositionInfo>(
		shared_from_this(), listener, holder);
}

obs_scene_t *
StreamElementsObsNativeVideoComposition::AddScene(std::string requestName)
{
	return obs_scene_create(GetUniqueSceneName(requestName).c_str());
}

bool StreamElementsObsNativeVideoComposition::RemoveScene(
	obs_scene_t *sceneToRemove)
{
	scenes_t scenes;

	GetAllScenesInternal(scenes);

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		if (*it == sceneToRemove) {
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

	scenes_t scenes;

	GetAllScenesInternal(scenes);

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		if (*it == scene) {
			obs_frontend_set_current_scene(
				obs_scene_get_source(scene));

			return true;
		}
	}

	return false;
}

void StreamElementsObsNativeVideoComposition::GetAllScenesInternal(
	scenes_t &result)
{
	result.clear();

	struct obs_frontend_source_list frontendScenes = {};

	obs_frontend_get_scenes(&frontendScenes);

	for (size_t idx = 0;
	     idx < frontendScenes.sources.num;
	     ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *source = frontendScenes.sources.array[idx];

		auto scene = obs_scene_from_source(source);

		if (scene) {
			result.push_back(obs_scene_get_ref(scene));
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

obs_scene_t *StreamElementsObsNativeVideoComposition::GetCurrentSceneRef()
{
	auto source = obs_frontend_get_current_scene();
	auto scene = obs_scene_from_source(source);

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
	std::vector<obs_encoder_t *> m_streamingVideoEncoders;
	std::vector<obs_encoder_t *> m_recordingVideoEncoders;
	obs_view_t *m_videoView = nullptr;
	uint32_t m_baseWidth = 1920;
	uint32_t m_baseHeight = 1080;

	std::shared_ptr<StreamElementsVideoCompositionBase> m_compositionOwner =
		nullptr;

public:
	StreamElementsCustomVideoCompositionInfo(
		std::shared_ptr<StreamElementsVideoCompositionBase> owner,
		StreamElementsVideoCompositionEventListener *listener,
		std::string holder,
		video_t *video, uint32_t baseWidth, uint32_t baseHeight,
		std::vector<obs_encoder_t *> streamingVideoEncoders,
		std::vector<obs_encoder_t *> recordingVideoEncoders,
		obs_view_t *videoView)
		: StreamElementsVideoCompositionBase::CompositionInfo(
			  owner, listener, holder),
		  m_compositionOwner(owner),
		  m_video(video),
		  m_baseWidth(baseWidth),
		  m_baseHeight(baseHeight),
		  m_videoView(videoView),
		  m_listener(listener)
	{
		for (auto encoder : streamingVideoEncoders) {
			m_streamingVideoEncoders.push_back(
				obs_encoder_get_ref(encoder));
		}

		for (auto encoder : recordingVideoEncoders) {
			m_recordingVideoEncoders.push_back(
				obs_encoder_get_ref(encoder));
		}
	}

	virtual ~StreamElementsCustomVideoCompositionInfo()
	{
		for (auto encoder : m_streamingVideoEncoders) {
			obs_encoder_release(encoder);
		}

		for (auto encoder : m_recordingVideoEncoders) {
			obs_encoder_release(encoder);
		}
	}

protected:
	virtual obs_encoder_t *GetStreamingVideoEncoder(size_t index)
	{
		if (index >= 0 && index < m_streamingVideoEncoders.size())
			return m_streamingVideoEncoders[index];
		else
			return nullptr;
	}

	virtual obs_encoder_t *GetRecordingVideoEncoder(size_t index)
	{
		if (index >= 0 && index < m_recordingVideoEncoders.size())
			return m_recordingVideoEncoders[index];
		else
			return GetStreamingVideoEncoder(index);
	}

public:
	virtual bool IsObsNative() { return false; }

	virtual video_t *GetVideo() { return m_video; }

	virtual void GetVideoBaseDimensions(uint32_t *videoWidth,
					    uint32_t *videoHeight)
	{
		*videoWidth = m_baseWidth;
		*videoHeight = m_baseHeight;
	}


	virtual void Render() {
		if (m_videoView) {
			obs_view_render(m_videoView);
		} else {
			obs_render_main_texture();
		}

		/*
		m_compositionOwner->ProcessRenderingRoot(
			[](obs_source_t *source) {
				obs_source_video_render(source);
			});
		*/
	}
};

// ctor only usable by this class
StreamElementsCustomVideoComposition::StreamElementsCustomVideoComposition(
	StreamElementsCustomVideoComposition::Private, std::string id,
	std::string name, uint32_t baseWidth, uint32_t baseHeight,
	std::vector<std::string> &streamingVideoEncoderIds,
	std::vector<OBSDataAutoRelease> &streamingVideoEncoderSettings,
	std::vector<OBSDataAutoRelease> &streamingVideoEncoderHotkeyData)
	: StreamElementsVideoCompositionBase("custom_video_composition",  id, name),
	  m_baseWidth(baseWidth),
	  m_baseHeight(baseHeight),
	  m_transitionDurationMs(0)
{
	if (streamingVideoEncoderIds.size() !=
		    streamingVideoEncoderSettings.size() ||
	    streamingVideoEncoderSettings.size() !=
		    streamingVideoEncoderHotkeyData.size())
		throw std::runtime_error(
			"mismatch between sizes of passed in vectors");

	/* align to multiple-of-two and SSE alignment sizes */
	m_baseWidth &= 0xFFFFFFFC;
	m_baseHeight &= 0xFFFFFFFE;

	obs_video_info ovi;

	if (!obs_get_video_info(&ovi))
		throw std::runtime_error("obs_get_video_info() failed");

	for (size_t idx = 0; idx < streamingVideoEncoderSettings.size(); ++idx) {
		obs_data_t* settings = streamingVideoEncoderSettings[idx];

		char buf[32];
		sprintf(buf, "%d", (int)idx + 1);

		auto created_encoder = obs_video_encoder_create(
			streamingVideoEncoderIds[idx].c_str(),
			(name + ": streaming video encoder " + std::string(buf)).c_str(),
			settings, streamingVideoEncoderHotkeyData[idx]);

		if (!created_encoder) {
			for (auto encoder : m_streamingVideoEncoders) {
				obs_encoder_release(encoder);
			}

			throw std::runtime_error(
				"obs_video_encoder_create() failed");
		}

		m_streamingVideoEncoders.push_back(created_encoder);
	}

	if (!m_streamingVideoEncoders.size()) {
		throw std::runtime_error("no encoders were created");
	}

	//
	// This will prevent obs_encoder_get_width & obs_encoder_get_height from crashing due to video output being improperly initialized for SOME REASON
	// https://app.bugsplat.com/v2/crash?database=OBS_Live&id=1488897
	//
	for (auto encoder : m_streamingVideoEncoders) {
		obs_encoder_set_scaled_size(encoder, m_baseWidth, m_baseHeight);
	}

	m_transition = obs_source_create_private(
		"cut_transition", (name + ": transition").c_str(), nullptr);

	if (!m_transition) {
		for (auto encoder : m_streamingVideoEncoders) {
			obs_encoder_release(encoder);
		}

		throw std::runtime_error("obs_source_create_private(cut_transition) failed");
	}

	ovi.base_width = m_baseWidth;
	ovi.base_height = m_baseHeight;
	ovi.output_width = m_baseWidth;
	ovi.output_height = m_baseHeight;

	m_view = obs_view_create();
	m_video = obs_view_add2(m_view, &ovi);

	obs_view_set_source(m_view, 0, m_transition);

	for (auto encoder : m_streamingVideoEncoders) {
		obs_encoder_set_video(encoder, m_video);
	}

	auto currentScene = scene_create_private_with_custom_size(
		GetUniqueSceneName("Scene").c_str(), m_baseWidth, m_baseHeight);
	m_currentScene = obs_scene_get_ref(currentScene);
	m_scenes.push_back(currentScene);

	m_signalHandlerData = new SESignalHandlerData(nullptr, this);

	add_scene_signals(currentScene, m_signalHandlerData);

	obs_transition_set(m_transition, obs_scene_get_source(currentScene));

	transition_set_defaults(m_transition, m_baseWidth, m_baseHeight);

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

void StreamElementsCustomVideoComposition::SetRecordingEncoders(
	std::vector<std::string> &recordingVideoEncoderIds,
	std::vector<OBSDataAutoRelease> &recordingVideoEncoderSettings,
	std::vector<OBSDataAutoRelease> &recordingVideoEncoderHotkeyData)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	if (recordingVideoEncoderIds.size() !=
		    recordingVideoEncoderSettings.size() ||
	    recordingVideoEncoderSettings.size() !=
		    recordingVideoEncoderHotkeyData.size())
		throw std::runtime_error(
			"mismatch between sizes of passed in vectors");

	for (size_t idx = 0; idx < recordingVideoEncoderSettings.size();
	     ++idx) {
		char buf[32];
		sprintf(buf, "%d", (int)idx + 1);

		auto created_encoder = obs_video_encoder_create(
			recordingVideoEncoderIds[idx].c_str(),
			(GetName() + ": recording video encoder " + std::string(buf)).c_str(),
			recordingVideoEncoderSettings[idx],
			recordingVideoEncoderHotkeyData[idx]);

		if (!created_encoder) {
			for (auto encoder : m_recordingVideoEncoders) {
				obs_encoder_release(encoder);
			}

			m_recordingVideoEncoders.clear();

			return;
		}

		//
		// This will prevent obs_encoder_get_width & obs_encoder_get_height from crashing due to video output being improperly initialized for SOME REASON
		// https://app.bugsplat.com/v2/crash?database=OBS_Live&id=1488897
		//
		obs_encoder_set_scaled_size(created_encoder, m_baseWidth,
					    m_baseHeight);

		obs_encoder_set_video(created_encoder, m_video);

		m_recordingVideoEncoders.push_back(created_encoder);
	}
}

StreamElementsCustomVideoComposition::~StreamElementsCustomVideoComposition()
{
	for (auto encoder : m_streamingVideoEncoders) {
		obs_encoder_release(encoder);
	}
	m_streamingVideoEncoders.clear();

	for (auto encoder : m_recordingVideoEncoders) {
		obs_encoder_release(encoder);
	}
	m_recordingVideoEncoders.clear();

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
	}
	m_scenes.clear();

	if (m_currentScene) {
		obs_scene_release(m_currentScene);
		m_currentScene = nullptr;
	}

	m_signalHandlerData->Wait();
	m_signalHandlerData->Release();
	m_signalHandlerData = nullptr;
}

std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
StreamElementsCustomVideoComposition::GetCompositionInfo(
	StreamElementsVideoCompositionEventListener *listener,
	std::string holder)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	return std::make_shared<StreamElementsCustomVideoCompositionInfo>(
		shared_from_this(), listener, holder, m_video, m_baseWidth, m_baseHeight, m_streamingVideoEncoders, m_recordingVideoEncoders, m_view);
}

obs_scene_t *StreamElementsCustomVideoComposition::GetCurrentScene()
{
	//std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	std::shared_lock<decltype(m_currentSceneMutex)> currentSceneLock;

	return m_currentScene;
}

obs_scene_t *StreamElementsCustomVideoComposition::GetCurrentSceneRef()
{
	//std::shared_lock<decltype(m_mutex)> lock(m_mutex);
	std::shared_lock<decltype(m_currentSceneMutex)> currentSceneLock;

	if (m_currentScene)
		return obs_scene_get_ref(m_currentScene);
	else
		return nullptr;
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

	std::vector<std::string> streamingVideoEncoderIds;
	std::vector<OBSDataAutoRelease> streamingVideoEncoderSettings;
	std::vector<OBSDataAutoRelease> streamingVideoEncoderHotkeyData;

	ParseEncodersList(streamingVideoEncoders, streamingVideoEncoderIds,
		      streamingVideoEncoderSettings,
		      streamingVideoEncoderHotkeyData);

	std::exception_ptr exception = nullptr;
	std::shared_ptr<StreamElementsCustomVideoComposition> result = nullptr;

	try {
		result = Create(id, name, width, height,
				streamingVideoEncoderIds,
				streamingVideoEncoderSettings,
				streamingVideoEncoderHotkeyData);
	} catch(...) {
		result = nullptr;

		exception = std::current_exception();
	}

	if (exception)
		std::rethrow_exception(exception);

	// Recording encoders
	std::vector<std::string> recordingVideoEncoderIds;
	std::vector<OBSDataAutoRelease> recordingVideoEncoderSettings;
	std::vector<OBSDataAutoRelease> recordingVideoEncoderHotkeyData;

	if (ParseEncodersList(recordingVideoEncoders, recordingVideoEncoderIds,
		recordingVideoEncoderSettings,
		recordingVideoEncoderHotkeyData))
	{
		try {
			result->SetRecordingEncoders(
				recordingVideoEncoderIds,
				recordingVideoEncoderSettings,
				recordingVideoEncoderHotkeyData);
		} catch (...) {
			result = nullptr;

			exception = std::current_exception();
		}
	}

	if (exception)
		std::rethrow_exception(exception);

	return result;
}

void StreamElementsCustomVideoComposition::GetAllScenesInternal(scenes_t &result)
{
	result.clear();

	for (auto item : m_scenes) {
		result.push_back(obs_scene_get_ref(item));
	}
}

obs_scene_t *
StreamElementsCustomVideoComposition::AddScene(std::string requestName)
{
	auto scene =
		scene_create_private_with_custom_size(GetUniqueSceneName(requestName).c_str(),
				     m_baseWidth, m_baseHeight);

	obs_scene_get_ref(scene); // caller will release

	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		m_scenes.push_back(scene);
	}

	auto source = obs_scene_get_source(scene);

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

	transition_set_defaults(m_transition, m_baseWidth, m_baseHeight);

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

void StreamElementsCustomVideoComposition::ProcessRenderingRoot(
	std::function<void(obs_source_t*)> callback)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	callback(m_transition);
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
	dispatch_scenes_reset_begin_event(this);

	std::vector<obs_scene_t *> scenesToRemove;

	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		scenesToRemove = m_scenes;
		m_scenes.clear();
	}

	// Clean all owned scenes here

	// Clear current transition
	// obs_transition_set(m_transition, (obs_source_t *)nullptr);

	for (auto scene : scenesToRemove) {
		auto source = obs_scene_get_source(scene);

		remove_scene_signals(scene, m_signalHandlerData);

		obs_source_dec_showing(source);
		obs_source_dec_active(source);

		obs_scene_release(scene);
	}
	scenesToRemove.clear();

	// Recreate current scene here

	auto currentScene = obs_scene_create_private(
		GetUniqueSceneNameInternal("Scene", scenesToRemove).c_str());

	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		m_scenes.push_back(currentScene);
	}

	{
		std::unique_lock<decltype(m_currentSceneMutex)> currentSceneLock;

		if (m_currentScene) {
			obs_scene_release(m_currentScene);
		}

		m_currentScene = obs_scene_get_ref(currentScene);
	}

	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		obs_transition_set(m_transition,
				   obs_scene_get_source(currentScene));

		auto source = obs_scene_get_source(currentScene);
		if (source) {
			obs_source_inc_showing(source);
			obs_source_inc_active(source);
		}

		add_scene_signals(currentScene, m_signalHandlerData);
	}

	dispatch_scene_list_changed_event(this);
	dispatch_scene_changed_event(this, currentScene);

	dispatch_scenes_reset_end_event(this);

	m_signalHandlerData->Wait();
}

////////////////////////////////////////////////////////////////////////////////
// OBS Native Virtual Composition - WITH CUSTOM ENCODERS
////////////////////////////////////////////////////////////////////////////////

std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
StreamElementsObsNativeVideoCompositionWithCustomEncoders::GetCompositionInfo(
	StreamElementsVideoCompositionEventListener *listener,
	std::string holder)
{
	obs_video_info ovi = {0};
	obs_get_video_info(&ovi);

	return std::make_shared<StreamElementsCustomVideoCompositionInfo>(
		shared_from_this(), listener, holder, obs_get_video(),
		ovi.base_width,
		ovi.base_height, m_streamingVideoEncoders,
		m_recordingVideoEncoders, nullptr /* render native OBS canvas */);
}

obs_scene_t *StreamElementsObsNativeVideoCompositionWithCustomEncoders::AddScene(
	std::string requestName)
{
	return obs_scene_create(GetUniqueSceneName(requestName).c_str());
}

bool StreamElementsObsNativeVideoCompositionWithCustomEncoders::RemoveScene(
	obs_scene_t *sceneToRemove)
{
	scenes_t scenes;

	GetAllScenesInternal(scenes);

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		if (*it == sceneToRemove) {
			obs_source_remove(obs_scene_get_source(sceneToRemove));

			return true;
		}
	}

	return false;
}

void StreamElementsObsNativeVideoCompositionWithCustomEncoders::SetTransition(
	obs_source_t *transition)
{
	obs_frontend_set_current_transition(transition);

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

void StreamElementsObsNativeVideoCompositionWithCustomEncoders::
	SetTransitionDurationMilliseconds(int duration)
{
	obs_frontend_set_transition_duration(duration);

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);

	dispatch_js_event("hostVideoCompositionChanged", json);
	dispatch_external_event("hostVideoCompositionChanged", json);
}

obs_source_t *StreamElementsObsNativeVideoCompositionWithCustomEncoders::GetTransition()
{
	auto source = obs_frontend_get_current_transition();

	obs_source_release(source);

	return source;
}

bool StreamElementsObsNativeVideoCompositionWithCustomEncoders::SetCurrentScene(
	obs_scene_t *scene)
{
	if (!scene)
		return false;

	scenes_t scenes;

	GetAllScenesInternal(scenes);

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		if (*it == scene) {
			obs_frontend_set_current_scene(
				obs_scene_get_source(scene));

			return true;
		}
	}

	return false;
}

void StreamElementsObsNativeVideoCompositionWithCustomEncoders::GetAllScenesInternal(
	scenes_t &result)
{
	result.clear();

	struct obs_frontend_source_list frontendScenes = {};

	obs_frontend_get_scenes(&frontendScenes);

	for (size_t idx = 0; idx < frontendScenes.sources.num; ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *source = frontendScenes.sources.array[idx];

		auto scene = obs_scene_from_source(source);

		if (scene) {
			result.push_back(obs_scene_get_ref(scene));
		}
	}

	obs_frontend_source_list_free(&frontendScenes);
}

void StreamElementsObsNativeVideoCompositionWithCustomEncoders::SerializeComposition(
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

	root->SetBool("isObsNativeComposition", IsObsNativeComposition());
	root->SetBool("canRemove", CanRemove());

	auto videoFrame = CefDictionaryValue::Create();

	videoFrame->SetInt("width", int(size.x));
	videoFrame->SetInt("height", int(size.y));

	root->SetDictionary("videoFrame", videoFrame);

	SerializeObsVideoEncoders(this, root);

	output->SetDictionary(root);
}

obs_scene_t *StreamElementsObsNativeVideoCompositionWithCustomEncoders::GetCurrentScene()
{
	auto source = obs_frontend_get_current_scene();
	auto scene = obs_scene_from_source(source);

	obs_source_release(source);

	return scene;
}

obs_scene_t *
StreamElementsObsNativeVideoCompositionWithCustomEncoders::GetCurrentSceneRef()
{
	auto source = obs_frontend_get_current_scene();
	auto scene = obs_scene_from_source(source);

	return scene;
}

StreamElementsObsNativeVideoCompositionWithCustomEncoders::
		StreamElementsObsNativeVideoCompositionWithCustomEncoders(
			StreamElementsObsNativeVideoCompositionWithCustomEncoders::Private,
			std::string id,
		std::string name,
		std::vector<std::string> & streamingVideoEncoderIds,
		std::vector<OBSDataAutoRelease> & streamingVideoEncoderSettings,
		std::vector<OBSDataAutoRelease> &
			streamingVideoEncoderHotkeyData)
		: StreamElementsVideoCompositionBase("custom_obs_native_video_composition",
						     id, name)
{
		if (streamingVideoEncoderIds.size() !=
			    streamingVideoEncoderSettings.size() ||
		    streamingVideoEncoderSettings.size() !=
			    streamingVideoEncoderHotkeyData.size())
			throw std::runtime_error(
				"mismatch between sizes of passed in vectors");

		obs_video_info ovi;

		if (!obs_get_video_info(&ovi))
			throw std::runtime_error("obs_get_video_info() failed");

		for (size_t idx = 0; idx < streamingVideoEncoderSettings.size();
		     ++idx) {
			obs_data_t *settings =
				streamingVideoEncoderSettings[idx];

			char buf[32];
			sprintf(buf, "%d", (int)idx + 1);

			auto created_encoder = obs_video_encoder_create(
				streamingVideoEncoderIds[idx].c_str(),
				(name + ": streaming video encoder " +
				 std::string(buf))
					.c_str(),
				settings, streamingVideoEncoderHotkeyData[idx]);

			if (!created_encoder) {
				for (auto encoder : m_streamingVideoEncoders) {
					obs_encoder_release(encoder);
				}

				throw std::runtime_error(
					"obs_video_encoder_create() failed");
			}

			m_streamingVideoEncoders.push_back(created_encoder);
		}

		if (!m_streamingVideoEncoders.size()) {
			throw std::runtime_error("no encoders were created");
		}

		//
		// This will prevent obs_encoder_get_width & obs_encoder_get_height from crashing due to video output being improperly initialized for SOME REASON
		// https://app.bugsplat.com/v2/crash?database=OBS_Live&id=1488897
		//
		for (auto encoder : m_streamingVideoEncoders) {
			obs_encoder_set_scaled_size(encoder, ovi.base_width,
						    ovi.base_height);
		}

		for (auto encoder : m_streamingVideoEncoders) {
			obs_encoder_set_video(encoder, obs_get_video());
		}
}

StreamElementsObsNativeVideoCompositionWithCustomEncoders::
	~StreamElementsObsNativeVideoCompositionWithCustomEncoders()
{
	for (auto encoder : m_streamingVideoEncoders) {
		obs_encoder_release(encoder);
	}
	m_streamingVideoEncoders.clear();

	for (auto encoder : m_recordingVideoEncoders) {
		obs_encoder_release(encoder);
	}
	m_recordingVideoEncoders.clear();
}

void StreamElementsObsNativeVideoCompositionWithCustomEncoders::SetRecordingEncoders(
	std::vector<std::string> &recordingVideoEncoderIds,
	std::vector<OBSDataAutoRelease> &recordingVideoEncoderSettings,
	std::vector<OBSDataAutoRelease> &recordingVideoEncoderHotkeyData)
{
	if (recordingVideoEncoderIds.size() !=
		    recordingVideoEncoderSettings.size() ||
	    recordingVideoEncoderSettings.size() !=
		    recordingVideoEncoderHotkeyData.size())
		throw std::runtime_error(
			"mismatch between sizes of passed in vectors");

	obs_video_info ovi = {0};
	obs_get_video_info(&ovi);

	for (size_t idx = 0; idx < recordingVideoEncoderSettings.size();
	     ++idx) {
		char buf[32];
		sprintf(buf, "%d", (int)idx + 1);

		auto created_encoder = obs_video_encoder_create(
			recordingVideoEncoderIds[idx].c_str(),
			(GetName() + ": recording video encoder " +
			 std::string(buf))
				.c_str(),
			recordingVideoEncoderSettings[idx],
			recordingVideoEncoderHotkeyData[idx]);

		if (!created_encoder) {
			for (auto encoder : m_recordingVideoEncoders) {
				obs_encoder_release(encoder);
			}

			m_recordingVideoEncoders.clear();

			return;
		}

		//
		// This will prevent obs_encoder_get_width & obs_encoder_get_height from crashing due to video output being improperly initialized for SOME REASON
		// https://app.bugsplat.com/v2/crash?database=OBS_Live&id=1488897
		//
		obs_encoder_set_scaled_size(created_encoder, ovi.base_width,
					    ovi.base_height);

		obs_encoder_set_video(created_encoder, obs_get_video());

		m_recordingVideoEncoders.push_back(created_encoder);
	}
}

std::shared_ptr<StreamElementsObsNativeVideoCompositionWithCustomEncoders>
StreamElementsObsNativeVideoCompositionWithCustomEncoders::Create(
	std::string id, std::string name,
	CefRefPtr<CefValue> streamingVideoEncoders,
	CefRefPtr<CefValue> recordingVideoEncoders)
{

	std::vector<std::string> streamingVideoEncoderIds;
	std::vector<OBSDataAutoRelease> streamingVideoEncoderSettings;
	std::vector<OBSDataAutoRelease> streamingVideoEncoderHotkeyData;

	ParseEncodersList(streamingVideoEncoders, streamingVideoEncoderIds,
			  streamingVideoEncoderSettings,
			  streamingVideoEncoderHotkeyData);

	std::exception_ptr exception = nullptr;
	std::shared_ptr<StreamElementsObsNativeVideoCompositionWithCustomEncoders> result =
		nullptr;

	try {
		result = Create(id, name, streamingVideoEncoderIds,
				streamingVideoEncoderSettings,
				streamingVideoEncoderHotkeyData);
	} catch (...) {
		result = nullptr;

		exception = std::current_exception();
	}

	if (exception)
		std::rethrow_exception(exception);

	// Recording encoders
	std::vector<std::string> recordingVideoEncoderIds;
	std::vector<OBSDataAutoRelease> recordingVideoEncoderSettings;
	std::vector<OBSDataAutoRelease> recordingVideoEncoderHotkeyData;

	if (ParseEncodersList(recordingVideoEncoders, recordingVideoEncoderIds,
			      recordingVideoEncoderSettings,
			      recordingVideoEncoderHotkeyData)) {
		try {
			result->SetRecordingEncoders(
				recordingVideoEncoderIds,
				recordingVideoEncoderSettings,
				recordingVideoEncoderHotkeyData);
		} catch (...) {
			result = nullptr;

			exception = std::current_exception();
		}
	}

	if (exception)
		std::rethrow_exception(exception);

	return result;
}
