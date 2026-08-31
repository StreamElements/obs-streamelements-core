# EncodingSettings

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| videoEncoderId | string | Video encoder Id |
| videoBitsPerSecond | number | Video bitrate |
| videoFrameWidth | number | Video frame width |
| videoFrameHeight | number | Video frame height |
| videoFramesPerSecond | number | Video fps |
| videoKeyframeIntervalSeconds | number | Video keyframe (I-frame) interval, this is also called GOP |
| audioEncoderId | string | Audio encoder Id |
| audioBitsPerSecond | number | Audio bitrate |
| audioSamplesPerSecond | number | Audio sample rate |
| audioChannels | number | Audio channels |

**Note**: audio encoder is assumed to be AAC and is omitted from the current version of the API
