# VideoCompositionViewOverlayInfo

**API 6.0+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| videoCompositionId | string | ID of the video composition which we are going to render in the overlay. |
| geometry | object (see below) | The rectangular area the overlay occupies.<br>The overlay area will include a rendering of the video composition, and a padding around it. |
| geometry.left | number | Left position in pixels |
| geometry.top | number | Top position in pixels |
| geometry.width | number | Width in pixels |
| geometry.height | number | Height in pixels |
