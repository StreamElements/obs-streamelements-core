# Existing input sources

`window.host`

## `getAllExistingInputSources(ResultCallback<InputSourceTypeInfo[]>)`

**Available since API version 6.0**

Get a list of existing **audio or video** input sources, which can be later referenced by *addSceneItemObsNativeSource*.

.

The result of this API call may be used to reference specific existing source instances as scene items.

**Data structures:** [`InputSourceTypeInfo`](../types/InputSourceTypeInfo.md)

## `getAllExistingVideoInputSources(ResultCallback<InputSourceTypeInfo[]>)`

**Available since API version 6.0**

Get a list of existing **video** input sources, which can be later referenced by *addSceneItemObsNativeSource*.

.

The result of this API call may be used to reference specific existing source instances as scene items.

**Data structures:** [`InputSourceTypeInfo`](../types/InputSourceTypeInfo.md)

## `getAllExistingAudioInputSources(ResultCallback<InputSourceTypeInfo[]>)`

**Available since API version 6.0**

Get a list of existing **audio** input sources, which can be later referenced by *addSceneItemObsNativeSource*.

.

The result of this API call may be used to reference specific existing source instances to an audio mix.

**Data structures:** [`InputSourceTypeInfo`](../types/InputSourceTypeInfo.md)
