# Popup window

`window.host`

## `openPopupWindow(PopupWindowInfo, ResultCallback<success>)`

Open pop-up window with specified info.

**Data structures:** [`PopupWindowInfo`](../types/PopupWindowInfo.md)

## `setContainerForeignPopupWindowsProperties(ForeignPopupWindowsInfo, ResultCallback<success>)`

> ⚠️ **Removed.** Implemented once, but taken out in `f206595` (Eject dependency on CEF and obs-browser (#1), 2022-06-17). No handler by this name is registered any more, so calling it will not resolve.

Deprecated in API 3.0

Set properties for foreign pop-up windows (popup windows opened using standard HTML DOM API and not via this API)

**Data structures:** [`ForeignPopupWindowsInfo`](../types/ForeignPopupWindowsInfo.md)

## `getContainerForeignPopupWindowsProperties(ResultCallback<ForeignPopupWindowsInfo>)`

> ⚠️ **Removed.** Implemented once, but taken out in `f206595` (Eject dependency on CEF and obs-browser (#1), 2022-06-17). No handler by this name is registered any more, so calling it will not resolve.

Deprecated in API 3.0

Get properties for foreign pop-up windows (popup windows opened using standard HTML DOM API and not via this API)

**Data structures:** [`ForeignPopupWindowsInfo`](../types/ForeignPopupWindowsInfo.md)
