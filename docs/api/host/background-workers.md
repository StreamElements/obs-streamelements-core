# Background workers

`window.host`

Background workers are invisible browsers (without a user interface).

Their behavior is similar to docking widgets: their state is saved across sessions when in normal mode and they are disposed of when state switches to on-boarding mode.

The background worker HTML code is supplied directly when adding a new worker and not loaded from a remote server. This allows the background worker to be self-contained and independent of Internet connection availability and quality.

The URL supplied as part of the background worker initialization is used to determine the *security context* (i.e. accessible cookies, CORS origin, etc.) of the background worker browser.

## `addBackgroundWorker(BackgroundWorkerInfo, ResultCallback<workerId>)`

Add a background worker according to specified info.

**Data structures:** [`BackgroundWorkerInfo`](../types/BackgroundWorkerInfo.md)

## `getAllBackgroundWorkers(ResultCallback<{id:BackgroundWorkerInfo}>)`

Get all background workers.

**Data structures:** [`BackgroundWorkerInfo`](../types/BackgroundWorkerInfo.md)

## `removeBackgroundWorkersByIds(array<workerId>, ResultCallback<success>)`

Remove background workers by their workerIds.
