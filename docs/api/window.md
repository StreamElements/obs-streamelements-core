# `window`

## Events

### `hostReady`

Fires the moment window.host API setup completes

### `hostContainerVisibilityChanged`

Available since API version 1.12

Fires when *window.host.hostContainerHidden* value changes.

### `hostUIThemeChanged`

Available since API version 1.2

Fires right after host UI theme changes.

### `hostBandwidthTestStarted`

Fired when bandwidth test is started.

### `hostBandwidthTestCompleted`

Fired when bandwidth test has completed.

### `hostBandwidthTestProgress`

Fired when bandwidth test has progress.

### `hostStreamingStartRequested`

Fired when streaming start is requested

### `hostStreamingStarting`

Fired when streaming is starting.

### `hostStreamingStarted`

Fired when streaming has started.

### `hostStreamingStopping`

Fired when streaming is stopping.

### `hostStreamingStopped`

Fired when streaming has stopped.

### `hostRecordingStarting`

Fired when recording is starting.

### `hostRecordingStarted`

Fired when recording has started.

### `hostRecordingStopping`

Fired when recording is stopping.

### `hostRecordingStopped`

Fired when recording has stopped.

### `hostActiveSceneChanging`

Fired when current scene has started changing. This is fired at the beginning of a transition.

Available since API version 6.1

### `hostActiveSceneChanged`

Fired when current scene has been changed. This is fired at the end of a transition.

### `hostSceneListChanged`

Fired when scenes are added/removed

Available since API version 1.21

### `hostActiveSceneItemListChanged`

Fired when scene items are added / removed / hidden / shown / selected / locked / deselected / unlocked / reordered / transformed

Available since API version 1.21

### `hostSceneItemListChanged`

Same as *hostActiveSceneItemListChanged* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemAdded`

Fired when scene items are added.

*Event.detail* contains a *SceneItemInfo* object describing the scene item which was added to the active scene.

Available since API version 1.23

### `hostSceneItemAdded`

Same as *hostActiveSceneItemAdded* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemRemoved`

Fired when scene items are removed.

Available since API version 1.23

### `hostSceneItemRemoved`

Same as *hostActiveSceneItemRemoved* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemTransformed`

Fired when scene items are transformed.

*Event.detail* contains a *SceneItemInfo* object describing the scene item which was transformed.

Available since API version 1.23

### `hostSceneItemTransformed`

Same as *hostActiveSceneItemTransformed* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemsOrderChanged`

Fired when scene items order is changed.

Available since API version 1.23

### `hostSceneItemOrderChanged`

Same as *hostActiveSceneItemOrderChanged* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemSelected`

Fired when active scene item is selected.

Available since API version 1.23

**Note**: more than one scene item can be selected.

### `hostSceneItemSelected`

Same as *hostActiveSceneItemSelected* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemUnselected`

Fired when active scene item is unselected.

Available since API version 1.23

**Note**: more than one scene item can be selected.

### `hostSceneItemUnselected`

