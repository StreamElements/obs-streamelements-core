# Video Composition View Overlays

`window.host`

Video composition view overlays, are overlays projected on top of browser widgets at specified coordinates & dimensions.

To show a video composition view overlay, a *videoCompositionId* is required. This means that a video composition must already exist beforehand.

## `setVideoCompositionViewOverlayProperties(VideoCompositionViewOverlayInfo, ResultCallback<VideoCompositionViewOverlayInfo | null>)`

Show or modify current video composition view overlay over the current calling browser widget.

**Available since API version 6.0**

**Data structures:** [`VideoCompositionViewOverlayInfo`](../types/VideoCompositionViewOverlayInfo.md)

## `getVideoCompositionViewOverlayProperties(ResultCallback<VideoCompositionViewOverlayInfo | null>)`

Get current browser widget’s currently active video composition view overlay’s properties or *null* if none is active.

**Available since API version 6.0**

**Data structures:** [`VideoCompositionViewOverlayInfo`](../types/VideoCompositionViewOverlayInfo.md)

## `removeVideoCompositionViewOverlay(ResultCallback<success>)`

Remove current browser widget’s active video composition view overlay if it exists.

**Available since API version 6.0**
