# Streaming

`window.host`

## `getStreamingStatus(ResultCallback<StreamingStatusInfo>)`

Available since API version 1.17

Retrieve current streaming status, including whether streaming is currently active.

## `requestStreamingStart(ResultCallback<success>)`

Available since API version 1.18

Request OBS to start streaming.

In addition this will have the same effect as calling adviseStreamingStartUIRequestAccepted (ResultCallback\<success>)

## `requestStreamingStop(ResultCallback<success>)`

Available since API version 1.18

Request OBS to stop streaming.

## `setStreamingStartUIHandlerProperties (StreamingStartUIHandlerProperties, ResultCallback<success>)`

Available since API version 1.18

Changes how UI-triggered streaming start behaves.

This method mainly controls whether triggering streaming start actually starts streaming or sends the hostStreamingStartRequested event for one of the JavaScript pages to handle as it sees fit.

The JavaScript page may choose to present additional user interface, alter streaming destination, etc.

## `adviseStreamingStartUIRequestAccepted (ResultCallback<success>)`

Available since API version 1.18

Notifies the Start Streaming UI handler that Start Streaming request delivered by the hostStreamingStartRequested event was accepted and will be handled.

## `adviseStreamingStartUIRequestRejected (ResultCallback<success>)`

Available since API version 1.18

Notifies the Start Streaming UI handler that Start Streaming request delivered by the hostStreamingStartRequested event was rejected and will not be handled.

In case *autoStart* has been set to *false* by calling setStreamingStartUIHandlerProperties (StreamingStartUIHandlerProperties, ResultCallback\<success>), this effectively means that Start Streaming failed and will be appropriately reflected in the UI.
