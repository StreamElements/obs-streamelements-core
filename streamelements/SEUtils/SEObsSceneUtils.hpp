#pragma once

//
// Traversal over OBS scenes and scene items. Needs libobs, so this one is not
// a candidate for the test build; it is here so that callers who only walk
// scenes do not have to drag in the rest of SEUtils.
//

#include <obs.h>

#include <functional>

void ObsSceneEnumAllItems(obs_scene_t *scene,
			  std::function<bool(obs_sceneitem_t *)> func);

void ObsSceneEnumAllItems(obs_source_t *source,
			  std::function<bool(obs_sceneitem_t *)> func);

void ObsCurrentSceneEnumAllItems(std::function<bool(obs_sceneitem_t *)> func);

/* ========================================================= */

void ObsEnumAllScenes(std::function<bool(obs_source_t *scene)> func);
