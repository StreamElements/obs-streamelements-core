# ObsEncoderInfo

**API 6.0+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| class | string | OBS internal encoder class |
| type | string | **Read-only**. Encoder type. Either “audio” or “video”.<br>**API 6.3+** |
| name | string | Encoder name |
| label | string | **Read-only**. Encoder display name for the UI |
| codec | string | **Read-only**. Encoder’s CODEC |
| width | number | **Read-only**. Output width for video encoders. |
| height | number | **Read-only**. Output height for video encoders. |
| settings | object[] | Key-value pairs of encoder settings. Each encoder class has their own settings to work with. |
| properties | ObsPropertyInfo[] | **Read-only**. Array of *ObsPropertyInfo* structures suitable for building a UI for encoder’s settings. |
| defaultSettings | object[] | **Read-only.** Key-value pairs of encoder settings default values. Each encoder class has their own settings to work with. |
| isDeprecated | boolean | **Read-only**. True if the encoder is marked as deprecated.<br>**API 6.2+** |
| audioMix | number | **Optional**. **Read-only** (for the moment). Indicates the audio mix (track) index (0-5) of the audio source.<br>Applies only to **audio** encoders.<br>**Default**: 0<br>**API 6.4+** |
