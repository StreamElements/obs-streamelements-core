# Scene items

`window.host`

## `getAllCurrentSceneItems(ResultCallback<SceneItemInfo[]>)`

**Available since API version 1.10**

Get all items in the current scene.

## `getAllSceneItems(SceneInfo, ResultCallback<SceneItemInfo[]>)`

**Available since API version 1.26**

Get all items in the scene specified by *SceneInfo.id*

## `removeCurrentSceneItemsByIds(array<sceneItemId>, ResultCallback<success>)`

**Available since API version 1.21**

Remove items from the current scene by their Ids.

## `removeSceneItemsByIds(array<sceneItemId>, ResultCallback<success>)`

**Available since API version 1.26**

Remove items from any scene by their Ids.

## `setCurrentSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`

**Available since API version 1.21**

Set scene item properties described by SceneItemInfo.

**Note:** the scene item to update is located by the value of *SceneItemInfo.id*.

## `setSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`

**Available since API version 1.26**

Set scene item properties described by SceneItemInfo.

**Note:** the scene item to update is located by the value of *SceneItemInfo.id*.

## `getCurrentSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`

**Available since API version 6.0**

Get scene item properties described by SceneItemInfo.

This call will retrieve _ALL_ available information for the *SceneItemInfo*, including *properties* which are unavailable elsewhere.

The reason we do this is that some source plug-ins do not implement *obs_source_get_properties* properly, and we want to minimize the amount of calls to this OBS API while still allowing fine-grained control where applicable.

**Note:** the scene item to retrieve is located by the value of *SceneItemInfo.id*.

## `getSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`

**Available since API version 6.0**

Get scene item properties described by SceneItemInfo.

This call will retrieve _ALL_ available information for the *SceneItemInfo*, including *properties* which are unavailable elsewhere.

The reason we do this is that some source plug-ins do not implement *obs_source_get_properties* properly, and we want to minimize the amount of calls to this OBS API while still allowing fine-grained control where applicable.

**Note:** the scene item to retrieve is located by the value of *SceneItemInfo.id*.

## `addCurrentSceneItemGameCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.21**

Add a game capture source scene item (id: “game_capture”)

**Allowed values for** ***SceneItemInfo.settings*** **(all optional)**

| **Option** | **Type** | **Description** |
| --- | --- | --- |
| sli_compatibility | bool | SLI/Crossfire capture mode (slow) |
| allow_transparency | bool | Allow transparency |
| limit_framerate | bool | Limit capture framerate |
| capture_cursor | bool | Capture cursor |
| anti_cheat_hook | bool | Use anti-cheat compatibility hook |
| capture_overlays | bool | Capture third-party overlays (such as steam) |

## `addSceneItemGameCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.26**

Add a game capture source scene item (id: “game_capture”)

**Allowed values for** ***SceneItemInfo.settings*** **(all optional)**

| **Option** | **Type** | **Description** |
| --- | --- | --- |
| sli_compatibility | bool | SLI/Crossfire capture mode (slow) |
| allow_transparency | bool | Allow transparency |
| limit_framerate | bool | Limit capture framerate |
| capture_cursor | bool | Capture cursor |
| anti_cheat_hook | bool | Use anti-cheat compatibility hook |
| capture_overlays | bool | Capture third-party overlays (such as steam) |

**Note**: scene to add the game capture source to is specified by *SceneItemInfo.sceneId*

## `addCurrentSceneItemBrowserSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.10**

Add browser source scene item (source class: “browser_source”)

**Allowed values for SceneItemInfo.settings**

| **Option** | **Type** | **Description** |
| --- | --- | --- |
| url | string | Content URL |
| css | string | Additional CSS<br>**Optional since API 1.21** |
| width | number | Browser width |
| height | number | Browser height |
| fps | number | Capture frames per second<br>**Note**: this should be aligned with global video framerate<br>**Optional since API 1.21** |
| fps_custom | bool | If true, the value of ‘fps’ field is honored<br>Optional |
| shutdown | bool | Shutdown source when not visible<br>Optional |
| restart_when_active | bool | Refresh browser when scene becomes active<br>Optional |
| reroute_audio | bool | Control audio via OBS<br>Optional |

## `addSceneItemBrowserSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.26**

Add browser source scene item (source class: “browser_source”)

**Allowed values for SceneItemInfo.settings**

