#include "StreamElementsUtils.hpp"
#include "StreamElementsObsOutputManager.hpp"

std::string StreamElementsObsOutputManager::AddSceneOutput(obs_scene_t *scene,
							   int view_width,
							   int view_height)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string id = CreateGloballyUniqueIdString();

	m_outputs[id] = std::make_shared<StreamElementsObsOutput>(scene, view_width, view_height);

	return id;
}

bool StreamElementsObsOutputManager::StartOutputById(std::string id) {
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_outputs.count(id) == 0)
		return false;

	return m_outputs[id]->Start();
}

bool StreamElementsObsOutputManager::StopOutputById(std::string id)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_outputs.count(id) == 0)
		return false;

	m_outputs[id]->Stop();

	return true;
}

/////////////////////////////////////////////////////////////////////////////
// StreamElementsObsOutput
/////////////////////////////////////////////////////////////////////////////

void StreamElementsObsOutput::Stop()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_output) {
		obs_output_stop(m_output);

		obs_output_release(m_output);

		m_output = nullptr;
	}

	if (m_view) {
		obs_view_remove(m_view);
		obs_view_destroy(m_view);

		m_view = nullptr;
		m_video = nullptr;
	}
}

bool StreamElementsObsOutput::Start()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_video)
		return false;

	// Output settings
	int maxBitrateBitsPerSecond = 1500000;
	std::string serverUrl;
	std::string streamKey;
	bool useAuth = false;
	std::string authUsername;
	std::string authPassword;
	std::string bindToIP;

	m_view = obs_view_create();
	m_video = obs_view_add(m_view);

	obs_source_t *scene_source = obs_scene_get_source(m_container_scene);

	obs_view_set_source(m_view, 0, scene_source);

	auto video_info = video_output_get_info(m_video);

	obs_encoder_t *vencoder = obs_video_encoder_create(
		"obs_x264", "test_x264", nullptr, nullptr);
	obs_encoder_t *aencoder = obs_audio_encoder_create(
		"ffmpeg_aac", "test_aac", nullptr, 0, nullptr);
	obs_service_t *service = obs_service_create(
		"rtmp_custom", "test_service", nullptr, nullptr);

	obs_data_t *service_settings = obs_data_create();

	// Configure video encoder
	obs_data_t *vencoder_settings = obs_data_create();

	obs_data_set_int(vencoder_settings, "bitrate",
			 (int)(maxBitrateBitsPerSecond / 1000L));
	obs_data_set_string(vencoder_settings, "rate_control", "CBR");
	obs_data_set_string(vencoder_settings, "preset", "veryfast");
	obs_data_set_int(vencoder_settings, "keyint_sec", 2);

	obs_encoder_update(vencoder, vencoder_settings);

	obs_encoder_set_video(vencoder, obs_get_video());

	// Configure audio encoder
	obs_data_t *aencoder_settings = obs_data_create();

	obs_data_set_int(aencoder_settings, "bitrate", 128);

	obs_encoder_update(aencoder, aencoder_settings);

	obs_encoder_set_audio(aencoder, obs_get_audio());

	// Configure service

	obs_data_set_string(service_settings, "service", "rtmp_custom");
	obs_data_set_string(service_settings, "server", serverUrl.c_str());
	obs_data_set_string(service_settings, "key", streamKey.c_str());

	obs_data_set_bool(service_settings, "use_auth", useAuth);
	obs_data_set_string(service_settings, "username", authUsername.c_str());
	obs_data_set_string(service_settings, "password", authPassword.c_str());

	obs_service_update(service, service_settings);
	obs_service_apply_encoder_settings(service, vencoder_settings,
					   aencoder_settings);

	// Configure output

	const char *output_type = obs_service_get_output_type(service);

	if (!output_type)
		output_type = "rtmp_output";

	obs_data_t *output_settings = obs_data_create();

	if (bindToIP.size()) {
		obs_data_set_string(output_settings, "bind_ip", bindToIP.c_str());
	}

	m_output = obs_output_create(output_type, "test_stream",
				     output_settings, nullptr);

	obs_output_set_video_encoder(m_output, vencoder);
	obs_output_set_audio_encoder(m_output, aencoder, 0);

	obs_output_set_service(m_output, service);

	obs_data_release(output_settings);
	obs_data_release(service_settings);
	obs_data_release(vencoder_settings);
	obs_data_release(aencoder_settings);

	obs_output_start(m_output);

	return true;
}
