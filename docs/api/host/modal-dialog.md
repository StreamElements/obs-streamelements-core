# Modal dialog

`window.host`

## `showModalDialog(DialogInfo, ResultCallback<object>)`

Open modal dialog with specified info, on result, execute specified callback.

**Data structures:** [`DialogInfo`](../types/DialogInfo.md)

## `endModalDialog(object, ResultCallback<success>)`

Within a modal dialog opened with showModalDialog(), signal the dialog has ended with the specified result. The first parameter value is the dialog result.
