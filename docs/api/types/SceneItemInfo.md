# SceneItemInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Scene Item Id.<br>This is set by the API and is ignored when specified by the caller unless updating an existing Scene Item.<br>The value is not globally unique and may change between OBS sessions.<br>**API 1.21+** |
| videoCompositionId | string | Optional. ID of the video composition to which the scene item’s scene belongs.<br>**API 6.0+** |
| sceneId | string | Optional. Write-only **until API 6.0**.<br>Used by addSceneItemXXXSource() methods to determine which Scene to add a Scene Item (source / group) to.<br>Default: current active scene.<br>**API 1.26+** |
| parentId | string | Id of a Scene Item Group to which we are adding this Scene Item.<br>**API 1.21+** |
| name | string | Scene item name, as seen in “Sources” OBS widget. |
| class | string | OBS source class ID<br>Useful source classes:<br>‘game_capture’ – game capture source<br>‘browser_source’ – browser source<br>‘dshow_input’ – DirectShow video capture<br>‘group’ – Group of scene items<br>**Ignored when adding a specific scene item type.** |
| visible | bool | True if scene item is visible, otherwise false<br>**API 1.21+** |
| selected | bool | True if scene item is selected, otherwise false<br>**API 1.21+** |
| locked | bool | True if scene item is locked, otherwise false<br>**API 1.21+** |
| preferExistingSourceReference | bool | If *true*, prefer an existing source of the same *class* to be referenced instead of creating a new one.<br>Defaults to *false*.<br>Useful when adding a video capture source or any other singleton exclusive source type.<br>**API 1.21+** |
| existingSourceId | string | **Optional**. If exists, and matches a source id returned by *getAllExistingVideoInputSources*, the matching existing source will be referenced instead of creating a new one.<br>Useful when adding a video capture source or any other source without duplicating it.<br>**API 6.0+** |
| order | int | Indicates scene item composition (layering) order when enumerating scene items or setting scene item properties.<br>0 = the most bottom layer<br>0+N = upper layers<br>**API 1.21+** |
| items | SceneItemInfo[] | Group items when ‘class’ = ‘group’<br>**API 1.21+** |
| settings | object | OBS source settings, may vary from source to source. See addCurrentSceneItemXXXSource() methods for more details. |
| auxiliaryData | any | Optional. Auxiliary application data to associate with this SceneItem.<br>**API 1.24+** |
| icon | IconInfo | The icon associated with this Scene Item.<br>**API 1.24+**<br>**Removed in API 4.0+** |
| defaultAction | ActionInfo | Default action associated with this Scene Item to invoke when user double-clicks with their mouse on the scene item.<br>**API 1.24+**<br>**Removed in API 4.0+** |
| contextMenu | MenuItemInfo[] | Context menu associated with this Scene Item to invoke when user right-clicks with their mouse on the scene item.<br>**API 1.24+**<br>**Removed in API 4.0+** |
| actions | ActionInfo[] | Actions associated with this Scene Item.<br>**API 1.24+**<br>**Removed in API 4.0+**<br>Actions will be displayed in OBS Sources list. |
| uiSettings | object | Scene item UI behavior settings:<br>‘**enabled’** – true/false, if false – scene item is disabled & unselectable. Default: true.<br>‘**opacity’** – 0.0 – 1.0. scene item opacity in scene items (sources) list. Default: 1.0.<br>**API 1.32+**<br>**Removed in API 4.0+**<br>**‘color’** – HEX color or color name (“#RRGGBB” or “red”) of the source name color.<br>**API 2.1+**<br>**Removed in API 4.0+** |
| composition | object | OBS scene item composition info:<br>‘srcWidth’ – source native width, *ignored when settings props.*<br>‘srcHeight’ – source native height, *ignored when setting props.*<br>‘position’ – object {<br>‘x’ – x coordinate<br>‘y’ – y coordinate<br>}<br>‘alignment’ – ‘top_left’, ‘bottom_right’, ‘top_center’, ‘center_right’, ‘center’, etc. **API 1.21+**<br>‘scale’ – object {<br>‘x’ – width scale factor<br>‘y’ – height scale factor<br>}<br>‘rotationDegrees’ – rotation degrees<br>‘crop’ – object {<br>    ‘left’ – left cropping (pixels)<br>‘top’– top cropping (pixels)<br>‘right’ – right cropping (pixels)<br>‘bottom’ – bottom cropping (pixels)<br>}<br>‘boundsType’ – ‘none’, ‘stretch’, ‘scale_to_inner_rect’, ‘scale_to_outer_rect’, ‘scale_to_width’, ‘scale_to_height’, ‘max_size_only’. **API 1.21+**<br>‘bounds’ – object { ‘x’, ‘y’ } – size of bounding box. Valid when ‘boundsType’ != ‘none’. **API 1.21+**<br>‘boundsAlignment’ – alignment within bounding box. Valid when ‘boundsType’ != ‘none’. **API 1.21+**<br>srcWidth * scaleX = actualWidth<br>srcHeight * scaleY = actualHeight<br>**Ignored when adding a scene item.** |
| hasAudio | bool | **Read-only**. Indicates whether the source has audio output. |
| hasVideo | bool | **Read-only**. Indicates whether the source has video output. |
| isVideoCaptureDevice | bool | **Read-only**. Indicates whether the source is a video capture device. |
| isGameCaptureDevice | bool | **Read-only**. Indicates whether the source is a game capture source. |
| isBrowserSource | bool | **Read-only**. Indicates whether the source is a browser source. |
| isFilterSource | bool | **Read-only**. Indicates whether the source is a filter.<br>**API 6.0+** |
| isInputSource | bool | **Read-only**. Indicates whether the source is an input source.<br>**API 6.0+** |
| isTransitionSource | bool | **Read-only**. Indicates whether the source is a transition.<br>**API 6.0+** |
| isSceneSource | bool | **Read-only**. Indicates whether the source is a scene.<br>**API 6.0+** |
| filters | InputSourceTypeInfo[] | List of Filters which are applied to the scene item’s **source**.<br>**Note**: modifying this list on a Scene Item, will effectively modify it on the OBS source which is referenced by the item: i.e. **may cause side effects on other scene items**.<br>**API 6.0+** |
