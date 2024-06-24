#include "StreamElementsComposition.hpp"
#include <obs-frontend-api.h>

////////////////////////////////////////////////////////////////////////////////
// OBS Main Composition
////////////////////////////////////////////////////////////////////////////////

class StreamElementsDefaultCompositionInfo
	: public StreamElementsCompositionBase::CompositionInfo {
private:
	StreamElementsCompositionEventListener *m_listener;

public:
	StreamElementsDefaultCompositionInfo(
		StreamElementsCompositionBase *owner,
		StreamElementsCompositionEventListener* listener)
		: StreamElementsCompositionBase::CompositionInfo(
			  owner, listener),
		  m_listener(listener)
	{
	}

	virtual ~StreamElementsDefaultCompositionInfo() {}

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
	StreamElementsCompositionBase::CompositionInfo>
StreamElementsObsNativeComposition::GetCompositionInfo(
	StreamElementsCompositionEventListener* listener)
{
	return std::make_shared<StreamElementsDefaultCompositionInfo>(this, listener);
}

void StreamElementsObsNativeComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
}

////////////////////////////////////////////////////////////////////////////////
// TODO: OBS Views
////////////////////////////////////////////////////////////////////////////////

