#include <obs-frontend-api.h>
#include "StreamElementsOutput.hpp"

bool StreamElementsOutputBase::IsEnabled()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_enabled;
}

void StreamElementsOutputBase::SetEnabled(bool enabled)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (enabled == m_enabled)
		return;

	m_enabled = enabled;

	if (CanStart()) {
		Start();
	}
}

bool StreamElementsOutputBase::CanStart()
{
	if (!m_compositionInfo)
		return false;

	if (!IsEnabled())
		return false;

	if (!obs_frontend_streaming_active())
		return false;

	if (IsActive())
		return false;

	return true;
}

bool StreamElementsOutputBase::Start()
{
	if (!CanStart())
		return false;

	return StartInternal(m_compositionInfo);
}

void StreamElementsOutputBase::Stop()
{
	StopInternal();
}

bool StreamElementsCustomOutput::IsActive()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!m_compositionInfo)
		return false;

	if (!m_output)
		return false;

	return obs_output_active(m_output);
}

bool StreamElementsCustomOutput::StartInternal(
	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
		compositionInfo)
{
	if (!compositionInfo)
		return false;

	const char *output_type = obs_service_get_output_type(m_service);

	if (!output_type)
		output_type = "rtmp_output";

	obs_data_t *output_settings = obs_data_create();

	/*
	if (bindToIP) {
		obs_data_set_string(output_settings, "bind_ip", bindToIP);
	}
	*/

	m_output = obs_output_create(output_type, GetId().c_str(),
						 output_settings, nullptr);

	if (m_output) {
		obs_output_set_video_encoder(
			m_output,
			compositionInfo->GetStreamingVideoEncoder());

		for (size_t i = 0;; ++i) {
			auto encoder =
				compositionInfo->GetStreamingAudioEncoder(i);

			if (!encoder)
				break;

			obs_output_set_audio_encoder(m_output, encoder, i);
		}

		obs_output_set_service(m_output, m_service);

		if (obs_output_start(m_output)) {
			return true;
		}

		obs_output_release(m_output);
		m_output = nullptr;
	} else {
		obs_data_release(output_settings);
	}

	return false;
}

void StreamElementsCustomOutput::StopInternal()
{
	if (!m_compositionInfo)
		return;

	// obs_output_stop(m_output);

	obs_output_force_stop(m_output);

	obs_output_release(m_output);
	m_output = nullptr;
}
