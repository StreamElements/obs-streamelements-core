#include "SEObsSceneUtils.hpp"

#include "../SETrace.hpp"

#include <obs.hpp>
#include <obs-frontend-api.h>

void ObsSceneEnumAllItems(obs_scene_t *scene,
			  std::function<bool(obs_sceneitem_t *)> func)
{
	struct local_context {
		std::vector<obs_sceneitem_t *> items;
	};

	local_context pass1_context;

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
		   void *param) {
			local_context *context = (local_context *)param;

			obs_sceneitem_addref(SETRACE_ADDREF(sceneitem));

			context->items.push_back(sceneitem);

			return true;
		},
		&pass1_context);

	local_context pass2_context;

	for (auto item : pass1_context.items) {
		pass2_context.items.push_back(item);

		if (obs_sceneitem_is_group(item)) {
			obs_sceneitem_group_enum_items(
				item,
				[](obs_scene_t *scene,
				   obs_sceneitem_t *sceneitem, void *param) {
					local_context *context =
						(local_context *)param;

					obs_sceneitem_addref(
						SETRACE_ADDREF(sceneitem));

					context->items.push_back(sceneitem);

					return true;
				},
				&pass2_context);
		}
	}

	bool keepCalling = true;

	for (auto item : pass2_context.items) {
		if (keepCalling) {
			keepCalling = func(item);
		}

		obs_sceneitem_release(SETRACE_DECREF(item));
	}
}
void ObsSceneEnumAllItems(obs_source_t *source,
			  std::function<bool(obs_sceneitem_t *)> func)
{
	if (!source)
		return;

	obs_scene_t *scene =
		obs_scene_from_source(source); // does not increment refcount

	ObsSceneEnumAllItems(scene, func);
}
void ObsCurrentSceneEnumAllItems(std::function<bool(obs_sceneitem_t *)> func)
{
	obs_source_t *sceneSource =
		SETRACE_ADDREF(obs_frontend_get_current_scene());

	if (!sceneSource)
		return;

	ObsSceneEnumAllItems(sceneSource, func);

	obs_source_release(SETRACE_DECREF(sceneSource));
}
void ObsEnumAllScenes(std::function<bool(obs_source_t *scene)> func)
{
	struct local_context {
		std::vector<obs_source_t *> list;
	};

	local_context context;

	obs_enum_scenes(
		[](void *data, obs_source_t *scene) -> bool {
			local_context *context = (local_context *)data;

			if (!obs_source_is_group(scene)) {
				context->list.push_back(SETRACE_ADDREF(
					obs_source_get_ref(scene)));
			}

			return true;
		},
		&context);

	for (auto scene : context.list) {
		if (!func(scene))
			break;
	}

	for (auto scene : context.list) {
		obs_source_release(SETRACE_DECREF(scene));
	}
}
