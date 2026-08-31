# Replay Buffer Outputs

`window.host`

## `getAllReplayBufferOutputs(ResultCallback<{id: OutputInfo}>)`

Available since API version 6.1

Get all replay buffer outputs.

## `addReplayBufferOutput(OutputInfo, ResultCallback<success>)`

Available since API version 6.1

Add a replay buffer output.

Replay buffer outputs are set up similar to Recording outputs, and are using the same encoders as Recording outputs, with the only difference that *recordingSettings.settings.* *splitAtMaximumDurationSeconds* and *recordingSettings.settings.splitAtMaximumMegabytes* specify the maximum size of the cyclical replay buffer, and not the spit points of a recording made to local files.

To get the contents of the replay buffer, one should call *triggerReplayBufferOutputSaveById* and wait for *hostReplayBufferOutputSavedToLocalFile* event to be dispatched in case they are interested in the file path of the saved replay buffer contents.

## `removeReplayBufferOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 6.1

Remove replay buffer outputs by their IDs.

**Active replay buffer outputs cannot be removed: they have to be disabled first.**

## `enableReplayBufferOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 6.1

Enable replay buffer outputs by their IDs.

## `disableReplayBufferOutputsByIds(Array<id>, ResultCallback<success>)`

Available since API version 6.1

Disable replay buffer outputs by their IDs.

## `triggerReplayBufferOutputSaveById(string<id>, ResultCallback<success>)`

Available since API version 6.1

Save replay buffer to local file and trigger *hostReplayBufferOutputSavedToLocalFile* event on completion.

This construct can be used to later set up a Media Source with *filePath* reported as part of the *hostReplayBufferOutputSavedToLocalFile* event payload to create Instant Replays.
