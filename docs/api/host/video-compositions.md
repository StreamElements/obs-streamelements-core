# Video Compositions

`window.host`

## `getAllVideoCompositions(ResultCallback<{ id: VideoCompositionInfo }>)`

Get all currently existing video compositions. The result of this API call is a map of video composition ID to *VideoCompositionInfo* structure.

**Available since API version 6.0**

## `removeVideoCompositionsByIds(Array<videoCompositionId>, ResultCallback<success>)`

Remove video compositions by their IDs. If an ID does not exist, it is ignored. If at least one video composition specified by an ID cannot be removed (*canRemove = false*), the whole call will fail.

**Available since API version 6.0**

## `addVideoComposition(VideoCompositionInfo, ResultCallback< VideoCompositionInfo | null>)`

Add a video composition. Returns the full composition info upon success, or *null* on failure. One of the reasons for failure may be a duplicate video composition ID.

**Available since API version 6.0**

## `setVideoCompositionProperties(VideoCompositionInfo, ResultCallback< VideoCompositionInfo | null>)`

Modifies video composition properties. Returns the full composition info upon success, or *null* on failure.

Currently only updating “name” is supported.

**Available since API version 6.0**
