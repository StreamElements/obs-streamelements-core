# RazerWyvrnHapticComponent

One haptic effect within a [`RazerWyvrnEventInfo`](RazerWyvrnEventInfo.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| effect | string | Effect name, as written in the configuration. |
| loop | number | Loop count. |
| mixing | string | How this effect combines with others already playing, e.g. `"Merge"`. |
| priority | string | Relative priority, as written in the configuration. |
| targeting | [`RazerWyvrnHapticTarget`](RazerWyvrnHapticTarget.md)[] | Where on the body the effect lands. |
| url | string | Session-signed local URL for the `.haps` file. Absent when the file is not present on disk. |