| **Option** | **Type** | **Description** |
| --- | --- | --- |
| url | string | Content URL |
| css | string | Additional CSS<br>**Optional since API 1.21** |
| width | number | Browser width |
| height | number | Browser height |
| fps | number | Capture frames per second<br>**Note**: this should be aligned with global video framerate<br>**Optional since API 1.21** |
| fps_custom | bool | If true, the value of ‘fps’ field is honored<br>Optional |
| shutdown | bool | Shutdown source when not visible<br>Optional |
| restart_when_active | bool | Refresh browser when scene becomes active<br>Optional |
| reroute_audio | bool | Control audio via OBS<br>Optional |

**Note**: scene to add the browser source to is specified by *SceneItemInfo.sceneId*

## `addCurrentSceneItemVideoCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.21**

Add video capture source scene item (source id: “dshow_input” on Windows)

**Required values for** ***SceneItemInfo.settings***

| **Option** | **Type** | **Description** |
| --- | --- | --- |
| video_device_id | string | Video capture device identifier obtained via getAvailableVideoCaptureDevices() call |

**Note:** you should set *SceneItemInfo.preferExistingSourceReference = true* when adding video capture sources.

## `addSceneItemVideoCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.26**

Add video capture source scene item (source id: “dshow_input” on Windows)

**Required values for** ***SceneItemInfo.settings***

| **Option** | **Type** | **Description** |
| --- | --- | --- |
| video_device_id | string | Video capture device identifier obtained via getAvailableVideoCaptureDevices() call |

**Note:** you should set *SceneItemInfo.preferExistingSourceReference = true* when adding video capture sources.

**Note**: scene to add the video capture source to is specified by *SceneItemInfo.sceneId*

## `addCurrentSceneItemObsNativeSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.21**

Add any OBS source

**Allowed values for SceneItemInfo.settings** are specific to each sourceId.

*SceneItemInfo.preferExistingSourceReference* – Boolean value indicating whether an existing source should be referenced if it exists. This is useful for video capture sources where only one capture source can access the capture device at a time.

**Note**: this is the *only* API call which supports referencing existing sources by ID (via *SceneItemInfo.existingSourceId* field of the SceneItemInfo structure). You can obtain the correct value for this field by calling *getAllExistingVideoInputSources*.

When using this method, you must also specify *SceneItemInfo.name*, *SceneItemInfo.class*, and *SceneItemInfo.settings*. This is a limitation of how the API is currently constructed due to time constraints, since those fields are not really used when adding a scene item source by reference.

**Note**: using this method is **not recommended** since it requires the JavaScript code to assume knowledge regarding OBS internal identifiers. At the moment of this writing, those identifiers are not documented by the OBS project, and therefor are not guaranteed to be immutable.

This method is provided as **last resort**, where time-to-market is of critical importance and conventional approach fails.

**One should plan for replacing the call to this method with a more streamlined alternative as soon as it becomes available.**

## `addSceneItemObsNativeSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.26**

Add any OBS source

**Allowed values for SceneItemInfo.settings** are specific to each sourceId.

*SceneItemInfo.preferExistingSourceReference* – Boolean value indicating whether an existing source should be referenced if it exists. This is useful for video capture sources where only one capture source can access the capture device at a time.

**Note**: using this method is **not recommended** since it requires the JavaScript code to assume knowledge regarding OBS internal identifiers. At the moment of this writing, those identifiers are not documented by the OBS project, and therefor are not guaranteed to be immutable.

This method is provided as **last resort**, where time-to-market is of critical importance and conventional approach fails.

**One should plan for replacing the call to this method with a more streamlined alternative as soon as it becomes available.**

**Note**: scene to add the OBS Native source to is specified by *SceneItemInfo.sceneId*

## `addCurrentSceneItemGroup(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.21**

Add OBS scene item group to the list of scene items.

**Note**: only *SceneItemInfo.name* and *SceneItemInfo.composition* members are honored and required.

## `addSceneItemGroup(SceneItemInfo, ResultCallback<SceneItemInfo>)`

**Available since API version 1.26**

Add OBS scene item group to the list of scene items.

**Note**: only *SceneItemInfo.name* and *SceneItemInfo.composition* members are honored and required.

**Note**: scene to add the source group to is specified by *SceneItemInfo.sceneId*

## `getAvailableInputSourceClasses(ResultCallback<string[]>)`

