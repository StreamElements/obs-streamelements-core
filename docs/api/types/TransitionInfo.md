# TransitionInfo

**API 6.0+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| videoCompositionId | string | ID of the video composition which the transition belongs to. |
| class | string | Transition class ID. Used to identify a specific transition type. |
| properties | ObsPropertyInfo[] | **Read-only.** List of *ObsPropertyInfo* structures which can be used for building a UI to edit the transition class. |
| settings | object[] | Key-value pairs of transition settings. |
| defaultSettings | object[] | **Read-only**. Key-value pairs of transition settings’ default values. |
| label | string | **Read-only**. Display name of the transition type. |
| width | number | **Optional.** Integer number. Transition width. |
| height | number | **Optional.** Integer number. Transition height. |
| scaleType | string | **Optional.** Either “maxOnly”, “aspect”, “stretch” (or “unknown” when we’re unable to map OBS internal scale type code to a string). |
| alignment | string | **Optional**. “top”, “left”, “bottom”, “right”, “center” |
| durationMilliseconds | number | **Optional**. Integer number. Transition duration in milliseconds.<br>Default: 300ms. |
