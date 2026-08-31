# DockingWidgetInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Docking widget Id.<br>This is set by the API and is ignored when specified by the caller.<br>The value is not globally unique and may change between OBS sessions. |
| url | string | Docking widget content URL |
| dockingArea | string | Docking area: “left”, “right”, “top”, “bottom” or “floating” for ‘not docking at the moment’. |
| title | string | Docking widget title |
| visible | bool | Indicates whether the docking widget is visible |
| executeJavaScriptOnLoad | string | *Optional*. JavaScript code to execute on *each* page load |
| reloadPolicy | string | *Optional.* Either “reload” or “navigate”. If set to “navigate”, reload button will navigate the dock widget browser to the initial URL set when adding the widget, otherwise will just reload the current page from server.<br>Default: “reload”<br>**API 1.31+** |

**Optional properties**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| minWidth | int | Minimum width |
| minHeight | int | Minimum height |
| width | int | Initial width |
| height | int | Initial height |

**Note**: when dockingArea is ‘left’ or ‘right’, it is recommended to set ‘width’ and ‘minWidth’. When dockingArea is ‘top’ or ‘bottom’ it is recommended to set ‘height’ and ‘minHeight’. When dockingArea is ‘floating’, set all optional properties.
