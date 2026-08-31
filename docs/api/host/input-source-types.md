# Input source types

`window.host`

## `getAvailableInputSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`

Available since API version 1.2

Get a list of available input source types.

An input source type may provide video and/or audio input.

Examples of input source types include the Browser Source, Video Capture Devices, and the Game Capture Source.

## `getAvailableVideoInputSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`

Available since API version 6.0

Get a list of available video input source types.

An input source type may provide **video** input.

## `getAvailableAudioInputSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`

Available since API version 6.0

Get a list of available audio input source types.

An input source type may provide **audio** input.

## `getInputSourceProperties(InputSourceTypeInfo, ResultCallback<InputSourceTypeInfo | null>)`

**Available since API version 6.0**

Get **input** source type’s properties.

This works by creating a source of *InputSourceTypeInfo.class* with *InputSourceTypeInfo.settings* applied to it, and returning an *InputSourceTypeInfo* with *InputSourceTypeInfo.properties* field adjusted to the source settings.
