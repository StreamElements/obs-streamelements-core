#include "StreamElementsVideoComposition.hpp"
#include <obs-frontend-api.h>

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

	auto scene1 = obs_scene_create_private("Scene 1");
	m_scenes.push_back(scene1);
	
	obs_transition_start(m_transition, OBS_TRANSITION_MODE_AUTO, 0,
			     obs_scene_get_source(scene1));
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
