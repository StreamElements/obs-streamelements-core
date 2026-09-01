# Scene collections

`window.host`

## `getAllSceneCollections(ResultCallback<SceneCollectionInfo[]>)`

**Available since API version 1.22**

Get all scene collections in the “scene collections” menu.

**Data structures:** [`SceneCollectionInfo`](../types/SceneCollectionInfo.md)

## `getCurrentSceneCollectionProperties(ResultCallback<SceneCollectionInfo>)`

**Available since API version 1.22**

Get current scene collection properties. Takes no arguments.

Resolved by looking up the collection OBS reports as current
(`obs_frontend_get_current_scene_collection()`, which returns the *name*)
against the scene-collection files on disk, so the two fields come from
different places:

- `id` — the collection’s `.json` filename, without the extension, under
  `<config>/obs-studio/basic/scenes/`.
- `name` — the `name` field stored *inside* that file, which is what the
  Scene Collections menu shows.

Returns **null** rather than an object if no file matches — for instance if
the current collection’s file carries no `name` field, since a file without
one is skipped when the list is built.

Either field can be fed back to `setCurrentSceneCollectionById`, which
takes a plain string and matches it case-insensitively against the `name`
first and the `id` second — so despite the name, it accepts both.

`referencedFiles` is not populated here; it appears only in results from
`queryUserEnvironmentBackupReferencedFiles`.

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
