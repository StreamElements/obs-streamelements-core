#include "StreamElementsVideoComposition.hpp"
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "StreamElementsUtils.hpp"
#include "StreamElementsObsSceneManager.hpp"

static class obs_graphics_guard {
public:
	obs_graphics_guard() { obs_enter_graphics(); }
	~obs_graphics_guard() { obs_leave_graphics(); }

	obs_graphics_guard(obs_graphics_guard &other) = delete;
	void operator=(obs_graphics_guard & other) = delete;
};

static void SerializeObsTransition(std::string videoCompositionId, obs_source_t* t, int durationMilliseconds, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!t)
		return;

	auto d = CefDictionaryValue::Create();

	std::string id = obs_source_get_id(t);
	d->SetString("class", id);
	d->SetString("videoCompositionId", id);

	auto settings = obs_source_get_settings(t);

	auto props = obs_get_source_properties(id.c_str());
	auto propsValue = CefValue::Create();
	obs_properties_apply_settings(props, settings);
	SerializeObsProperties(props, propsValue);
	obs_properties_destroy(props);

	auto settingsValue = SerializeObsData(settings);

	obs_data_release(settings);

	d->SetValue("properties", propsValue);
	d->SetValue("settings", settingsValue);

	auto defaultsData = obs_get_source_defaults(id.c_str());
	d->SetValue("defaultSettings", SerializeObsData(defaultsData));
	obs_data_release(defaultsData);

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

bool DeserializeObsTransition(CefRefPtr<CefValue> input, obs_source_t **t, int *durationMilliseconds)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	auto d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return false;

	std::string id = d->GetString("class");

	auto settings = obs_data_create();

	if (d->HasKey("settings")) {
		DeserializeObsData(d->GetValue("settings"), settings);
	}
	
	*t = obs_source_create_private(id.c_str(), id.c_str(), settings);

	obs_data_release(settings);

	if (d->HasKey("width") &&
	    d->GetType("width") == VTYPE_INT && d->HasKey("height") && d->GetType("height") == VTYPE_INT) {
		int cx = d->GetInt("width");
		int cy = d->GetInt("height");

		if (cx <= 0 || cy <= 0)
			return false;

		obs_transition_set_size(*t, cx, cy);
	}

	if (d->HasKey("scaleType") && d->GetType("scaleType") == VTYPE_STRING) {
		auto scaleType = d->GetString("scaleType");

		if (scaleType == "maxOnly") {
			obs_transition_set_scale_type(
				*t, OBS_TRANSITION_SCALE_MAX_ONLY);
		} else if (scaleType == "aspect") {
			obs_transition_set_scale_type(
				*t, OBS_TRANSITION_SCALE_ASPECT);
		} else if (scaleType == "stretch") {
			obs_transition_set_scale_type(
				*t, OBS_TRANSITION_SCALE_STRETCH);
		} else {
			return false;
		}
	}

	if (d->HasKey("alignment") && d->GetType("alignment") == VTYPE_STRING) {
		obs_transition_set_alignment(
			*t, GetInt32FromAlignmentId(
				    d->GetString("alignment").c_str()));
	}

	if (d->HasKey("durationMilliseconds") &&
	    d->GetType("durationMilliseconds") == VTYPE_INT) {
		*durationMilliseconds = d->GetInt("durationMilliseconds");
	} else {
		*durationMilliseconds = 300; // OBS FE default
	}

	return true;
}

static CefRefPtr<CefDictionaryValue> SerializeObsEncoder(obs_encoder_t *e)
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

	if (obs_encoder_get_type(e) == OBS_ENCODER_VIDEO) {
		result->SetInt("width", obs_encoder_get_width(e));
		result->SetInt("height", obs_encoder_get_height(e));
	}

	auto settings = obs_encoder_get_settings(e);

	result->SetValue("settings",
			 CefParseJSON(obs_data_get_json(settings),
				      JSON_PARSER_ALLOW_TRAILING_COMMAS));

	result->SetValue("properties", SerializeObsEncoderProperties(obs_encoder_get_id(e), settings));

	auto defaultSettings = obs_encoder_get_defaults(e);

	if (defaultSettings) {
		result->SetValue("defaultSettings",
			    SerializeObsData(defaultSettings));

		obs_data_release(defaultSettings);
	}

	obs_data_release(settings);

	return result;
}

static void SerializeObsEncoders(StreamElementsVideoCompositionBase* composition, CefRefPtr<CefDictionaryValue>& root)
{
	auto info = composition->GetCompositionInfo(nullptr);

	auto encoder = info->GetStreamingVideoEncoder();

	// OBS Native composition doesn't have an encoder until we start streaming
	if (!encoder)
		return;

	auto streamingVideoEncoders = CefListValue::Create();
	streamingVideoEncoders->SetDictionary(
		0, SerializeObsEncoder(encoder));
	root->SetList("streamingVideoEncoders", streamingVideoEncoders);

	auto streamingAudioEncoders = CefListValue::Create();
	for (size_t i = 0;; ++i) {
		auto encoder = info->GetStreamingAudioEncoder(i);

		if (!encoder)
			break;

		streamingAudioEncoders->SetDictionary(
			i, SerializeObsEncoder(encoder));
	}
	root->SetList("streamingAudioEncoders", streamingAudioEncoders);
}

