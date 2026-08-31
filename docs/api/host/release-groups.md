# Release groups

`window.host`

## `setHostReleaseGroupProperties(quality<ReleaseGroupInfo>, ResultCallback<success>)`

**Available since API version 1.11**

Set host release group properties. See ReleaseGroupInfo data structure for additional information.

## `getHostReleaseGroupProperties(ResultCallback<ReleaseGroupInfo>)`

**Available since API version 1.11**

Retrieve host release group properties. See ReleaseGroupInfo data structure for additional information.

**Note:** In case no release group information is available for the current computer, *ResultCallback* is called with *null* as parameter.

## `queryHostReleaseGroupUpdateAvailability([QuerySoftwareUpdateArgs], ResultCallback<success>)`

**Available since API version 1.11**

Query whether an update is available for the current host release group. If an update is available, the “OBS.Live needs an update” window will show.
