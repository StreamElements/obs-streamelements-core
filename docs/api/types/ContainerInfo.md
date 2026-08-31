# ContainerInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| Id | string | Container (widget) id, empty for central widget |
| dockingArea | string | Docking area: “left”, “top”, “right”, “bottom”, “floating” for docking widgets. “none” for non-docking instances (pop-ups, dialogs, notification bar, central widget, etc.) |
| url | string | Content URL |
| theme | string | Name of OBS theme |
| type | string | dockingWidget<br>nonModalDialog<br>modalDialog<br>popupWindow<br>backgroundWorker<br>globalInvokeHandler<br>centralWidget<br>notificationBar<br>unknown |
