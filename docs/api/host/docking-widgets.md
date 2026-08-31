# Docking widgets

`window.host`

## `addDockingWidget(DockingWidgetInfo, ResultCallback<widgetId>)`

Add docking widget to OBS UI according to specified info.

**Data structures:** [`DockingWidgetInfo`](../types/DockingWidgetInfo.md)

## `getAllDockingWidgets(ResultCallback<{id:DockingWidgetInfo}>)`

Get all docking browser widgets.

**Data structures:** [`DockingWidgetInfo`](../types/DockingWidgetInfo.md)

## `removeDockingWidgetsByIds(array<widgetId>, ResultCallback<success>)`

Remove docking browser widgets by their widgetIds.

## `toggleDockingWidgetFloatingById(widgetId, ResultCallback<success>)`

Available since API version 1.8

Toggle docking widget floating state by widget Id.

When the widget is floating, it behaves as a regular window. When it’s docked, it’s snapped to one of the corners of the main window.

## `setDockingWidgetDimensionsById(widgetId, DimensionsInfo, ResultCallback<success>)`

Available since API version 1.8

Set docking widget width and height by widget Id.

**Note:** setting widget dimensions is allowed only when the widget is in floating state.

**Data structures:** [`DimensionsInfo`](../types/DimensionsInfo.md)

## `setDockingWidgetPositionById(widgetId, PositionInfo, ResultCallback<success>)`

**Available since API version 1.8**

Set docking widget left and top coordinates by widget Id.

**Note:** setting widget position is allowed only when the widget is in floating state.

**Data structures:** [`PositionInfo`](../types/PositionInfo.md)

## `setDockingWidgetUrlById(widgetId, url, ResultCallback<success>)`

**Available since API version 1.23**

Set docking widget URL by widget Id.

**Note:** this method replaces the widget’s initial URL.

## `setDockingWidgetTitleById(widgetId, title, ResultCallback<success>)`

**Available since API version 6.0**

Set docking widget title by widget Id.

## `groupDockingWidgetsPairByIds(firstWidgetId, secondWidgetId, ResultCallback<success>)`

**Available since API version 3.1**

Group docking widgets in a single location.

Widget Ids can be specified either as IDs set at creation time by the API, or, meta-IDs corresponding to native OBS dock widgets:

| **Meta-ID** | **OBS Widget** |
| --- | --- |
| :scenesDock | Scenes |
| :sourcesDock | Sources |
| :mixerDock | Audio Mixer |
| :transitionsDock | Scene Transitions |
| :controlsDock | Controls |
| :statsDock | Stats |

## `insertDockingWidgetBeforeId(firstWidgetId, secondWidgetId, ResultCallback<success>)`

**Available since API version 3.1**

Reorder dock widgets so firstWidgetId appears **before** secondWidgetId, or, at the beginning of the list.

Widget Ids can be specified either as IDs set at creation time by the API, or, meta-IDs corresponding to native OBS dock widgets’ object names:

| **Meta-ID** | **OBS Widget** |
| --- | --- |
| :scenesDock | Scenes |
| :sourcesDock | Sources |
| :mixerDock | Audio Mixer |
| :transitionsDock | Scene Transitions |
| :controlsDock | Controls |
| :statsDock | Stats |

## `insertDockingWidgetAfterId(firstWidgetId, secondWidgetId, ResultCallback<success>)`

**Available since API version 3.1**

Reorder dock widgets so firstWidgetId appears **after** secondWidgetId, or, at the end of the list.

Widget Ids can be specified either as IDs set at creation time by the API, or, meta-IDs corresponding to native OBS dock widgets’ object names:

| **Meta-ID** | **OBS Widget** |
| --- | --- |
| :scenesDock | Scenes |
| :sourcesDock | Sources |
| :mixerDock | Audio Mixer |
| :transitionsDock | Scene Transitions |
| :controlsDock | Controls |
| :statsDock | Stats |

## `showDockingWidgetById(string<widgetId>, ResultCallback<success>)`

Show docking widget by its *widgetId*.

## `hideDockingWidgetById(string<widgetId>, ResultCallback<success>)`

Hide docking widget by its *widgetId*.
