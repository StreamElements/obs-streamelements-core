# Scenes

`window.host`

## `getAllScenes({ videoCompositionId }, ResultCallback<SceneInfo[]>)`

**Available since API version 1.21**

Get all scenes in the scene collection.

## `getAllScenes({ videoCompositionId }, ResultCallback<SceneInfo[]>)`

**Available since API version 6.0**

Get all scenes in the specified *videoCompositionId*.

## `getCurrentScene(ResultCallback<SceneInfo>)`

**Available since API version 1.21**

Get current scene info.

## `getCurrentScene({ videoCompositionId }, ResultCallback<SceneInfo>)`

**Available since API version 6.0**

Get current scene info in the specified *videoCompositionId*.

## `addScene(SceneInfo, ResultCallback<sceneId>)`

**Available since API version 1.21**

Add a scene to the scene collection according to specified info.

## `setCurrentSceneById(sceneId, ResultCallback<success>)`

**Available since API version 1.21**

Set current scene by Id.

This API call will find the relevant Video Composition automatically.

## `removeScenesByIds(array<sceneId>, ResultCallback<success>)`

**Available since API version 1.21**

Remove scenes by their Ids.

**Note**: the last scene in the collection cannot be removed. You must add a scene to replace it first, and then remove the remnant scene.

## `setScenePropertiesById(SceneInfo, ResultCallback<success>)`

**Available since API version 1.21**

Set scene properties by scene Id (*SceneInfo.id* specifies the scene Id to update).

## `setScenesAuxiliaryActions(ActionInfo[], ResultCallback<success>)`

**Available since API version 1.24**

Set Scenes list auxiliary actions (shown in the bottom-right corner of the OBS Scenes dock).

## `getScenesAuxiliaryActions(ResultCallback<ActionInfo[]>)`

**Available since API version 1.24**

Get Scenes list auxiliary actions (shown in the bottom-right corner of the OBS Scenes dock).
