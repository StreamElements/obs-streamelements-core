#include "StreamElementsVideoComposition.hpp"
#include <obs-frontend-api.h>
#include <util/config-file.h>

#include "StreamElementsUtils.hpp"

static CefRefPtr<CefDictionaryValue> SerializeObsEncoder(obs_encoder_t *e)
{
	auto result = CefDictionaryValue::Create();

	result->SetString("id", obs_encoder_get_id(e));

	auto settings = obs_encoder_get_settings(e);

	result->SetValue("settings",
			 CefParseJSON(obs_data_get_json(settings),
				      JSON_PARSER_ALLOW_TRAILING_COMMAS));

	obs_data_release(settings);

	return result;
}

static void SerializeObsEncoders(StreamElementsVideoCompositionBase* composition, CefRefPtr<CefDictionaryValue>& root)
{
	auto info = composition->GetCompositionInfo(nullptr);

	auto streamingVideoEncoders = CefListValue::Create();
	streamingVideoEncoders->SetDictionary(
		0, SerializeObsEncoder(info->GetStreamingVideoEncoder()));
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

	auto videoFrame = CefDictionaryValue::Create();

	videoFrame->SetInt("width", int(size.x));
	videoFrame->SetInt("height", int(size.y));

	root->SetDictionary("videoFrame", videoFrame);

	SerializeObsEncoders(this, root);

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
	std::string videoEncoderId, obs_data_t *videoEncoderSettings,
	obs_data_t *videoEncoderHotkeyData)
	: StreamElementsVideoCompositionBase(id, name),
	  m_baseWidth(baseWidth),
	  m_baseHeight(baseHeight)
{
	obs_video_info ovi;

	if (!obs_get_video_info(&ovi))
		throw std::exception("obs_get_video_info() failed", 1);

	m_videoEncoder = obs_video_encoder_create(
		videoEncoderId.c_str(), (name + ": video encoder").c_str(),
		videoEncoderSettings, videoEncoderHotkeyData);

	if (!m_videoEncoder)
		throw std::exception("obs_video_encoder_create() failed", 2);

	m_transition = obs_source_create_private(
		"cut_transition", (name + ": transition").c_str(), nullptr);

	if (!m_transition) {
		obs_encoder_release(m_videoEncoder);
		throw std::exception("obs_source_create_private(cut_transition) failed", 2);
	}

	ovi.base_width = m_baseWidth;
	ovi.base_height = m_baseHeight;
	ovi.output_width = m_baseWidth;
	ovi.output_height = m_baseHeight;

	m_view = obs_view_create();
	m_video = obs_view_add2(m_view, &ovi);

	obs_view_set_source(m_view, 0, m_transition);

	obs_encoder_set_video(m_videoEncoder, m_video);

	m_currentScene = obs_scene_create_private("Scene 1");
	m_scenes.push_back(m_currentScene);

	obs_transition_start(m_transition, OBS_TRANSITION_MODE_AUTO, 0,
			     obs_scene_get_source(m_currentScene));

	// TODO: Remove debug code
	QtPostTask([=]() -> void {
		auto source = obs_source_create("browser_source",
						"test browser source", nullptr,
						nullptr);

		obs_data_t *settings = obs_data_create();
		obs_data_set_int(settings, "width", baseWidth);
		obs_data_set_int(settings, "height", baseHeight);
		obs_source_update(source, settings);
		obs_data_release(settings);

		struct atomic_update_args {
			obs_source_t *source;
			obs_sceneitem_t *sceneItem;
			//obs_sceneitem_t *group;
		};

		atomic_update_args args = {};

		args.source = source;
		args.sceneItem = nullptr;
		//args.group = nullptr;

		obs_enter_graphics();
		obs_scene_atomic_update(
			m_currentScene,
			[](void *data, obs_scene_t *scene) {
				atomic_update_args *args =
					(atomic_update_args *)data;

				auto sceneItem =
					obs_scene_add(scene, args->source);
				obs_sceneitem_addref(
					sceneItem); // obs_scene_add() does not increment refcount for result. why? because.

				args->sceneItem = sceneItem;
			},
			&args);
		obs_leave_graphics();

		obs_sceneitem_release(args.sceneItem);
		obs_source_release(source);

		obs_enum_sources(
			[](void *arg, obs_source_t *iterator) {
				std::string id = obs_source_get_id(iterator);
				std::string name =
					obs_source_get_name(iterator);

				std::string showing =
					obs_source_showing(iterator) ? "showing"
								     : "hidden";

				std::string active = obs_source_active(iterator)
							     ? "active"
							     : "inactive";

				OutputDebugStringA(
					("source: " + id + " - " + name + " - " + showing + ", " + active + "\n")
						.c_str());

				return true;
			},
			nullptr);

		uint32_t cx, cy;
		obs_transition_get_size(m_transition, &cx, &cy);
		char buffer[32];
		std::string size = itoa(cx, buffer, 10);
		size += " x ";
		size += itoa(cy, buffer, 10);
		OutputDebugStringA(("transition size: " + size).c_str());
	});
}

StreamElementsCustomVideoComposition::~StreamElementsCustomVideoComposition()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (m_videoEncoder) {
		obs_encoder_release(m_videoEncoder);
		m_videoEncoder = nullptr;
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
		shared_from_this(), listener, m_video, m_baseWidth, m_baseHeight, m_videoEncoder, m_view);
}

void StreamElementsCustomVideoComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	auto root = CefDictionaryValue::Create();

	root->SetString("id", GetId());
	root->SetString("name", GetName());

	auto videoFrame = CefDictionaryValue::Create();

	videoFrame->SetInt("width", m_baseWidth);
	videoFrame->SetInt("height", m_baseHeight);

	root->SetDictionary("videoFrame", videoFrame);

	SerializeObsEncoders(this, root);

	output->SetDictionary(root);
}

obs_scene_t *StreamElementsCustomVideoComposition::GetCurrentScene()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_currentScene;
}
