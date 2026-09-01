# RazerWyvrnChromaComponent

One lighting effect within a [`RazerWyvrnEventInfo`](RazerWyvrnEventInfo.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| effect | string | Effect name, as written in the configuration. |
| interrupt | bool | Whether this effect interrupts whatever is currently playing. |
| device | string | `"Keyboard"`, `"Keypad"`, `"Mouse"`, `"Mousepad"`, `"Headset"` or `"ChromaLink"`. Empty when the name carries no recognised device suffix. |
| url | string | Session-signed local URL for the `.chroma` file. Absent when the file is not present on disk. |

**Note:** a Chroma effect is already device-specific — a configuration lists one
component per device, and the effect name is the asset's base name
(`Interact_Keyboard` → `Interact_Keyboard.chroma`). `device` is read back from
that suffix, and a caller needs it to pick the grid dimensions, since those are
not stored in the file. See [the `.chroma`
format](../host/razer-wyvrn.md#the-chroma-format).

`device` names the *family*, not the geometry: both the 132-LED and the 192-LED
keyboard grids appear as `"Keyboard"`, and only the file header separates them.
