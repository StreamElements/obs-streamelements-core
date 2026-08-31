# Menu

`window.host`

## `setAuxiliaryMenuItems(MenuItemInfo[], ResultCallback<success>)`

**Available since API version 1.22**

Set auxiliary menu items.

Auxiliary menu items will appear among other items under the StreamElements menu in OBS and allow invoking API methods described in this document as their actions.

See *MenuItemInfo* for further details.

**Note**: auxiliary menu items will be reset upon host state reset (logout, after version upgrade, etc.).

**Data structures:** [`MenuItemInfo`](../types/MenuItemInfo.md)

## `getAuxiliaryMenuItems(ResultCallback<MenuItemInfo[]>)`

**Available since API version 1.22**

Get currently active auxiliary menu items previously set by *setAuxiliaryMenuItems*.

**Data structures:** [`MenuItemInfo`](../types/MenuItemInfo.md)

## `setShowBuiltInMenuItems(bool<show>, ResultCallback<success>)`

**Available since API version 1.38**

Specify whether to show built-in menu items.

## `getShowBuiltInMenuItems(ResultCallback<bool>)`

**Available since API version 1.38**

Get show built-in menu items state.
