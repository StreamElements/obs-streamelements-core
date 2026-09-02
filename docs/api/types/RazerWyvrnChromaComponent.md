# RazerWyvrnChromaComponent

One lighting effect within a [`RazerWyvrnEventInfo`](RazerWyvrnEventInfo.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| effect | string | Effect name, as written in the configuration. |
| interrupt | bool | Whether this effect interrupts whatever is currently playing. |
| device | string | `keyboard`, `keyboardExtended`, `keypad`, `mouse`, `mousepad`, `headset` or `chromaLink`. Empty when the name carries no recognised device suffix. |
| url | string | Session-signed local URL for the `.chroma` file. Absent when the file is not present on disk. |

**Note:** a Chroma effect is already device-specific — a configuration lists one
component per device, and the effect name is the asset's base name
(`Interact_Keyboard` → `Interact_Keyboard.chroma`). `device` is read back from
that suffix and normalised to camelCase (see [Enum
values](../host/razer-wyvrn.md#enum-values)). A caller needs it to pick the grid
dimensions, since those are not stored in the file.

`keyboard` and `keyboardExtended` are **different grids** — 132 LEDs (6 × 22)
and 192 LEDs (8 × 24). Both are common; the file header is what distinguishes
them, so cross-check the size rather than trusting the suffix. See [the `.chroma`
format](../host/razer-wyvrn.md#the-chroma-format).
