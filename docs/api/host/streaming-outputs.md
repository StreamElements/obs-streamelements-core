# Streaming Outputs

`window.host`

## `getAllStreamingOutputs(ResultCallback<{id: OutputInfo}>)`

Available since API version 5.0

Get all streaming outputs.

## `addStreamingOutput(OutputInfo, ResultCallback<success>)`

Available since API version 5.0

Add a streaming output.

## `removeStreamingOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 5.0

Remove streaming outputs by their IDs.

**Active streaming outputs cannot be removed: they have to be disabled first.**

## `enableStreamingOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 5.0

Enable streaming outputs by their IDs.

## `disableStreamingOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 5.0

Disable streaming outputs by their IDs.
