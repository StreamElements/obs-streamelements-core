# InputSourceTypeInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Input type ID<br>**Breaking change**: **since API 6.0** this will reflect an existing source ID to be referenced when adding existing sources as scene items. |
| class | string | Input type class (former ID). Includes source type version for disambiguation. |
| unversionedClass | string | Unversioned input type class (may reference different versions of the source on different systems)<br>**API 6.0+** |
| isDeprecated | boolean | Specifies whether this is a deprecated source.<br>Usage of deprecated sources is discouraged, but is still available.<br>OBS displays deprecated sources in a special “Deprecated” submenu.<br>**API 6.0+** |
| name | string | Input type name for non-existing sources (those which have not been created yet, i.e. source types), or the actual source instance name (**since API 6.0**) |
| className | string | Input type name (**since API 6.0**) |
| hasVideo | boolean | If true: input type provides video signal |
| hasAudio | boolean | If true: input type provides audio signal |
| isVideoCaptureDevice | boolean | If true: input type is a video capture device |
| isGameCaptureDevice | boolean | If true: input type is a game capture device |
| isBrowserSource | boolean | If true: input type is a browser source |
| isSceneSource | boolean | **Read-only**. For source instances only.<br>If true: input source is a Scene<br>**API 6.0+** |
| isGroupSource | boolean | **Read-only**. For source instances only.<br>If true: input source is a Group<br>**API 6.0+** |
| isFilterSource | bool | **Read-only**. Indicates whether the source is a filter.<br>**API 6.0+** |
| isInputSource | bool | **Read-only**. Indicates whether the source is an input source.<br>**API 6.0+** |
| isTransitionSource | bool | **Read-only**. Indicates whether the source is a transition.<br>**API 6.0+** |
| isSceneSource | bool | **Read-only**. Indicates whether the source is a scene.<br>**API 6.0+** |
| settings | object | **Read-only**. For source instances only.<br>Source settings key-value pairs.<br>**API 6.0+** |
| defaultSettings | object | **Read-only**.<br>Source default settings key-value pairs.<br>**API 6.0+** |
| properties | ObsPropertyInfo[] | **Read-only**.<br>Source properties which can be used to construct a UI to edit the source properties.<br>**API 6.0+** |
| order | number | **Optional**.<br>**Only applies to Filters**.<br>Order of a Filter source in the list of filters of a parent source.<br>**API 6.0+** |
