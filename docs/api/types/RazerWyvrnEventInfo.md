# RazerWyvrnEventInfo

One event declared by a WYVRN configuration, together with everything that
firing it would do.

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | The event name. Matched case-insensitively by [`setRazerWyvrnEvent`](../host/razer-wyvrn.md#setrazerwyvrneventrazerwyvrneventinfo-resultcallbacksuccess). |
| source | string | The configuration folder the event came from, which is normally the application that installed it. |
| kind | string | `exact` for a literal event name, `fallbackPattern` for a regular expression matched against names that no exact entry claims. |
| chroma | [`RazerWyvrnChromaComponent`](RazerWyvrnChromaComponent.md)[] | Lighting the event triggers. May be empty. |
| haptics | [`RazerWyvrnHapticComponent`](RazerWyvrnHapticComponent.md)[] | Haptics the event triggers. May be empty. |
| fallback | [`RazerWyvrnEventInfo`](RazerWyvrnEventInfo.md) | *Input only.* When passed to `setRazerWyvrnEvent`, the event to try if this one is not declared on the machine. Nests to arbitrary depth. Never present in results. |

**Note:** `fallbackPattern` entries are supported but unexercised — no
configuration observed in the wild declares any.

`chroma` and `haptics` are omitted when `getAllRazerWyvrnEvents` is called with
`components: false`.
