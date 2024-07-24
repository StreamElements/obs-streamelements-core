#pragma once

#include <obs.h>

static inline void
scanGroupSceneItems(obs_sceneitem_t *group,
		    std::function<void(obs_sceneitem_t * /*item*/,
				       obs_sceneitem_t * /*parent*/)>
			    callback,
		    bool recursive)
{
	struct data_t {
		std::function<void(obs_sceneitem_t * /*item*/,
				   obs_sceneitem_t * /*parent*/)>
			callback;
		bool recursive;
		obs_sceneitem_t *group;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;
	data.group = group;

	obs_sceneitem_group_enum_items(
		group,
		[](obs_scene_t *scene, obs_sceneitem_t *item, void *data_p) {
			auto data = (data_t *)data_p;

			data->callback(item, data->group);

			if (data->recursive && obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(item, data->callback, data->recursive);
			}

			return true;
		},
		&data);
}

static inline void scanSceneItems(
	obs_scene_t* scene,
	std::function<void(obs_sceneitem_t * /*item*/, obs_sceneitem_t * /*parent*/)>
		callback,
	bool recursive)
{
	struct data_t {
		std::function<void(obs_sceneitem_t * /*item*/,
				   obs_sceneitem_t * /*parent*/)>
			callback;
		bool recursive;
	};

	data_t data = {};
	data.callback = callback;
	data.recursive = recursive;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t * /* scene */, obs_sceneitem_t *item,
		   void *data_p) {
			auto data = (data_t *)data_p;

			if (data->recursive && obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(item, data->callback,
						    data->recursive);
			}

			data->callback(item, nullptr);

			return true;
		},
		&data);
}
