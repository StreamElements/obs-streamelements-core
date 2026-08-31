# Audio Compositions

`window.host`

## `getAllAudioCompositions(ResultCallback< id: AudioCompositionInfo }>)`

Get all currently existing audio compositions. The result of this API call is a map of audio composition ID to *AudioCompositionInfo* structure.

**Available since API version 6.0**

**Data structures:** [`AudioCompositionInfo`](../types/AudioCompositionInfo.md)

## `removeAudioCompositionsByIds(Array<audioCompositionId>, ResultCallback<success>)`

Remove audio compositions by their IDs. If an ID does not exist, it is ignored. If at least one audio composition specified by an ID cannot be removed (*canRemove = false*), the whole call will fail.

**Available since API version 6.0**

## `addAudioComposition(AudioCompositionInfo, ResultCallback< AudioCompositionInfo | null>)`

Add an audio composition. Returns the full composition info upon success, or *null* on failure. One of the reasons for failure may be a duplicate audio composition ID.

**Available since API version 6.0**

**Data structures:** [`AudioCompositionInfo`](../types/AudioCompositionInfo.md)

## `setAudioCompositionProperties(AudioCompositionInfo, ResultCallback< AudioCompositionInfo | null>)`

Modifies audio composition properties. Returns the full composition info upon success, or *null* on failure.

Currently only updating “name” is supported.

**Available since API version 6.0**

**Data structures:** [`AudioCompositionInfo`](../types/AudioCompositionInfo.md)
