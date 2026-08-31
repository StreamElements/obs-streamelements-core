# User interface state

`window.host`

## `getUserInterfaceState(ResultCallback<UserInterfaceStateProperties>)`

**Available since API version 1.20**

Get current user interface state, this is used to save main window geometry and state of dock widgets.

## `setUserInterfaceState(UserInterfaceStateProperties , ResultCallback<success>)`

**Available since API version 1.20**

Set (restore) current user interface state, from data previously obtained from *getUserInterfaceState*.
