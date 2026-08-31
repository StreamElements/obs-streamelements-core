# MenuItemInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| type | string | “**separator**” – menu separator<br>“**command**” – menu command item<br>“**container**” – container for a submenu: with this item, either “*items*” or “*itemsSource*” property is required. |
| title | string | Menu item title.<br>Ignored when type = “separator”. |
| enabled | bool | Optional. Indicates whether menu item is enabled or disabled.<br>Default: true<br>**API 1.30+** |
| icon.url | string | Optional. Indicates which URL to load an item’s icon from. |
| invoke | string | Menu item action.<br>Valid only when type = “command”.<br>The value of this field corresponds to one of API method names described in this document.<br>Examples:<br>“showModalDialog”, “showCentralWidget”, “addDockingWidget”, “openPopupWindow”, etc. |
| invokeArgs | array | Arguments to the API method described by *invoke* as described by the API call documentation in this document.<br>Valid only when type = “command”. |
| items | MenuItemInfo[] | Applies when type = “container”<br>Array of *MenuItemInfo* objects describing a submenu. |
| itemsSource | string | Applies when type = “container”, appears instead of “items”.<br>Specifies an internal source of container items.<br>Possible values:<br>“**:dockingWidgets**” – list of docking widgets with their state (visible/hidden)<br>**API 1.38+** |
