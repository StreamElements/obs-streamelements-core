#pragma once

#include <obs.h>
#include <obs-frontend-api.h>
//#include <media-io/video-io.h>

#include <mutex>
#include <memory>
#include <string>

#include "cef-headers.hpp"

class StreamElementsObsOutput;

class StreamElementsObsOutputManager {
public:
	StreamElementsObsOutputManager() {}
	~StreamElementsObsOutputManager() {}

public:
	//void DeserializeOutput(CefRefPtr<CefValue> input,
	//		       CefRefPtr<CefValue> output);

	std::string AddSceneOutput(obs_scene_t *scene, int view_width, int view_height);
	bool StartOutputById(std::string id);
	bool StopOutputById(std::string id);

private:
	std::recursive_mutex m_mutex;
	std::map<std::string, std::shared_ptr<StreamElementsObsOutput>>
		m_outputs;
};


class StreamElementsObsOutput {
public:
	StreamElementsObsOutput(obs_scene_t *scene, int view_width,
				int view_height)
	{
		m_container_scene = obs_scene_create_private(
			GetComputerSystemUniqueId().c_str());

		obs_source_t *source_to_render =
			scene ? obs_scene_get_source(scene)
			      : obs_frontend_get_current_scene();

		obs_sceneitem_t *item =
			obs_scene_add(m_container_scene, source_to_render);

		// Get container scene source
		obs_source_t *container_source =
			obs_scene_get_source(m_container_scene);

		// Calculate aspect ratio
		double aspect_ratio =
			(double)view_width / (double)view_height;

		int width = obs_source_get_width(source_to_render);
		int height = obs_source_get_height(source_to_render);

		// TODO: Calculate crop

		// Create crop filter
		obs_data_t *crop_settings = obs_data_create();

		obs_data_set_bool(crop_settings, "relative", false);
		obs_data_set_int(crop_settings, "left", 200);
		obs_data_set_int(crop_settings, "top", 0);
		obs_data_set_int(crop_settings, "right", 200);
		obs_data_set_int(crop_settings, "bottom", 0);
		obs_data_set_int(crop_settings, "cx", 0);
		obs_data_set_int(crop_settings, "cy", 0);

		obs_source_t *crop_filter_source = obs_source_create(
			"crop_filter", "Crop", crop_settings, nullptr);

		obs_data_release(crop_settings);

		obs_source_filter_add(container_source, crop_filter_source);
	}

	~StreamElementsObsOutput()
	{
		Stop();

		obs_scene_release(m_container_scene);
	}

public:
	bool Start();
	void Stop();

private:
	std::recursive_mutex m_mutex;
	obs_scene_t *m_container_scene = nullptr;
	obs_view_t *m_view = nullptr;
	video_t *m_video = nullptr;

	obs_output_t *m_output = nullptr;
};
