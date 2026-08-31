# Recording Outputs

`window.host`

## `getAllRecordingOutputs(ResultCallback<{id: OutputInfo}>)`

Available since API version 6.0

Get all recording outputs.

## `addRecordingOutput(OutputInfo, ResultCallback<success>)`

Available since API version 6.0

Add a recording output.

## `removeRecordingOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 6.0

Remove recording outputs by their IDs.

**Active recording outputs cannot be removed: they have to be disabled first.**

## `enableRecordingOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 6.0

Enable recording outputs by their IDs.

## `disableRecordingOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 6.0

Disable recording outputs by their IDs.

## `triggerRecordingOutputSplitById(string<id>, ResultCallback<success>)`

Available since API version 6.0

Split recording output file at the next keyframe.
