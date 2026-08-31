# StreamingSettings

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| type | string | Streaming service type (usually “rtmp_custom” or similar), only available when retrieving existing streaming settings, otherwise always assumed to be “rtmp_custom”<br>**API version 5.0+** |
| serverUrl | string | RTMP server URL |
| streamKey | string | RTMP stream key |
| useAuth | bool | Enable authentication? |
| authUsername | string | Valid if useAuth == true |
| authPassword | string | Valid if useAuth == true |
