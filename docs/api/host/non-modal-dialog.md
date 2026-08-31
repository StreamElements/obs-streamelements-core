# Non-Modal dialog

`window.host`

## `showNonModalDialog(DialogInfo, ResultCallback<object>)`

Open non-modal dialog with specified info, on result, execute specified callback.

**Available since API version 3.2**

**Data structures:** [`DialogInfo`](../types/DialogInfo.md)

## `endNonModalDialog(object, ResultCallback<success>)`

Within a modal dialog opened with showNonModalDialog(), signal the dialog has ended with the specified result. The first parameter value is the dialog result.

**Available since API version 3.2**

## `getAllNonModalDialogs(ResultCallback<{id: DialogInfo}>)`

Retrieve an object who’s keys are non-modal dialog *id*, and values are *DialogInfo* structures for all currently open non-modal dialogs.

**Available since API version 3.3**

**Data structures:** [`DialogInfo`](../types/DialogInfo.md)

## `closeNonModalDialogsByIds(array<id>, ResultCallback<success>)`

Accepts an array of non-modal dialog *id* and closes each matching non-modal dialog.

**Available since API version 3.3**

## `focusNonModalDialogById(id, ResultCallback<success>)`

Accepts an *id* of a non-modal dialog and focuses input on it (brings it to front, activates the window).

## `setNonModalDialogDimensionsById(id, DimensionsInfo, ResultCallback<success>)`

Accepts an *id* of a non-modal dialog and new *DimensionsInfo* and adjusts the non-modal dialog dimensions.

**Available since API version 3.3**

**Data structures:** [`DimensionsInfo`](../types/DimensionsInfo.md)
