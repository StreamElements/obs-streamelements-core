# ViewportSceneItemRotationInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| videoCompositionId | string | ID of the video composition which the scene item belongs to. |
| id | string | ID of the scene item |
| viewportRotationDegrees | number | **Optional**. Scene item rotation in degrees (>=0, \<360), relative to the viewport coordinates (i.e. if scene item belongs to a group which is rotated, the rotation degrees of the item will be summed with the group’s rotation degrees).<br>This field is specified when setting rotation degrees, and is returned by the API when retrieving rotation degrees. |
