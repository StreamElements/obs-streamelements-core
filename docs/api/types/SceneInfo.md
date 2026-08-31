# SceneInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Scene Id.<br>This is set by the API and is ignored when specified by the caller.<br>The value is not globally unique and may change between OBS sessions. |
| videoCompositionId | string | **Optional**. Specifies which video composition the scene belongs to.<br>**API 6.0+** |
| name | string | Scene name, as seen in the “Scenes” OBS widget. |
| icon | IconInfo | Optional. Icon to display next to scene name.<br>**API 1.24+**<br>**Removed in API 4.0+** |
| active | bool | Optional. Specifies whether this Scene is currently the active scene in OBS.<br>When creating scenes, this property specifies whether the new scene will become the active scene.<br>Default value: true<br>**API 1.26+** |
| defaultAction | ActionInfo | Default action associated with this Scene to invoke when user double-clicks with their mouse on the scene item.<br>**API 1.24+**<br>**Removed in API 4.0+** |
| contextMenu | MenuItemInfo[] | Context menu associated with this Scene to invoke when user right-clicks with their mouse on the scene item.<br>**API 1.24+**<br>**Removed in API 4.0+** |
| auxiliaryData | any | Optional. Auxiliary application data to associate with this Scene.<br>**API 1.24+** |
