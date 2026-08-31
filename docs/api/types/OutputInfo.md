# OutputInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Output unique ID.<br>Two outputs with the same ID cannot be added. |
| name | string | Output name |
| videoCompositionId | string | **Optional**. Source video composition ID.<br>When creating an output, this field is optional and defaults to OBS native composition. |
| audioCompositionId | string | **Optional**. Source audio composition ID.<br>When creating an output, this field is optional and defaults to OBS native composition. |
| audioTracks | Array\<number> | **Optional**. Array of audio tracks indexes to use in the output.<br>For outputs which support multiple audio tracks (like RTMP), multiple tracks may be specified. Corresponding audio streams will be added to the output.<br>Default: **[0]** |
| videoEncoders | Array\<number \| ObsEncoderInfo> | **Optional**. Array of video encoders to reference. The numbers represent indexes into *streamingVideoEncoders* or *recordingVideoEncoders* (depending on the type of output) available on a *VideoCompositionInfo* of a specific *videoCompositionId*.<br>For outputs which support multiple encoders (like RTMP), multiple encoders may be specified. Corresponding video streams will be added to the output.<br>Default: **[0]**<br>**API 6.2+**<br>**Since API 6.4+** specifying *ObsEncoderInfo* instead of encoder indexes is also supported. When an *ObsEncoderInfo* is specified, the output will create its own instance of a video encoder instead of the encoders provided by the *VideoComposition*, thus will **override** any encoding settings set on the *VideoComposition*.<br>Indexes and *ObsEncoderInfo* records may be used together to create a mix of encoders defined on the *VideoComposition* and additional encoders endemic to the specific *Output*. |
| type | string | **Read-only**. “streaming”, “recording” or “replayBuffer” |
| streamingSettings | StreamingSettings | Output streaming settings. Exists and required when *type* = “streaming”. |
| recordingSettings | RecordingSettings | **Read-only**. Output recording settings. Exists when *type* = “recording”. |
| auxiliaryData | object | **Optional**. Any user-defined auxiliary data. |
| isEnabled | boolean | Specifies whether the output is enabled. Enabled outputs start streaming when OBS main output is streaming |
| isActive | boolean | **Read-only**. Specifies whether the output is currently active. |
| canDisable | boolean | **Read-only**. Specifies whether the output can be disabled. |
| isObsNativeVideoComposition | boolean | **Read-only**. Specifies whether the video composition connected to the output is OBS native video composition. |
| isObsNativeAudioComposition | boolean | **Read-only**. Specifies whether the audio composition connected to the output is OBS native audio composition. |
| isObsNativeOutput | boolean | **Read-only**. Specifies whether the output is OBS native output. |
| canRemove | boolean | **Read-only**. Specifies whether the output can be removed. |
| error | object | **Read-only**. Only available after an fatal error. |
| error.message | string | **Read-only**. Last fatal error message. |
| stats | object | **Read-only**. Only available when output is active. |
| stats.canPause | boolean | **Read-only**. Specifies whether the output can be paused. |
| stats.isReconnecting | boolean | **Read-only**. Specifies whether the output is currently reconnecting. |
| stats.framesDropped | number | **Read-only**. Number of frames dropped. |
| stats.congestion | number | **Read-only**. Congestion. |
| stats.lastErrorMessage | string | **Read-only**. Last error message. |
| stats.totalBytes | number | **Read-only**. Total bytes sent. |
| stats.totalFrames | number | **Read-only**. Total frames sent. |
| stats.width | number | **Read-only**. Output width. |
| stats.height | number | **Read-only**. Output height. |
| stats.isPaused | bool | **Read-only**. Specifies whether the output is paused. |
