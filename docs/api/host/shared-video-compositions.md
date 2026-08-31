# Shared Video Compositions

`window.host`

Shared video compositions are published to OBS frontend and can be used as Secondary Canvas in enhanced streaming scenarios.

Video Compositions can be connected and disconnected from/to Shared Video Compositions: in this sense SVCs are very similar to Outputs.

Shared Video Compositions are persistent: their persistence is managed by OBS frontend on Scene Collection level.

If streaming / recording is active, changing/removing Shared Video Compositions will not be allowed. This is to prevent possible issues when SVC frame size will change while an encoder is consuming frames from the SVC.

## `addSharedVideoComposition(SharedVideoCompositionInfo, ResultCallback<SharedVideoCompositionInfo | null>)`

Create a new Shared Video Composition.

Shared Video Composition name is assigned automatically to reflect it’s status: either unassigned, or assigned to a Video Composition, in which case it will reflect the assigned Video Composition name.

Available since API version 6.6

## `setSharedVideoCompositionProperties(SharedVideoCompositionInfo, ResultCallback<SharedVideoCompositionInfo | null>)`

Set Shared Video Composition properties. At the time of this writing, only setting “name” is supported.

This is the only way to change the automatically assigned Shared Video Composition name, and should not really be used by anyone, unless you have a good reason to do so (perhaps to add canvas metadata to the name).

SharedVideoComposition.id field is always required.

Available since API version 6.6

## `removeSharedVideoCompositionsByIds(Array<string>, ResultCallback<success>)`

Remove existing Shared Video Compositions by their *sharedVideoCompositionIds*.

Available since API version 6.6

## `getAllSharedVideoCompositions(ResultCallback<{ id: SharedVideoCompositionInfo }>)`

Retrieve all existing Shared Video Compositions as an object, where it’s key is *sharedVideoCompositionId* and value is a *SharedVideoCompositionInfo* corresponding to each SVC.

Available since API version 6.6

## `connectVideoCompositionToSharedVideoComposition({ sharedVideoCompositionId: string, videoCompositionId: string }, ResultCallback<SharedVideoCompositionInfo>)`

Connect an existing Video Composition to an existing Shared Video Composition.

This will adjust the SVC frame size to match the source VC frame size.

One VC can be shared by multiple SVCs. In this sense, an SVC works very much like an Output.

Calling this API while OBS video output is active (streaming, recording) is not allowed.

Available since API version 6.6

## `disconnectVideoCompositionsFromSharedVideoCompositionsByIds(Array<string>, ResultCallback<success>)`

Disconnect Shared Video Compositions from Video Compositions by *sharedVideoCompositionIds*.

Calling this API while OBS video output is active (streaming, recording) is not allowed.

Available since API version 6.6
