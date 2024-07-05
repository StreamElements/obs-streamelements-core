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
// TODO: OBS Views
////////////////////////////////////////////////////////////////////////////////

