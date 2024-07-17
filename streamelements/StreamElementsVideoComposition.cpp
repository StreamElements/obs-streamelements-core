#include "StreamElementsVideoComposition.hpp"
#include <obs-frontend-api.h>

#include "StreamElementsUtils.hpp"

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

public:
	StreamElementsCustomVideoCompositionInfo(
		std::shared_ptr<StreamElementsVideoCompositionBase> owner,
		StreamElementsCompositionEventListener *listener, video_t* video, obs_encoder_t *videoEncoder, obs_view_t *videoView)
		: StreamElementsVideoCompositionBase::CompositionInfo(owner,
								      listener),
		  m_video(video),
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

	virtual void Render() { obs_view_render(m_videoView); }
};

// ctor only usable by this class
StreamElementsCustomVideoComposition::StreamElementsCustomVideoComposition(
	StreamElementsCustomVideoComposition::Private, std::string id,
	std::string name, uint32_t width, uint32_t height,
	std::string videoEncoderId, obs_data_t *videoEncoderSettings,
	obs_data_t *videoEncoderHotkeyData)
	: StreamElementsVideoCompositionBase(id, name)
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

	ovi.base_width = width;
	ovi.base_height = height;
	ovi.output_width = width;
	ovi.output_height = height;

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
		obs_data_set_int(settings, "width", width);
		obs_data_set_int(settings, "height", height);
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
		shared_from_this(), listener, m_video, m_videoEncoder, m_view);
}

void StreamElementsCustomVideoComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);
}

obs_scene_t *StreamElementsCustomVideoComposition::GetCurrentScene()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_currentScene;
}
