#include <obs-frontend-api.h>
#include "StreamElementsOutput.hpp"

bool StreamElementsOutput::IsEnabled()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_enabled;
}

void StreamElementsOutput::SetEnabled(bool enabled)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (enabled == m_enabled)
		return;

	m_enabled = enabled;

	if (m_enabled)
		Start();
}

bool StreamElementsOutput::IsActive()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!m_compositionInfo)
		return false;

	if (!m_output)
		return false;

	return obs_output_active(m_output);
}

bool StreamElementsOutput::Start()
{
	if (!obs_frontend_streaming_active())
		return false;

	return StartInternal();
}

void StreamElementsOutput::Stop()
{
	StopInternal();
}


bool StreamElementsOutput::StartInternal()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (m_compositionInfo)
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

	m_output = obs_output_create(output_type, m_id.c_str(),
						 output_settings, nullptr);

	if (m_output) {
		obs_output_set_video_encoder(
			m_output,
			m_compositionInfo->GetStreamingVideoEncoder());

		for (size_t i = 0;; ++i) {
			auto encoder =
				m_compositionInfo->GetStreamingAudioEncoder(i);

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

	m_compositionInfo = nullptr;

	return false;
}

void StreamElementsOutput::StopInternal()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!m_compositionInfo)
		return;

	// obs_output_stop(m_output);

	obs_output_force_stop(m_output);

	obs_output_release(m_output);
	m_output = nullptr;

	m_compositionInfo = nullptr;
}