////////////////////////////////////////////////////////////////////////////////
// Composition base
////////////////////////////////////////////////////////////////////////////////

std::string
StreamElementsVideoCompositionBase::GetUniqueSceneName(std::string name)
{
	std::string result(name);

	std::vector<obs_scene_t *> scenes;
	GetAllScenes(scenes);

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

bool StreamElementsVideoCompositionBase::SafeRemoveScene(
	obs_scene_t *sceneToRemove)
{
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

obs_sceneitem_t *
StreamElementsVideoCompositionBase::GetSceneItemById(std::string id,
						     bool addRef)
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
		ObsSceneEnumAllItems(
			scene, [&](obs_sceneitem_t *sceneitem) -> bool {
				if (searchPtr == sceneitem) {
					result = sceneitem;

					/* Found what we're looking for, stop iteration */
					return false;
				}

				/* Continue or stop iteration */
				return !result;
			});

		if (result) {
			break;
		}
	}

	if (result && addRef) {
		obs_sceneitem_addref(result);
	}

	return result;
}

void StreamElementsVideoCompositionBase::SerializeTransition(
	CefRefPtr<CefValue>& output)
{
	obs_graphics_guard graphics_guard;

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

	if (!DeserializeObsTransition(input, &transition, &durationMs))
		return;

	obs_graphics_guard graphics_guard;

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
	StreamElementsCompositionEventListener *m_listener;

public:
	StreamElementsDefaultVideoCompositionInfo(
		std::shared_ptr<StreamElementsVideoCompositionBase> owner,
		StreamElementsCompositionEventListener* listener)
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

	virtual obs_encoder_t *GetStreamingAudioEncoder(size_t index) {
		auto output = obs_frontend_get_streaming_output();

		auto result = obs_output_get_audio_encoder(output, index);

		obs_output_release(output);

		return result;
	}

	virtual video_t *GetVideo() { return obs_get_video(); }
	virtual audio_t *GetAudio() { return obs_get_audio(); }

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
	StreamElementsCompositionEventListener* listener)
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
	obs_graphics_guard graphics_guard;

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

	SerializeObsEncoders(this, root);

	output->SetDictionary(root);
}

obs_scene_t* StreamElementsObsNativeVideoComposition::GetCurrentScene()
{
	obs_graphics_guard graphics_guard;

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
	StreamElementsCompositionEventListener *m_listener;

	video_t *m_video;
	obs_encoder_t *m_videoEncoder;
	obs_view_t *m_videoView;
	uint32_t m_baseWidth;
	uint32_t m_baseHeight;

public:
	StreamElementsCustomVideoCompositionInfo(
		std::shared_ptr<StreamElementsVideoCompositionBase> owner,
		StreamElementsCompositionEventListener *listener,
		video_t *video, uint32_t baseWidth, uint32_t baseHeight,
		obs_encoder_t *videoEncoder, obs_view_t *videoView)
		: StreamElementsVideoCompositionBase::CompositionInfo(owner,
								      listener),
		  m_video(video),
		  m_baseWidth(baseWidth),
		  m_baseHeight(baseHeight),
		  m_videoEncoder(videoEncoder),
		  m_videoView(videoView),
		  m_listener(listener)
	{
	}

	virtual ~StreamElementsCustomVideoCompositionInfo() {}

public:
	virtual bool IsObsNative() { return false; }

	virtual obs_encoder_t *GetStreamingVideoEncoder()
	{
		return m_videoEncoder;
	}

	virtual obs_encoder_t *GetStreamingAudioEncoder(size_t index)
	{
		auto output = obs_frontend_get_streaming_output();

		auto result = obs_output_get_audio_encoder(output, index);

		obs_output_release(output);

		return result;
	}

	virtual video_t *GetVideo() { return m_video; }
	virtual audio_t *GetAudio() { return obs_get_audio(); }

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
	: StreamElementsVideoCompositionBase(id, name),
	  m_baseWidth(baseWidth),
	  m_baseHeight(baseHeight),
	  m_transitionDurationMs(0)
{
	obs_graphics_guard graphics_guard;

	obs_video_info ovi;

	if (!obs_get_video_info(&ovi))
		throw std::exception("obs_get_video_info() failed", 1);

	m_streamingVideoEncoder = obs_video_encoder_create(
		streamingVideoEncoderId.c_str(), (name + ": video encoder").c_str(),
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

	m_currentScene = obs_scene_create_private("Scene 1");
	m_scenes.push_back(m_currentScene);

	obs_transition_start(m_transition, OBS_TRANSITION_MODE_AUTO, 0,
			     obs_scene_get_source(m_currentScene));
}

StreamElementsCustomVideoComposition::~StreamElementsCustomVideoComposition()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
	obs_graphics_guard graphics_guard;

	if (m_streamingVideoEncoder) {
		obs_encoder_release(m_streamingVideoEncoder);
		m_streamingVideoEncoder = nullptr;
	}

	if (m_view) {
		obs_view_remove(m_view);
		obs_view_destroy(m_view);
		m_view = nullptr;
	}

	m_video = nullptr;

	if (m_transition) {
		obs_transition_clear(m_transition);
		obs_source_release(m_transition);
		m_transition = nullptr;
	}

	if (m_scenes.size()) {
		for (auto scene : m_scenes) {
			obs_scene_release(scene);
		}

		m_scenes.clear();
	}
}

std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
StreamElementsCustomVideoComposition::GetCompositionInfo(
	StreamElementsCompositionEventListener *listener)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return std::make_shared<StreamElementsCustomVideoCompositionInfo>(
		shared_from_this(), listener, m_video, m_baseWidth, m_baseHeight, m_streamingVideoEncoder, m_view);
}

