# Scene collections

`window.host`

## `getAllSceneCollections(ResultCallback<SceneCollectionInfo[]>)`

**Available since API version 1.22**

Get all scene collections in the “scene collections” menu.

getCurrentSceneCollectionProperties(ResultCallback\<SceneCollectionInfo>)

**Available since API version 1.22**

Get current scene collection properties.

**Data structures:** [`SceneCollectionInfo`](../types/SceneCollectionInfo.md)

## `addSceneCollection(SceneCollectionInfo, ResultCallback<success>)`

**Available since API version 1.22**

Add a new scene collection according to specified info, and select the new scene collection.

**Warning:** as of OBS 24.0.3, this API is **broken** due to bugs in obs_frontend_add_scene_collection OBS front-end API.

Will be resolved in OBS 25.

**Data structures:** [`SceneCollectionInfo`](../types/SceneCollectionInfo.md)

## `setCurrentSceneCollectionById(sceneCollectionId, ResultCallback<success>)`

**Available since API version 1.22**

Sets current scene collection to the specified sceneCollectionId.
