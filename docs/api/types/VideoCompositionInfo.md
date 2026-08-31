# VideoCompositionInfo

**API 6.0+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Video composition ID. Must be unique. |
| name | string | Video composition name |
| view | string | **Optional**. Either “custom” or “obs”. If “obs” is specified, then the video composition references the OBS native video view. Any other value indicates that a new, custom video composition will be created.<br>**Default**: “custom”<br>**API 6.2+** |
| videoFrame.width | number | Output video frame width |
| videoFrame.height | number | Output video frame height |
| streamingVideoEncoders | ObsEncoderInfo[] | Array of *ObsEncoderInfo* structures.<br>As of **API 6.2** may contain multiple (at least one) items. |
| recordingVideoEncoders | ObsEncoderInfo[] | **Optional**. Array of *ObsEncoderInfo* structures.<br>As of **API 6.2** may contain multiple (at least one) items.<br>If not specified, *streamingVideoEncoders* will be used for recording.<br>Recording video encoders are also used by replay buffer outputs. |
| isObsNativeComposition | boolean | **Read-only**. Specifies whether this composition is the OBS native video composition, or a custom video composition. |
| canRemove | boolean | **Read-only**. Specifies whether this composition can be removed. OBS native video composition, and composition which is currently in use (referenced by either an output or a view) cannot be removed. |