obs_scene_t *StreamElementsCustomVideoComposition::GetCurrentScene()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_currentScene;
}

void StreamElementsCustomVideoComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
	obs_graphics_guard graphics_guard;

	auto root = CefDictionaryValue::Create();

	root->SetString("id", GetId());
	root->SetString("name", GetName());

	root->SetBool("isObsNativeComposition", false);
	root->SetBool("canRemove", this->CanRemove());

	auto videoFrame = CefDictionaryValue::Create();

	videoFrame->SetInt("width", m_baseWidth);
	videoFrame->SetInt("height", m_baseHeight);

	root->SetDictionary("videoFrame", videoFrame);

	SerializeObsEncoders(this, root);

	output->SetDictionary(root);
}

std::shared_ptr<StreamElementsVideoCompositionBase>
StreamElementsCustomVideoComposition::Create(
	std::string id, std::string name, uint32_t width, uint32_t height,
       CefRefPtr<CefValue> streamingVideoEncoders)
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
	std::shared_ptr<StreamElementsVideoCompositionBase> result = nullptr;

	try {
		result = Create(id, name, width, height,
				streamingVideoEncoderId,
				streamingVideoEncoderSettings,
				streamingVideoEncoderHotkeyData);
	} catch(...) {
		result = nullptr;

		exception = std::current_exception();
	}

	obs_data_release(streamingVideoEncoderSettings);
	if (streamingVideoEncoderHotkeyData)
		obs_data_release(streamingVideoEncoderHotkeyData);

	if (exception)
		std::rethrow_exception(exception);

	return result;
}

void StreamElementsCustomVideoComposition::GetAllScenes(
	std::vector<obs_scene_t *> &result)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	result.clear();

	for (auto item : m_scenes) {
		result.push_back(item);
	}
}

obs_scene_t *
StreamElementsCustomVideoComposition::AddScene(std::string requestName)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
	obs_graphics_guard graphics_guard;

	auto scene = obs_scene_create_private(GetUniqueSceneName(requestName).c_str());

	m_scenes.push_back(scene);

	auto source = obs_scene_get_source(scene);

	calldata_t *data = calldata_create();

	calldata_set_ptr(data, "source", source);

	add_source_signals(source, data);
	add_scene_signals(source, data);

	calldata_destroy(data);

	return scene;
}

bool StreamElementsCustomVideoComposition::RemoveScene(obs_scene_t* scene)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
	obs_graphics_guard graphics_guard;

	for (auto it = m_scenes.begin(); it != m_scenes.end(); ++it) {
		if (scene == *it) {
			m_scenes.erase(it);

			obs_scene_release(scene);

			return true;
		}
	}

	return false;
}

void StreamElementsCustomVideoComposition::SetTransition(
	obs_source_t *transition)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	obs_graphics_guard graphics_guard;

	auto old_transition = m_transition;
	m_transition = transition;
	obs_source_addref(m_transition);
	obs_source_release(old_transition);

	obs_transition_start(m_transition, OBS_TRANSITION_MODE_AUTO, 0,
			     obs_scene_get_source(m_currentScene));
}

obs_source_t *StreamElementsCustomVideoComposition::GetTransition()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
	obs_graphics_guard graphics_guard;

	auto transition = m_transition;

	return transition;
}

bool StreamElementsCustomVideoComposition::SetCurrentScene(obs_scene_t* scene)
{
	if (!scene)
		return false;

	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
	obs_graphics_guard graphics_guard;

	for (auto item : m_scenes) {
		if (scene == item) {
			m_currentScene = scene;

			obs_transition_start(
				m_transition, OBS_TRANSITION_MODE_AUTO,
				GetTransitionDurationMilliseconds(),
				obs_scene_get_source(m_currentScene));

			return true;
		}
	}

	return false;
}
