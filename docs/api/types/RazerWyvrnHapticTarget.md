# RazerWyvrnHapticTarget

One body target of a
[`RazerWyvrnHapticComponent`](RazerWyvrnHapticComponent.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| target | string | Body region, e.g. `"Chest"`, `"Hand"`, `"Waist"`. |
| spatialization | string | e.g. `"Global"`. |
| gain | number | Intensity multiplier. |

**Note:** `target` is passed through exactly as the configuration writes it. The
shipped data contains both a lowercase `"waist"` and the misspelling `"Wasit"`;
a caller mapping these to body regions should see what the data actually says
rather than a cleaned-up version of it.
