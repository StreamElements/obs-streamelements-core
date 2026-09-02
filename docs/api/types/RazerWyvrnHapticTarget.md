# RazerWyvrnHapticTarget

One body target of a
[`RazerWyvrnHapticComponent`](RazerWyvrnHapticComponent.md).

**Available since API version 6.8**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| target | string | Body region: `hand`, `head`, `chest`, `waist`, `leg`, `all`, `down`, `top`. |
| spatialization | string | `global`, `left` or `right`. |
| gain | number | Intensity multiplier. |

**Note:** values are normalised to camelCase from the vendor's own
capitalisation — see [Enum
values](../host/razer-wyvrn.md#enum-values). That folds the `Waist`/`waist`
split in the source data onto one value, but does not repair the `Wasit` typo
that also occurs, which arrives as `wasit`. A caller mapping targets to body
regions should expect that.
