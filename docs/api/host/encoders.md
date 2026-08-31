# Encoders

`window.host`

## `getAvailableEncoders(ResultCallback<EncoderInfo[]>)`

Get a list of available encoders.

**Data structures:** [`EncoderInfo`](../types/EncoderInfo.md)

## `getAvailableVideoEncoders(ResultCallback<EncoderInfo[]>)`

Get a list of available video encoders.

**Data structures:** [`EncoderInfo`](../types/EncoderInfo.md)

## `getAvailableAudioEncoders(ResultCallback<EncoderInfo[]>)`

Get a list of available audio encoders.

**Data structures:** [`EncoderInfo`](../types/EncoderInfo.md)

## `getEncoderProperties(EncoderInfo, ResultCallback<ObsEncoderInfo | null>)`

Get the properties and default settings of an encoder, which can be used for building a UI and also setting encoders up when creating Audio and Video Compositions.

Available since API version 6.0

**Data structures:** [`EncoderInfo`](../types/EncoderInfo.md), [`ObsEncoderInfo`](../types/ObsEncoderInfo.md)
