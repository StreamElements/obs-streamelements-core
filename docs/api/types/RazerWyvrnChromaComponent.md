# RazerWyvrnChromaComponent

One lighting effect within a [`RazerWyvrnEventInfo`](RazerWyvrnEventInfo.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| effect | string | Effect name, as written in the configuration. |
| interrupt | bool | Whether this effect interrupts whatever is currently playing. |
| assets | [`RazerWyvrnChromaAsset`](RazerWyvrnChromaAsset.md)[] | The `.chroma` files backing this effect, one per device variant present on disk. |

**Note:** `assets` lists what actually exists rather than what a naming
convention implies — not every effect ships every device.