Same as *hostActiveSceneItemUnselected* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemSettingsChanged`

Fired when scene items underlying source settings are changed.

*Event.detail* contains a *SceneItemInfo* object describing the scene item which was changed.

Available since API version 1.23

**Note:** available only for the Browser Source

### `hostSceneItemSettingsChanged`

Same as *hostActiveSceneItemSettingsChanged* but fires for any scene.

Available since API version 1.25

**Note:** available only for the Browser Source

### `hostActiveSceneItemPropertiesChanged`

Fired when scene items underlying source properties  are changed.

*Event.detail* contains a *SceneItemInfo* object describing the scene item which was changed.

Available since API version 1.23

### `hostSceneItemPropertiesChanged`

Same as *hostActiveSceneItemPropertiesChanged* but fires for any scene.

Available since API version 1.25

### `hostActiveSceneItemRenamed`

Fired when scene items underlying source is renamed.

*Event.detail* contains a *SceneItemInfo* object describing the scene item which was changed.

Available since API version 1.23

### `hostSceneItemRenamed`

Same as *hostActiveSceneItemRenamed* but fires for any scene.

Available since API version 1.25

### `hostVideoPreviewMouseDoubleClicked`

Fired when anything on the OBS video preview pane is double-clicked with the mouse.

Available since API version 1.23

### `hostCurrentSceneItemsListMouseDoubleClicked`

Fired when anything on the OBS Sources list pane is double-clicked with the mouse.

Available since API version 1.23

### `hostBeforeFocusChange`

Fired when focus is about to change anywhere in the OBS application main window.

Available since API version 1.23

**Note:** can be used to detect end of transformation of scene items by the user on the preview pane and force an update NOW.

### `hostExit`

Fired on exit.

### `hostHotkeyPressed`

Available since API version 1.3

Fires when hotkey combination has been pressed.

Example:

```js
window.addEventListener('hostHotkeyPressed', function(e) {
    // e.detail contains data passed in eventDetail parameter to
    // window.host.addHotkeyBinding()
    console.log('*** event: hostHotkeyPressed', e.detail);
}, false, true);
```

### `hostHotkeyReleased`

Available since API version 1.3

Fires when hotkey combination has been released.

Example:

```js
window.addEventListener('hostHotkeyReleased', function(e) {
    // e.detail contains data passed in eventDetail parameter to
    // window.host.addHotkeyBinding()
    console.log('*** event: hostHotkeyReleased', e.detail);
}, false, true);
```

### `hostContainerKeyCombinationPressed`

Available since API version 1.3

Fires when hotkey combination has been pressed.

Example:

```js
window.addEventListener(' hostContainerKeyCombinationPressed', function(e) {
    // e.detail contains data described by KeyCombinationInfo structure.
    //
    console.log('*** event: hostContainerKeyCombinationPressed', e.detail);
}, false, true);
```

### `hostContainerKeyCombinationReleased`

Available since API version 1.3

Fires when hotkey combination has been released.

Example:

```js
window.addEventListener(' hostContainerKeyCombinationReleased', function(e) {
    // e.detail contains data described by KeyCombinationInfo structure.
    //
    console.log('*** event: hostContainerKeyCombinationReleased', e.detail);
}, false, true);
```

### `hostHotkeyBindingsChanged`

Available since API version 1.4

Fires when hotkey bindings have been changed via an API call or the OBS UI.

### `hostMessageReceived`

Available since API version 1.8

Fires when a message is received from this or another browser or an external controller.

Messages are dispatched using the broadcastMessage(message\<any>, ResultCallback\<success>) Message bus API call **or by an external controller**.

*event.detail* is a MessageBusMessageInfo data structure.

Example:

```js
window.addEventListener('hostMessageReceived', function(e) {
    var message = e.detail.message;
    var messageSource = e.detail.source;
    var messageSourceAddress = e.detail.sourceAddress;
    var messageScope = e.detail.scope;

    log('*** event: hostMessageReceived: scope = "' + messageScope + '", source = "' + messageSource + '", sourceAddress = "' + messageSourceAddress + '", message = ' + JSON.stringify(message));
});
```

**Note: this event is also available in the Browser Source.**

#### Commands from external controllers

External commands are constructed as follows (content of *event.detail.message* object):

```js
{
    "version": 1,
    "source": {
        "class": "controller",
        "element": {
            "class": "button",
            "function": "alerts_volume_set",
            "id": "specific_element_instance_id"
        }
    },
    "target": {
        "scope": "broadcast"
    },
    "payload": {
        "class": "command",
        "command": {
            "id": "volume_set",
            "data": {
                "value": 0
            }
        }
    }
}
```

Important fields for handling the command:

| **Field** | **Description** | **Comments** |
| --- | --- | --- |
| version | Protocol version | Use for backward compatibility with older versions of the protocol |
| source.class | Event source class | ‘controller’ for external controllers. |
| source.element.class | Type of the controlling element | ‘button’ for buttons |
| source.element.function | The generic function the controlling element fulfills | ‘alerts_mute_state’ might be a good candidate for buttons controlling alerts mute state (both mute and unmute) |
| source.element.id | Unique controlling element ID |  |
| target.scope | The scope of the message targets | ‘broadcast’ = all targets |
| payload.class | Type of the payload | ‘command’ = command message |
| payload.command.name | Command name (ID) | This is the actual command to be processed |
| payload.command.data | Command arguments | Command-specific arguments |

Well-known commands:

| Command ID | Description | Command data field | Command data field value description |
| --- | --- | --- | --- |
| alerts-skip | Skip alert |  |  |
| alerts-mute-on | Mute alerts |  |  |
| alerts-mute-off | Unmute alerts |  |  |
| alerts-pause-on | Pause alerts |  |  |
| alerts-pause-off | Unpause alerts |  |  |
| jar-empty | Empty jar |  |  |
| media-request-video-on | Show media player |  |  |
| media-request-video-off | Hide media player |  |  |
| media-request-off | Pause media player |  |  |
| media-request-on | Unpause media player |  |  |
| overlays-reload | Reload overlays |  |  |
| kappagen | Kappagen | value | Optionally specifies the emotesplosion size. If not specified – default is used. |

### `hostEventReceived`

Available since API version 1.9

Fires when an event is received from this or another browser.

Messages are dispatched using the broadcastEvent(EventInfo, ResultCallback\<success>) Message bus API call.

*event.detail* is a MessageBusMessageInfo data structure.

*event.detail.message* is an EventInfo data structure.

Example:

```js
window.addEventListener('hostEventReceived', function(e) {
    var event = e.detail.message;
    var messageSource = e.detail.source;
    var messageSourceAddress = e.detail.sourceAddress;
    var messageScope = e.detail.scope;

    log('*** event: hostMessageReceived: scope = "' + messageScope + '", source = "' + messageSource + '", sourceAddress = "' + messageSourceAddress + '", event = ' + JSON.stringify(event));
});
```

**Note: this event is not available in the Browser Source.**

### `hostStateReset`

Available since API version 1.15

Fires just before the user signs out via the UI (menu item).

Allows small delay (100ms) between the request and destruction of cookies & widgets.

It is **not recommended** to rely on this delay since it might be not enough on systems with high load.

### `hostUserInterfaceStateChanged`

Available since API version 1.20

Fires right after host user interface state changes

### `hostSceneCollectionChanged`

Available since API version 1.32

Fires right after scene collection change

### `hostSceneCollectionListChanged`

Available since API version 1.32

Fires right after scene collection list change

### `hostProfileChanged`

Available since API version 1.32

Fires right after profile change

### `hostProfileListChanged`

Available since API version 1.32

Fires right after profile list change

### `hostNativeManageBroadcastButtonVisible`

Fired when “manage broadcast” button becomes visible. This indicates an active integration with YouTube which presents its own “manage broadcast” dialog.

Available since API version 2.3

### `hostNativeManageBroadcastButtonHidden`

Fired when “manage broadcast” button becomes hidden. This indicates that there is no active integration with YouTube which presents its own “manage broadcast” dialog.

Available since API version 2.3

### `hostStreamingOutputListChanged`

Any change to streaming outputs list except stats.

Available since API version 5.0

### `hostStreamingOutputStarted`

Fired when the output starts

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputError`

