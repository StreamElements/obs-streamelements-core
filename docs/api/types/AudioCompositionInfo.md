# AudioCompositionInfo

**API 6.0+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Audio composition ID. Must be unique. |
| name | string | Audio composition name |
| streamingAudioEncoder | ObsEncoderInfo | **Used on creation**. *ObsEncoderInfo* structure describing an audio encoder for streaming. |
| recordingAudioEncoder | ObsEncoderInfo | **Optional**. **Used on creation**. *ObsEncoderInfo* structure describing an audio encoder for recording. |
| audioTracks | Array\<object> | **Read-only**. Audio tracks available for use by outputs. |
| audioTracks[].id | number | **Read-only**. Audio track index. This is the id to use when filling *audioTracks* array on an *OutputInfo*. |
| audioTracks[].name | string | **Read-only**. Audio track name. |
| isObsNativeComposition | boolean | **Read-only**. Specifies whether this composition is the OBS native audio composition, or a custom audio composition. |
| canRemove | boolean | **Read-only**. Specifies whether this composition can be removed. OBS native audio composition, and composition which is currently in use (referenced by either an output or a view) cannot be removed. |
