#pragma once

#include <obs.h>

static inline bool
scanGroupSceneItems(obs_sceneitem_t *group,
		    std::function<bool(obs_sceneitem_t * /*item*/,
				       obs_sceneitem_t * /*parent*/)>
			    callback,
		    bool recursive)
{
	struct data_t {
		std::function<bool(obs_sceneitem_t * /*item*/,
				   obs_sceneitem_t * /*parent*/)>
			callback;
		bool recursive;
		obs_sceneitem_t *group;
		bool result;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;
	data.group = group;
	data.result = true;

	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *data_p) {
			auto data = (data_t *)data_p;

			if (data->recursive && obs_sceneitem_is_group(item)) {
				data->result = scanGroupSceneItems(
					item, data->callback, data->recursive);
			}

			if (data->result)
				data->result =
					data->callback(item, data->group);

			return data->result;
		},
		&data);

	return data.result;
}

static inline bool
scanSceneItems(obs_scene_t *scene,
	       std::function<bool(obs_sceneitem_t * /*item*/,
				  obs_sceneitem_t * /*parent*/)>
		       callback,
	       bool recursive)
{
	struct data_t {
		std::function<bool(obs_sceneitem_t * /*item*/,
				   obs_sceneitem_t * /*parent*/)>
			callback;
		bool recursive;
		bool result;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;
	data.result = true;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t * /* scene */, obs_sceneitem_t *item,
		   void *data_p) -> bool {
			auto data = (data_t *)data_p;

			if (data->recursive && obs_sceneitem_is_group(item)) {
				data->result = scanGroupSceneItems(
					item, data->callback, data->recursive);
			}

			if (data->result)
				data->result = data->callback(item, nullptr);

			return data->result;
		},
		&data);

	return data.result;
}
