# RazerWyvrnChromaAsset

One `.chroma` file, for one device.

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| device | string | `"Keyboard"`, `"Keypad"`, `"Mouse"`, `"Mousepad"`, `"Headset"` or `"ChromaLink"`. |
| url | string | Session-signed local URL. Fetching it returns the file's exact bytes. |

**Note:** `device` names the *family*, not the geometry. Both the 132-LED and
the 192-LED keyboard grids appear as `"Keyboard"`, and only the file header
separates them — see [the `.chroma`
format](../host/razer-wyvrn.md#the-chroma-format).