**Available since API version 1.21**

Retrieve list of available source classes for use with addCurrentSceneItemObsNativeSource.

## `getSourceClassProperties(SceneItemInfo, RestulCallback<ObsPropertyInfo[]>)`

**Available since API version 1.21**

Retrieve list of settings properties which a specified source class expects along with extra metadata.

**Note:** the only *SceneItemInfo* properties respected by this API method are *SceneItemInfo.class* (required) and *SceneItemInfo.settings* (optional).

## `ungroupCurrentSceneItemGroupById(SceneItemInfo, ResultCallback<success>)`

**Available since API version 1.21**

Extract scene items from scene item group referenced by *SceneItemInfo.id* into the top level and remove the scene item group.

**Note:** only *SceneItemInfo.id* is respected by the API method.

## `ungroupSceneItemGroupById(SceneItemInfo, ResultCallback<success>)`

**Available since API version 1.26**

Extract scene items from scene item group referenced by *SceneItemInfo.id* into the top level and remove the scene item group.

**Note:** only *SceneItemInfo.id* is respected by the API method.

## `invokeCurrentSceneItemDefaultActionById(string<sceneItemId>, ResultCallback<success>)`

**Available since API version 1.24**

Invoke scene item’s default action (the OBS source properties dialog) for the specified *sceneItemId*.

This method is useful when *SceneItemInfo.defaultAction* for the item was set, but you still want to invoke the native source properties dialog.

## `invokeCurrentSceneItemDefaultContextMenuById(string<sceneItemId>, ResultCallback<success>)`

**Available since API version 1.24**

Invoke scene item’s default context menu (the OBS source context menu) for the specified *sceneItemId*.

This method is useful when *SceneItemInfo.contextMenu* for the item was set, but you still want to invoke the native source context menu.

## `setCurrentSceneItemsAuxiliaryActions(ActionInfo[], ResultCallback<success>)`

**Available since API version 1.24**

Set auxiliary actions available at the bottom-right corner of the OBS Scene Items (Sources) dock.

## `getCurrentSceneItemsAuxiliaryActions(ResultCallback<ActionInfo[]>)`

**Available since API version 1.24**

Get auxiliary actions available at the bottom-right corner of the OBS Scene Items (Sources) dock.

## `openSceneItemPropertiesDialogById(string<sceneItemId>, ResultCallback<success>)`

**Available since API version 6.0**

Open OBS native source properties dialog for the specified scene item ID.

## `openSceneItemFiltersDialogById(string<sceneItemId>, ResultCallback<success>)`

**Available since API version 6.0**

Open OBS native source filters dialog for the specified scene item ID.

## `openSceneItemInteractionDialogById(string<sceneItemId>, ResultCallback<success>)`

**Available since API version 6.0**

Open OBS native source interaction dialog for the specified scene item ID.

## `openSceneItemTransformEditorDialogById(string<sceneItemId>, ResultCallback<success>)`

**Available since API version 6.0**

Open OBS native source transform editor dialog for the specified scene item ID.

## `getSceneItemRotationInViewport(SceneItemInfo or ViewportSceneItemRotationInfo, ResultCallback<ViewportSceneItemRotationInfo>)`

**Available since API version 6.0**

Retrieve scene item rotation in absolute degrees (rotation relative to viewport, taking any additional rotations from parent groups into account).

## `setSceneItemRotationInViewport(ViewportSceneItemRotationInfo, ResultCallback< ViewportSceneItemRotationInfo>)`

**Available since API version 6.0**

Set scene item rotation in absolute degrees (rotation relative to viewport, taking any additional rotations from parent groups into account).

**NOTE: Rotation will always be performed around the item’s center**.

## `getSceneItemBoundingBoxInViewport(SceneItemInfo or ViewportSceneItemGeometryInfo, ResultCallback<ViewportSceneItemGeometryInfo>)`

**Available since API version 6.0**

Retrieve scene item bounding box in absolute coordinates (relative to viewport top-left (0,0), taking any additional transformations from parent groups and the scene item itself into account).

## `setSceneItemPositionInViewport(ViewportSceneItemGeometryInfo, ResultCallback< ViewportSceneItemGeometryInfo>)`

**Available since API version 6.0**

Set scene item (top, left) position in absolute coordinates (relative to viewport top-left (0,0), taking any additional transformations from parent groups and the scene item itself into account).
