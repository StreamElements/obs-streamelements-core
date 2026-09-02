# RazerWyvrnHapticComponent

One haptic effect within a [`RazerWyvrnEventInfo`](RazerWyvrnEventInfo.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| effect | string | Effect name, as written in the configuration. |
| loop | number | Loop count. |
| mixing | string | How this effect combines with others already playing: `merge` or `override`. |
| priority | string | `veryLow`, `low`, `medium`, `high` or `veryHigh`. May be empty when the configuration omits it. |
| targeting | [`RazerWyvrnHapticTarget`](RazerWyvrnHapticTarget.md)[] | Where on the body the effect lands. |
| url | string | Session-signed local URL for the `.haps` file. Absent when the file is not present on disk. |

**Note:** `mixing` and `priority` are normalised to camelCase — see [Enum
values](../host/razer-wyvrn.md#enum-values).