Fired when the output start errors

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputStopped`

Fired when the output stops (also after \`hostStreamingOutputError\`)

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputPaused`

Fired when the output is paused

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputUnpaused`

Fired when the output is unpaused

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputStarting`

Fired when the output is starting

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputStopping`

Fired when the output is stopping

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputActivated`

Fired when the output was activated (begins capturing data)

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputDeactivated`

Fired when the output was deactivated (stops capturing data)

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputReconnecting`

Fired when the output is reconnecting

Payload: { outputId }

Available since API version 5.0

### `hostStreamingOutputReconnected`

Fired when the output has successfully reconnected

Payload: { outputId }

Available since API version 5.0

### `hostRecordingOutputListChanged`

Any change to recording outputs list except stats.

Available since API version 6.0

### `hostRecordingOutputStarted`

Fired when the output starts

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputError`

Fired when the output start errors

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputStopped`

Fired when the output stops (also after \`hostStreamingOutputError\`)

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputPaused`

Fired when the output is paused

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputUnpaused`

Fired when the output is unpaused

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputStarting`

Fired when the output is starting

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputStopping`

Fired when the output is stopping

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputActivated`

Fired when the output was activated (begins capturing data)

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputDeactivated`

Fired when the output was deactivated (stops capturing data)

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputReconnecting`

Fired when the output is reconnecting

Payload: { outputId }

Available since API version 6.0

### `hostRecordingOutputReconnected`

Fired when the output has successfully reconnected

Payload: { outputId }

Available since API version 6.0

### `hostReplayBufferOutputListChanged`

Any change to ReplayBuffer outputs list except stats.

Available since API version 6.1

### `hostReplayBufferOutputStarted`

Fired when the output starts

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputError`

Fired when the output start errors

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputStopped`

Fired when the output stops (also after \`hostStreamingOutputError\`)

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputPaused`

Fired when the output is paused

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputUnpaused`

Fired when the output is unpaused

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputStarting`

Fired when the output is starting

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputStopping`

Fired when the output is stopping

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputActivated`

Fired when the output was activated (begins capturing data)

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputDeactivated`

Fired when the output was deactivated (stops capturing data)

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputReconnecting`

Fired when the output is reconnecting

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputReconnected`

Fired when the output has successfully reconnected

Payload: { outputId }

Available since API version 6.1

### `hostReplayBufferOutputSavedToLocalFile`

Fired when the output has successfully saved its buffer to a local file.

This event may be used to set up a Media Source referencing the saved buffer, to create Instant Replays.

Payload: { outputId, fliePath }

Available since API version 6.1

### `hostSceneRenamed`

Fired when a scene is renamed

Payload: SceneInfo

Available since API version 6.0

### `hostActiveSceneRenamed`

Fired when the current active scene is renamed

Payload: SceneInfo

Available since API version 6.0

### `hostSceneRemoved`

Fired when a scene is removed

Payload: SceneInfo

Available since API version 6.0

### `hostActiveSceneRemoved`

Fired when the current active scene is removed

Payload: SceneInfo

Available since API version 6.0

hostVideoCompositionListChanged

Fired when the list of video compositions changes (video compositions are added or removed)

Available since API version 6.0

hostVideoCompositionChanged

Fired when video composition properties change

Payload: VideoCompositionInfo

Available since API version 6.0

### `hostAudioCompositionListChanged`

Fired when audio composition properties change

Payload: AudioCompositionInfo

hostAudioCompositionChanged

Fired when audio composition properties change

**Payload**: AudioCompositionInfo

Available since API version 6.0

### `hostVideoCompositionBeforeScenesReset`

Fired before all scenes on a video composition are being reset. This happens mostly when the user changes a scene collection, or, upon video composition destruction.

If the front-end is hooking scene/scene items/source change events to save state, it should suspend saving when receiving this event, since changes are being forced upon the video composition. It should save the current state instead to be restored when *hostVideoCompositionAfterScenesReset* fires.

**Payload**: { videoCompositionId: string }

Available since API version 6.0

### `hostVideoCompositionAfterScenesReset`

Fired after all scenes on a video composition have been reset. This happens mostly when the user changes a scene collection, or, upon video composition destruction.

See *hostVideoCompositionBeforeScenesReset* for more information about handling this event.

Available since API version 6.0

**Payload**: { videoCompositionId: string }

### `hostActiveTransitionChanged`

Fired when a video composition’s transition changes, including changes to settings and properties.

Available since API version 6.5

**Payload**: { videoCompositionId: string }

### `hostSharedVideoCompositionListChanged`

Fired when a Shared Video Compositions list changes for any reason.

Available since API version 6.6

**Payload**: null

### `⚠️ hostRazerWyvrnStatusChanged`

> **Not available in this release** — see [Razer WYVRN](host/razer-wyvrn.md).

Fired when the [Razer WYVRN](host/razer-wyvrn.md) integration changes state — becoming ready, failing to initialize, or shutting down.

Available since API version 6.8

**Payload**: [`RazerWyvrnStatus`](types/RazerWyvrnStatus.md)

The payload is the same object `getHostCapabilities` returns as its
`razerWyvrn` member, so this can be treated as a push of a value that would
otherwise have to be polled.

Initialization takes about 3.4 seconds and runs in the background, so a page
that loads later misses this event entirely. Call
[`getHostCapabilities`](host/host-information.md#gethostcapabilitiesresultcallbackhostcapabilities)
first and treat it as the source of truth; subscribing alone is not enough.

It fires on failure as well as success — on the overwhelmingly common machine
with no Razer software installed it arrives carrying `dllNotFound` rather than
never arriving.

It fires only on a *change*, so a build where the integration is compiled out
(`notCompiledIn`) or the platform does not support it (`notSupportedOnPlatform`)
never transitions and never fires. That is the other reason to call
`getHostCapabilities` first rather than waiting on this event.

## Methods

### `window.open()`

Standard window.open() and \<a href=”…” target=”_blank”>\</a> will open a modeless CEF browser window in OBS context. This is provided for compatibility with existing web standards.

Intended use: opening YouTube dashboard, opening StreamElements overlay editor without requiring a second log-on.

For more flexible uses, such as allowing the new window to dock to OBS, modal dialogs, and injecting JavaScript code into the 3rd party URL context, see addDockingWidget(), showModalDialog() and openPopupWindow() methods below.

For opening the platform default browser, see openExternalBrowser() method below.

For opening modal dialogs, see showModalDialog() method below.
