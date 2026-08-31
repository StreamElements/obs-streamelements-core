# Audio & Video Composition Encoders

`window.host`

## `getAllAvailableVideoEncoderClasses(ResultCallback<ObsEncoderInfo[]>)`

Get list of all available video encoder classes, along with their properties suitable for building a UI.

**Available since API version 6.0**

**Data structures:** [`ObsEncoderInfo`](../types/ObsEncoderInfo.md)

## `getAllAvailableAudioEncoderClasses(ResultCallback<ObsEncoderInfo[]>)`

Get list of all available audio encoder classes, along with their properties suitable for building a UI.

**Available since API version 6.0**

**Data structures:** [`ObsEncoderInfo`](../types/ObsEncoderInfo.md)

## `getAvailableVideoEncoderClassProperties(ObsEncoderInfo, ResultCallback<ObsEncoderInfo>)`

Takes an *ObsEncoderInfo* description of a ***video*** encoder, with *settings* set, and returns an *ObsEncoderInfo* for the same encoder, with *properties* set according to input defined by *settings* property.

This can be used to construct dynamic encoder property sheets.

**Available since API version 6.3**

**Data structures:** [`ObsEncoderInfo`](../types/ObsEncoderInfo.md)

## `getAvailableAudioEncoderClassProperties(ObsEncoderInfo, ResultCallback<ObsEncoderInfo>)`

Takes an *ObsEncoderInfo* description of an ***audio*** encoder, with *settings* set, and returns an *ObsEncoderInfo* for the same encoder, with *properties* set according to input defined by *settings* property.

This can be used to construct dynamic encoder property sheets.

**Available since API version 6.3**

**Data structures:** [`ObsEncoderInfo`](../types/ObsEncoderInfo.md)
