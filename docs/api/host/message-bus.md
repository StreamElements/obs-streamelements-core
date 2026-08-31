# Message bus

`window.host`

## `broadcastMessage(message<any>, ResultCallback<success>)`

Available since API version 1.8

Broadcast message to all (browser) listeners, except external controllers.

In browsers, the message will be received by the *hostMessageReceived* event handler.

**Note: this API call is also available in the Browser Source.**

## `broadcastEvent(EventInfo, ResultCallback<success>)`

Available since API version 1.9

Broadcast event to all listeners, **including external controllers**.

In browsers, the message will be received by the hostEventReceived event handler.

External controllers will receive the event according to the external controller communication protocol.

**Note: this API call is not available in the Browser Source.**

When you need to communicate with the Browser Source, use *broadcastMessage*.
