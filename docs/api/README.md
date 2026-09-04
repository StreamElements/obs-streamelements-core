# JavaScript OBS API

**API version 6.6** — the host API SE.Live exposes to page JavaScript
running inside OBS.

Converted from `JavaScript OBS API Version 6.6.docx` — last recorded
API revision 2025-10-07, file last saved 2025-10-09, Word revision 117 of a
document started 2018-05-22. That document is no longer the source of truth;
this tree is.

## Start here

| | |
| --- | --- |
| [Abstract](abstract.md) | what this API is |
| [Status of this Document](status.md) | how stable the spec is |
| [Conventions](conventions.md) | naming, callbacks, error handling |
| [General guidelines](general-guidelines.md) | how to use the API well |
| [Versioning](versioning.md) | how API versions are numbered |
| [Web Authentication](web-authentication.md) | authenticating a page |
| [Goals and Considerations](goals-and-considerations.md) | design rationale |
| [Revision History](revision-history.md) | every change, 1.0 → 6.6 |

## Reference

- [`window`](window.md) — 104 events and methods on the page's own `window`
- [`window.host`](host/README.md) — 217 calls across 58 groups
- [Data structures](types/README.md) — 70 object types

### `window.host` by group

- [Properties](host/properties.md) — 4 calls
- [Startup flags](host/startup-flags.md) — 2 calls
- [Cookies](host/cookies.md) — 1 call
- [On-boarding](host/on-boarding.md) — 2 calls
- [Logged-in notification](host/logged-in-notification.md) — 2 calls
- [External browser](host/external-browser.md) — 1 call
- [Popup window](host/popup-window.md) — 3 calls
- [Modal dialog](host/modal-dialog.md) — 2 calls
- [Non-Modal dialog](host/non-modal-dialog.md) — 6 calls
- [Generic dialog API](host/generic-dialog-api.md) — 1 call
- [Container information](host/container-information.md) — 1 call
- [Central widget](host/central-widget.md) — 2 calls
- [Docking widgets](host/docking-widgets.md) — 13 calls
- [Background workers](host/background-workers.md) — 3 calls
- [Scene collections](host/scene-collections.md) — 3 calls
- [Scenes](host/scenes.md) — 10 calls
- [Scene items](host/scene-items.md) — 34 calls
- [Streaming bandwidth test](host/streaming-bandwidth-test.md) — 3 calls
- [Encoders](host/encoders.md) — 4 calls
- [Input source types](host/input-source-types.md) — 4 calls
- [Existing input sources](host/existing-input-sources.md) — 3 calls
- [Filter source types](host/filter-source-types.md) — 4 calls
- [Output settings](host/output-settings.md) — 3 calls
- [Streaming Outputs](host/streaming-outputs.md) — 5 calls
- [Recording Outputs](host/recording-outputs.md) — 6 calls
- [Replay Buffer Outputs](host/replay-buffer-outputs.md) — 6 calls
- [Native Integration with Streaming Services](host/native-integration-with-streaming-services.md) — 1 call
- [Streaming](host/streaming.md) — 6 calls
- [Notification bar](host/notification-bar.md) — 2 calls
- [Output preview area title bar](host/output-preview-area-title-bar.md) — 2 calls
- [Output preview area frame](host/output-preview-area-frame.md) — 2 calls
- [Status bar](host/status-bar.md) — 1 call
- [System resources](host/system-resources.md) — 3 calls
- [Hotkey bindings](host/hotkey-bindings.md) — 5 calls
- [Host information](host/host-information.md) — 2 calls
- [Message bus](host/message-bus.md) — 2 calls
- [Release groups](host/release-groups.md) — 3 calls
- [External Scene Data](host/external-scene-data.md) — 3 calls
- [HTTP Requests](host/http-requests.md) — 1 call
- [User interface state](host/user-interface-state.md) — 2 calls
- [Menu](host/menu.md) — 4 calls
- [Profiles](host/profiles.md) — 3 calls
- [Backup/Restore](host/backup-restore.md) — 4 calls
- [Browser Sources](host/browser-sources.md) — 1 call
- [Razer WYVRN](host/razer-wyvrn.md) — 2 calls
- [Defer settings save](host/defer-settings-save.md) — 2 calls
- [Invoke API methods in Batch Mode](host/invoke-api-methods-in-batch-mode.md) — 1 call
- [Synthetic input](host/synthetic-input.md) — 2 calls
- [HTTP server](host/http-server.md) — 4 calls
- [Scoped Storage](host/scoped-storage.md) — 4 calls
- [Audio & Video Composition Encoders](host/audio-video-composition-encoders.md) — 4 calls
- [Audio Compositions](host/audio-compositions.md) — 4 calls
- [Video Compositions](host/video-compositions.md) — 4 calls
- [Shared Video Compositions](host/shared-video-compositions.md) — 6 calls
- [Video Composition View Overlays](host/video-composition-view-overlays.md) — 3 calls
- [Screenshots](host/screenshots.md) — 1 call
- [Transitions](host/transitions.md) — 4 calls
- [Menus](host/menus.md) — 1 call
- [Modules](host/modules.md) — 2 calls

### All `window.host` calls, alphabetically

Calls marked ⚠️ are documented but **not registered by the plug-in** — deprecated and removed, or never implemented. Open one to see which.




| Call | Group |
| --- | --- |
| [`addAudioComposition(AudioCompositionInfo, ResultCallback< AudioCompositionInfo \| null>)`](host/audio-compositions.md#addaudiocompositionaudiocompositioninfo-resultcallback-audiocompositioninfo-null) | [Audio Compositions](host/audio-compositions.md) |
| [`addBackgroundWorker(BackgroundWorkerInfo, ResultCallback<workerId>)`](host/background-workers.md#addbackgroundworkerbackgroundworkerinfo-resultcallbackworkerid) | [Background workers](host/background-workers.md) |
| [`addBrowserScopedHttpServer(GenericNetworkServiceInfo, ResultCallback<GenericNetworkServiceInfo>)`](host/http-server.md#addbrowserscopedhttpservergenericnetworkserviceinfo-resultcallbackgenericnetworkserviceinfo) | [HTTP server](host/http-server.md) |
| [`addCurrentSceneItemBrowserSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addcurrentsceneitembrowsersourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addCurrentSceneItemGameCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addcurrentsceneitemgamecapturesourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addCurrentSceneItemGroup(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addcurrentsceneitemgroupsceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addCurrentSceneItemObsNativeSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addcurrentsceneitemobsnativesourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addCurrentSceneItemVideoCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addcurrentsceneitemvideocapturesourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addDockingWidget(DockingWidgetInfo, ResultCallback<widgetId>)`](host/docking-widgets.md#adddockingwidgetdockingwidgetinfo-resultcallbackwidgetid) | [Docking widgets](host/docking-widgets.md) |
| [`addHotkeyBinding(HotkeyBindingInfo, ResultCallback<boundHotkeyId>)`](host/hotkey-bindings.md#addhotkeybindinghotkeybindinginfo-resultcallbackboundhotkeyid) | [Hotkey bindings](host/hotkey-bindings.md) |
| [`addRecordingOutput(OutputInfo, ResultCallback<success>)`](host/recording-outputs.md#addrecordingoutputoutputinfo-resultcallbacksuccess) | [Recording Outputs](host/recording-outputs.md) |
| [`addReplayBufferOutput(OutputInfo, ResultCallback<success>)`](host/replay-buffer-outputs.md#addreplaybufferoutputoutputinfo-resultcallbacksuccess) | [Replay Buffer Outputs](host/replay-buffer-outputs.md) |
| [`addScene(SceneInfo, ResultCallback<sceneId>)`](host/scenes.md#addscenesceneinfo-resultcallbacksceneid) | [Scenes](host/scenes.md) |
| [`addSceneCollection(SceneCollectionInfo, ResultCallback<success>)`](host/scene-collections.md#addscenecollectionscenecollectioninfo-resultcallbacksuccess) | [Scene collections](host/scene-collections.md) |
| [`addSceneItemBrowserSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addsceneitembrowsersourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addSceneItemGameCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addsceneitemgamecapturesourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addSceneItemGroup(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addsceneitemgroupsceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addSceneItemObsNativeSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addsceneitemobsnativesourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addSceneItemVideoCaptureSource(SceneItemInfo, ResultCallback<SceneItemInfo>)`](host/scene-items.md#addsceneitemvideocapturesourcesceneiteminfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`addSharedVideoComposition(SharedVideoCompositionInfo, ResultCallback<SharedVideoCompositionInfo \| null>)`](host/shared-video-compositions.md#addsharedvideocompositionsharedvideocompositioninfo-resultcallbacksharedvideocompositioninfo-null) | [Shared Video Compositions](host/shared-video-compositions.md) |
| [`addStreamingOutput(OutputInfo, ResultCallback<success>)`](host/streaming-outputs.md#addstreamingoutputoutputinfo-resultcallbacksuccess) | [Streaming Outputs](host/streaming-outputs.md) |
| [`addVideoComposition(VideoCompositionInfo, ResultCallback< VideoCompositionInfo \| null>)`](host/video-compositions.md#addvideocompositionvideocompositioninfo-resultcallback-videocompositioninfo-null) | [Video Compositions](host/video-compositions.md) |
| [`adviseSignedIn(ResultCallback<success>)`](host/logged-in-notification.md#advisesignedinresultcallbacksuccess) | [Logged-in notification](host/logged-in-notification.md) |
| [`adviseSignedOut(ResultCallback<success>)`](host/logged-in-notification.md#advisesignedoutresultcallbacksuccess) | [Logged-in notification](host/logged-in-notification.md) |
| [`adviseStreamingStartUIRequestAccepted (ResultCallback<success>)`](host/streaming.md#advisestreamingstartuirequestaccepted-resultcallbacksuccess) | [Streaming](host/streaming.md) |
| [`adviseStreamingStartUIRequestRejected (ResultCallback<success>)`](host/streaming.md#advisestreamingstartuirequestrejected-resultcallbacksuccess) | [Streaming](host/streaming.md) |
| [`apiMajorVersion`](host/properties.md#apimajorversion) | [Properties](host/properties.md) |
| [`apiMinorVersion`](host/properties.md#apiminorversion) | [Properties](host/properties.md) |
| [`batchInvokeSeries(InvokeInfo[], ResultCallback<result[]>)`](host/invoke-api-methods-in-batch-mode.md#batchinvokeseriesinvokeinfo-resultcallbackresult) | [Invoke API methods in Batch Mode](host/invoke-api-methods-in-batch-mode.md) |
| [`beginDeferSaveTransaction(ResultCallback<transactionHandle>)`](host/defer-settings-save.md#begindefersavetransactionresultcallbacktransactionhandle) | [Defer settings save](host/defer-settings-save.md) |
| [`beginOnBoarding(ResultCallback<success>)`](host/on-boarding.md#beginonboardingresultcallbacksuccess) | [On-boarding](host/on-boarding.md) |
| [`broadcastEvent(EventInfo, ResultCallback<success>)`](host/message-bus.md#broadcasteventeventinfo-resultcallbacksuccess) | [Message bus](host/message-bus.md) |
| [`broadcastMessage(message<any>, ResultCallback<success>)`](host/message-bus.md#broadcastmessagemessageany-resultcallbacksuccess) | [Message bus](host/message-bus.md) |
| [`closeNonModalDialogsByIds(array<id>, ResultCallback<success>)`](host/non-modal-dialog.md#closenonmodaldialogsbyidsarrayid-resultcallbacksuccess) | [Non-Modal dialog](host/non-modal-dialog.md) |
| [`completeDeferSaveTransaction(transactionHandle, ResultCallback<success>)`](host/defer-settings-save.md#completedefersavetransactiontransactionhandle-resultcallbacksuccess) | [Defer settings save](host/defer-settings-save.md) |
| [`completeOnBoarding(ResultCallback<success>)`](host/on-boarding.md#completeonboardingresultcallbacksuccess) | [On-boarding](host/on-boarding.md) |
| [`connectVideoCompositionToSharedVideoComposition({ sharedVideoCompositionId: string, videoCompositionId: string }, ResultCallback<SharedVideoCompositionInfo>)`](host/shared-video-compositions.md#connectvideocompositiontosharedvideocomposition-sharedvideocompositionid-string-videocompositionid-string-resultcallbacksharedvideocompositioninfo) | [Shared Video Compositions](host/shared-video-compositions.md) |
| [`createUserEnvironmentBackupPackage(BackupRequestInfo, ResultCallback<BackupContentInfo>)`](host/backup-restore.md#createuserenvironmentbackuppackagebackuprequestinfo-resultcallbackbackupcontentinfo) | [Backup/Restore](host/backup-restore.md) |
| [`deleteAllCookies(ResultCallback<success>)`](host/cookies.md#deleteallcookiesresultcallbacksuccess) | [Cookies](host/cookies.md) |
| [`disableRecordingOutputsByIds(Array<id>, ResultCallback<success>)`](host/recording-outputs.md#disablerecordingoutputsbyidsarrayid-resultcallbacksuccess) | [Recording Outputs](host/recording-outputs.md) |
| [`disableReplayBufferOutputsByIds(Array<id>, ResultCallback<success>)`](host/replay-buffer-outputs.md#disablereplaybufferoutputsbyidsarrayid-resultcallbacksuccess) | [Replay Buffer Outputs](host/replay-buffer-outputs.md) |
| [`disableStreamingOutputsByIds(Array<id>, ResultCallback<success>)`](host/streaming-outputs.md#disablestreamingoutputsbyidsarrayid-resultcallbacksuccess) | [Streaming Outputs](host/streaming-outputs.md) |
| [`disconnectVideoCompositionsFromSharedVideoCompositionsByIds(Array<string>, ResultCallback<success>)`](host/shared-video-compositions.md#disconnectvideocompositionsfromsharedvideocompositionsbyidsarraystring-resultcallbacksuccess) | [Shared Video Compositions](host/shared-video-compositions.md) |
| [`⚠️ dispatchKeyboardEvent(KeyboardEventInfo, ResultCallback<success>)`](host/synthetic-input.md#dispatchkeyboardeventkeyboardeventinfo-resultcallbacksuccess) | [Synthetic input](host/synthetic-input.md) |
| [`⚠️ dispatchMouseEvent(MouseEventInfo, ResultCallback<success>)`](host/synthetic-input.md#dispatchmouseeventmouseeventinfo-resultcallbacksuccess) | [Synthetic input](host/synthetic-input.md) |
| [`enableRecordingOutputsByIds(Array<id>, ResultCallback<success>)`](host/recording-outputs.md#enablerecordingoutputsbyidsarrayid-resultcallbacksuccess) | [Recording Outputs](host/recording-outputs.md) |
| [`enableReplayBufferOutputsByIds(Array<id>, ResultCallback<success>)`](host/replay-buffer-outputs.md#enablereplaybufferoutputsbyidsarrayid-resultcallbacksuccess) | [Replay Buffer Outputs](host/replay-buffer-outputs.md) |
| [`enableStreamingOutputsByIds(Array<id>, ResultCallback<success>)`](host/streaming-outputs.md#enablestreamingoutputsbyidsarrayid-resultcallbacksuccess) | [Streaming Outputs](host/streaming-outputs.md) |
| [`endDialog(object, ResultCallback<success>)`](host/generic-dialog-api.md#enddialogobject-resultcallbacksuccess) | [Generic dialog API](host/generic-dialog-api.md) |
| [`endModalDialog(object, ResultCallback<success>)`](host/modal-dialog.md#endmodaldialogobject-resultcallbacksuccess) | [Modal dialog](host/modal-dialog.md) |
| [`endNonModalDialog(object, ResultCallback<success>)`](host/non-modal-dialog.md#endnonmodaldialogobject-resultcallbacksuccess) | [Non-Modal dialog](host/non-modal-dialog.md) |
| [`focusNonModalDialogById(id, ResultCallback<success>)`](host/non-modal-dialog.md#focusnonmodaldialogbyidid-resultcallbacksuccess) | [Non-Modal dialog](host/non-modal-dialog.md) |
| [`getAllAudioCompositions(ResultCallback< id: AudioCompositionInfo }>)`](host/audio-compositions.md#getallaudiocompositionsresultcallback-id-audiocompositioninfo) | [Audio Compositions](host/audio-compositions.md) |
| [`getAllAvailableAudioEncoderClasses(ResultCallback<ObsEncoderInfo[]>)`](host/audio-video-composition-encoders.md#getallavailableaudioencoderclassesresultcallbackobsencoderinfo) | [Audio & Video Composition Encoders](host/audio-video-composition-encoders.md) |
| [`getAllAvailableVideoEncoderClasses(ResultCallback<ObsEncoderInfo[]>)`](host/audio-video-composition-encoders.md#getallavailablevideoencoderclassesresultcallbackobsencoderinfo) | [Audio & Video Composition Encoders](host/audio-video-composition-encoders.md) |
| [`getAllBackgroundWorkers(ResultCallback<{id:BackgroundWorkerInfo}>)`](host/background-workers.md#getallbackgroundworkersresultcallbackidbackgroundworkerinfo) | [Background workers](host/background-workers.md) |
| [`getAllBrowserScopedHttpServers(ResultCallback<GenericNetworkServiceInfo[]>)`](host/http-server.md#getallbrowserscopedhttpserversresultcallbackgenericnetworkserviceinfo) | [HTTP server](host/http-server.md) |
| [`getAllCurrentSceneItems(ResultCallback<SceneItemInfo[]>)`](host/scene-items.md#getallcurrentsceneitemsresultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`getAllDockingWidgets(ResultCallback<{id:DockingWidgetInfo}>)`](host/docking-widgets.md#getalldockingwidgetsresultcallbackiddockingwidgetinfo) | [Docking widgets](host/docking-widgets.md) |
| [`getAllExistingAudioInputSources(ResultCallback<InputSourceTypeInfo[]>)`](host/existing-input-sources.md#getallexistingaudioinputsourcesresultcallbackinputsourcetypeinfo) | [Existing input sources](host/existing-input-sources.md) |
| [`getAllExistingInputSources(ResultCallback<InputSourceTypeInfo[]>)`](host/existing-input-sources.md#getallexistinginputsourcesresultcallbackinputsourcetypeinfo) | [Existing input sources](host/existing-input-sources.md) |
| [`getAllExistingVideoInputSources(ResultCallback<InputSourceTypeInfo[]>)`](host/existing-input-sources.md#getallexistingvideoinputsourcesresultcallbackinputsourcetypeinfo) | [Existing input sources](host/existing-input-sources.md) |
| [`getAllHotkeyBindings(ResultCallback<HotkeyBindingInfo[]>)`](host/hotkey-bindings.md#getallhotkeybindingsresultcallbackhotkeybindinginfo) | [Hotkey bindings](host/hotkey-bindings.md) |
| [`getAllLoadedHostModules(ResultCallback<LoadedHostModuleInfo[]>)`](host/modules.md#getallloadedhostmodulesresultcallbackloadedhostmoduleinfo) | [Modules](host/modules.md) |
| [`getAllManagedHotkeyBindings(ResultCallback<HotkeyBindingInfo[]>)`](host/hotkey-bindings.md#getallmanagedhotkeybindingsresultcallbackhotkeybindinginfo) | [Hotkey bindings](host/hotkey-bindings.md) |
| [`getAllNonModalDialogs(ResultCallback<{id: DialogInfo}>)`](host/non-modal-dialog.md#getallnonmodaldialogsresultcallbackid-dialoginfo) | [Non-Modal dialog](host/non-modal-dialog.md) |
| [`getAllProfiles(ResultCallback<ProfileInfo[]>)`](host/profiles.md#getallprofilesresultcallbackprofileinfo) | [Profiles](host/profiles.md) |
| [`⚠️ getAllRazerWyvrnEvents(ResultCallback<RazerWyvrnEventInfo[]>)`](host/razer-wyvrn.md#getallrazerwyvrneventsresultcallbackrazerwyvrneventinfo) | [Razer WYVRN](host/razer-wyvrn.md) |
| [`getAllRecordingOutputs(ResultCallback<{id: OutputInfo}>)`](host/recording-outputs.md#getallrecordingoutputsresultcallbackid-outputinfo) | [Recording Outputs](host/recording-outputs.md) |
| [`getAllReplayBufferOutputs(ResultCallback<{id: OutputInfo}>)`](host/replay-buffer-outputs.md#getallreplaybufferoutputsresultcallbackid-outputinfo) | [Replay Buffer Outputs](host/replay-buffer-outputs.md) |
| [`getAllSceneCollections(ResultCallback<SceneCollectionInfo[]>)`](host/scene-collections.md#getallscenecollectionsresultcallbackscenecollectioninfo) | [Scene collections](host/scene-collections.md) |
| [`getAllSceneItems(SceneInfo, ResultCallback<SceneItemInfo[]>)`](host/scene-items.md#getallsceneitemssceneinfo-resultcallbacksceneiteminfo) | [Scene items](host/scene-items.md) |
| [`getAllScenes(ResultCallback<SceneInfo[]>)`](host/scenes.md#getallscenesresultcallbacksceneinfo) | [Scenes](host/scenes.md) |
| [`getAllScenes({ videoCompositionId }, ResultCallback<SceneInfo[]>)`](host/scenes.md#getallscenes-videocompositionid-resultcallbacksceneinfo) | [Scenes](host/scenes.md) |
| [`getAllScopedStorageJsonItems(ScopedStorageItemInfo, ResultCallback< ScopedStorageItemInfo [] \| null>)`](host/scoped-storage.md#getallscopedstoragejsonitemsscopedstorageiteminfo-resultcallback-scopedstorageiteminfo-null) | [Scoped Storage](host/scoped-storage.md) |
| [`getAllSharedVideoCompositions(ResultCallback<{ id: SharedVideoCompositionInfo }>)`](host/shared-video-compositions.md#getallsharedvideocompositionsresultcallback-id-sharedvideocompositioninfo) | [Shared Video Compositions](host/shared-video-compositions.md) |
| [`getAllStreamingOutputs(ResultCallback<{id: OutputInfo}>)`](host/streaming-outputs.md#getallstreamingoutputsresultcallbackid-outputinfo) | [Streaming Outputs](host/streaming-outputs.md) |
| [`getAllVideoCompositions(ResultCallback<{ id: VideoCompositionInfo }>)`](host/video-compositions.md#getallvideocompositionsresultcallback-id-videocompositioninfo) | [Video Compositions](host/video-compositions.md) |
| [`getAuxiliaryMenuItems(ResultCallback<MenuItemInfo[]>)`](host/menu.md#getauxiliarymenuitemsresultcallbackmenuiteminfo) | [Menu](host/menu.md) |
| [`getAvailableAudioEncoderClassProperties(ObsEncoderInfo, ResultCallback<ObsEncoderInfo>)`](host/audio-video-composition-encoders.md#getavailableaudioencoderclasspropertiesobsencoderinfo-resultcallbackobsencoderinfo) | [Audio & Video Composition Encoders](host/audio-video-composition-encoders.md) |
| [`getAvailableAudioEncoders(ResultCallback<EncoderInfo[]>)`](host/encoders.md#getavailableaudioencodersresultcallbackencoderinfo) | [Encoders](host/encoders.md) |
| [`getAvailableAudioFilterSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`](host/filter-source-types.md#getavailableaudiofiltersourcetypesresultcallbackinputsourcetypeinfo) | [Filter source types](host/filter-source-types.md) |
| [`getAvailableAudioInputSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`](host/input-source-types.md#getavailableaudioinputsourcetypesresultcallbackinputsourcetypeinfo) | [Input source types](host/input-source-types.md) |
| [`getAvailableEncoders(ResultCallback<EncoderInfo[]>)`](host/encoders.md#getavailableencodersresultcallbackencoderinfo) | [Encoders](host/encoders.md) |
| [`getAvailableFilterSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`](host/filter-source-types.md#getavailablefiltersourcetypesresultcallbackinputsourcetypeinfo) | [Filter source types](host/filter-source-types.md) |
| [`getAvailableInputSourceClasses(ResultCallback<string[]>)`](host/scene-items.md#getavailableinputsourceclassesresultcallbackstring) | [Scene items](host/scene-items.md) |
| [`getAvailableInputSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`](host/input-source-types.md#getavailableinputsourcetypesresultcallbackinputsourcetypeinfo) | [Input source types](host/input-source-types.md) |
| [`getAvailableTransitionClasses(ResultCallback<TransitionInfo[]>)`](host/transitions.md#getavailabletransitionclassesresultcallbacktransitioninfo) | [Transitions](host/transitions.md) |
| [`getAvailableVideoEncoderClassProperties(ObsEncoderInfo, ResultCallback<ObsEncoderInfo>)`](host/audio-video-composition-encoders.md#getavailablevideoencoderclasspropertiesobsencoderinfo-resultcallbackobsencoderinfo) | [Audio & Video Composition Encoders](host/audio-video-composition-encoders.md) |
| [`getAvailableVideoEncoders(ResultCallback<EncoderInfo[]>)`](host/encoders.md#getavailablevideoencodersresultcallbackencoderinfo) | [Encoders](host/encoders.md) |
| [`getAvailableVideoFilterSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`](host/filter-source-types.md#getavailablevideofiltersourcetypesresultcallbackinputsourcetypeinfo) | [Filter source types](host/filter-source-types.md) |
| [`getAvailableVideoInputSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`](host/input-source-types.md#getavailablevideoinputsourcetypesresultcallbackinputsourcetypeinfo) | [Input source types](host/input-source-types.md) |
| [`⚠️ getContainerForeignPopupWindowsProperties(ResultCallback<ForeignPopupWindowsInfo>)`](host/popup-window.md#getcontainerforeignpopupwindowspropertiesresultcallbackforeignpopupwindowsinfo) | [Popup window](host/popup-window.md) |
| [`getCurrentContainerProperties(ResultCallback<ContainerInfo>)`](host/container-information.md#getcurrentcontainerpropertiesresultcallbackcontainerinfo) | [Container information](host/container-information.md) |
| [`getCurrentProfile(ResultCallback<ProfileInfo>)`](host/profiles.md#getcurrentprofileresultcallbackprofileinfo) | [Profiles](host/profiles.md) |
| [`getCurrentScene(ResultCallback<SceneInfo>)`](host/scenes.md#getcurrentsceneresultcallbacksceneinfo) | [Scenes](host/scenes.md) |
| [`getCurrentScene({ videoCompositionId }, ResultCallback<SceneInfo>)`](host/scenes.md#getcurrentscene-videocompositionid-resultcallbacksceneinfo) | [Scenes](host/scenes.md) |
| [`getCurrentSceneCollectionProperties(ResultCallback<SceneCollectionInfo>)`](host/scene-collections.md#getcurrentscenecollectionpropertiesresultcallbackscenecollectioninfo) | [Scene collections](host/scene-collections.md) |
| [`getCurrentSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`](host/scene-items.md#getcurrentsceneitempropertiesbyidsceneiteminfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`getCurrentSceneItemsAuxiliaryActions(ResultCallback<ActionInfo[]>)`](host/scene-items.md#getcurrentsceneitemsauxiliaryactionsresultcallbackactioninfo) | [Scene items](host/scene-items.md) |
| [`getEncoderProperties(EncoderInfo, ResultCallback<ObsEncoderInfo \| null>)`](host/encoders.md#getencoderpropertiesencoderinfo-resultcallbackobsencoderinfo-null) | [Encoders](host/encoders.md) |
| [`getEncodingSettings(ResultCallback<EncodingSettings>)`](host/output-settings.md#getencodingsettingsresultcallbackencodingsettings) | [Output settings](host/output-settings.md) |
| [`getExternalSceneDataProviders(ResultCallback<ExternalSceneDataProviderInfo>)`](host/external-scene-data.md#getexternalscenedataprovidersresultcallbackexternalscenedataproviderinfo) | [External Scene Data](host/external-scene-data.md) |
| [`getExternalSceneDataSceneCollectionContent(ExternalSceneDataSceneCollectionInfo, ResultCallback<ExternalSceneDataSceneCollectionContent>)`](host/external-scene-data.md#getexternalscenedatascenecollectioncontentexternalscenedatascenecollectioninfo-resultcallbackexternalscenedatascenecollectioncontent) | [External Scene Data](host/external-scene-data.md) |
| [`getExternalSceneDataSceneCollections(ExternalSceneDataProviderInfo, ResultCallback<ExternalSceneDataSceneCollectionInfo>)`](host/external-scene-data.md#getexternalscenedatascenecollectionsexternalscenedataproviderinfo-resultcallbackexternalscenedatascenecollectioninfo) | [External Scene Data](host/external-scene-data.md) |
| [`getFilterSourceProperties(InputSourceTypeInfo, ResultCallback<InputSourceTypeInfo \| null>)`](host/filter-source-types.md#getfiltersourcepropertiesinputsourcetypeinfo-resultcallbackinputsourcetypeinfo-null) | [Filter source types](host/filter-source-types.md) |
| [`⚠️ getHostCapabilities(ResultCallback<HostCapabilities>)`](host/host-information.md#gethostcapabilitiesresultcallbackhostcapabilities) | [Host information](host/host-information.md) |
| [`getHostProperties(ResultCallback<HostInfo>)`](host/host-information.md#gethostpropertiesresultcallbackhostinfo) | [Host information](host/host-information.md) |
| [`getHostReleaseGroupProperties(ResultCallback<ReleaseGroupInfo>)`](host/release-groups.md#gethostreleasegrouppropertiesresultcallbackreleasegroupinfo) | [Release groups](host/release-groups.md) |
| [`getInputSourceProperties(InputSourceTypeInfo, ResultCallback<InputSourceTypeInfo \| null>)`](host/input-source-types.md#getinputsourcepropertiesinputsourcetypeinfo-resultcallbackinputsourcetypeinfo-null) | [Input source types](host/input-source-types.md) |
| [`getNativeStreamingServiceIntegrationStatus(ResultCallback<NativeStreamingServiceIntegrationInfo>`](host/native-integration-with-streaming-services.md#getnativestreamingserviceintegrationstatusresultcallbacknativestreamingserviceintegrationinfo) | [Native Integration with Streaming Services](host/native-integration-with-streaming-services.md) |
| [`getSceneItemBoundingBoxInViewport(SceneItemInfo or ViewportSceneItemGeometryInfo, ResultCallback<ViewportSceneItemGeometryInfo>)`](host/scene-items.md#getsceneitemboundingboxinviewportsceneiteminfo-or-viewportsceneitemgeometryinfo-resultcallbackviewportsceneitemgeometryinfo) | [Scene items](host/scene-items.md) |
| [`getSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`](host/scene-items.md#getsceneitempropertiesbyidsceneiteminfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`getSceneItemRotationInViewport(SceneItemInfo or ViewportSceneItemRotationInfo, ResultCallback<ViewportSceneItemRotationInfo>)`](host/scene-items.md#getsceneitemrotationinviewportsceneiteminfo-or-viewportsceneitemrotationinfo-resultcallbackviewportsceneitemrotationinfo) | [Scene items](host/scene-items.md) |
| [`getScenesAuxiliaryActions(ResultCallback<ActionInfo[]>)`](host/scenes.md#getscenesauxiliaryactionsresultcallbackactioninfo) | [Scenes](host/scenes.md) |
| [`getShowBuiltInMenuItems(ResultCallback<bool>)`](host/menu.md#getshowbuiltinmenuitemsresultcallbackbool) | [Menu](host/menu.md) |
| [`getSourceClassProperties(SceneItemInfo, RestulCallback<ObsPropertyInfo[]>)`](host/scene-items.md#getsourceclasspropertiessceneiteminfo-restulcallbackobspropertyinfo) | [Scene items](host/scene-items.md) |
| [`getStartupFlags(ResultCallback<number<flags>>)`](host/startup-flags.md#getstartupflagsresultcallbacknumberflags) | [Startup flags](host/startup-flags.md) |
| [`getStreamingStatus(ResultCallback<StreamingStatusInfo>)`](host/streaming.md#getstreamingstatusresultcallbackstreamingstatusinfo) | [Streaming](host/streaming.md) |
| [`getSystemCPUUsageTimes(ResultCallback<SystemCPUUsageTimes>)`](host/system-resources.md#getsystemcpuusagetimesresultcallbacksystemcpuusagetimes) | [System resources](host/system-resources.md) |
| [`getSystemHardwareProperties(ResultCallback<SystemHardwareInfo>)`](host/system-resources.md#getsystemhardwarepropertiesresultcallbacksystemhardwareinfo) | [System resources](host/system-resources.md) |
| [`getSystemMemoryUsage(ResultCallback<SystemMemoryUsageInfo>)`](host/system-resources.md#getsystemmemoryusageresultcallbacksystemmemoryusageinfo) | [System resources](host/system-resources.md) |
| [`getUserInterfaceState(ResultCallback<UserInterfaceStateProperties>)`](host/user-interface-state.md#getuserinterfacestateresultcallbackuserinterfacestateproperties) | [User interface state](host/user-interface-state.md) |
| [`getVideoCompositionTransition(object<{ videoCompositionId }>, ResultCallback<TransitionInfo \| null \| false>)`](host/transitions.md#getvideocompositiontransitionobject-videocompositionid-resultcallbacktransitioninfo-null-false) | [Transitions](host/transitions.md) |
| [`getVideoCompositionViewOverlayProperties(ResultCallback<VideoCompositionViewOverlayInfo \| null>)`](host/video-composition-view-overlays.md#getvideocompositionviewoverlaypropertiesresultcallbackvideocompositionviewoverlayinfo-null) | [Video Composition View Overlays](host/video-composition-view-overlays.md) |
| [`groupDockingWidgetsPairByIds(firstWidgetId, secondWidgetId, ResultCallback<success>)`](host/docking-widgets.md#groupdockingwidgetspairbyidsfirstwidgetid-secondwidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`hideCentralWidget(ResultCallback<success>)`](host/central-widget.md#hidecentralwidgetresultcallbacksuccess) | [Central widget](host/central-widget.md) |
| [`hideDockingWidgetById(string<widgetId>, ResultCallback<success>)`](host/docking-widgets.md#hidedockingwidgetbyidstringwidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`hideNotificationBar(ResultCallback<success>)`](host/notification-bar.md#hidenotificationbarresultcallbacksuccess) | [Notification bar](host/notification-bar.md) |
| [`hideOutputPreviewFrame (ResultCallback<success>)`](host/output-preview-area-frame.md#hideoutputpreviewframe-resultcallbacksuccess) | [Output preview area frame](host/output-preview-area-frame.md) |
| [`hideOutputPreviewTitleBar (ResultCallback<success>)`](host/output-preview-area-title-bar.md#hideoutputpreviewtitlebar-resultcallbacksuccess) | [Output preview area title bar](host/output-preview-area-title-bar.md) |
| [`hostContainerHidden`](host/properties.md#hostcontainerhidden) | [Properties](host/properties.md) |
| [`hostReady`](host/properties.md#hostready) | [Properties](host/properties.md) |
| [`httpRequestText(HTTPRequestInfo, ResultCallback<HTTPResponseInfo>)`](host/http-requests.md#httprequesttexthttprequestinfo-resultcallbackhttpresponseinfo) | [HTTP Requests](host/http-requests.md) |
| [`insertDockingWidgetAfterId(firstWidgetId, secondWidgetId, ResultCallback<success>)`](host/docking-widgets.md#insertdockingwidgetafteridfirstwidgetid-secondwidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`insertDockingWidgetBeforeId(firstWidgetId, secondWidgetId, ResultCallback<success>)`](host/docking-widgets.md#insertdockingwidgetbeforeidfirstwidgetid-secondwidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`invokeCurrentSceneItemDefaultActionById(string<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#invokecurrentsceneitemdefaultactionbyidstringsceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`invokeCurrentSceneItemDefaultContextMenuById(string<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#invokecurrentsceneitemdefaultcontextmenubyidstringsceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`openDefaultBrowser(url, ResultCallback<success>)`](host/external-browser.md#opendefaultbrowserurl-resultcallbacksuccess) | [External browser](host/external-browser.md) |
| [`openFileLocationInHostFileManager(string<filePath>, ResultCallback<success>)`](host/modules.md#openfilelocationinhostfilemanagerstringfilepath-resultcallbacksuccess) | [Modules](host/modules.md) |
| [`openPopupWindow(PopupWindowInfo, ResultCallback<success>)`](host/popup-window.md#openpopupwindowpopupwindowinfo-resultcallbacksuccess) | [Popup window](host/popup-window.md) |
| [`openSceneItemFiltersDialogById(string<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#opensceneitemfiltersdialogbyidstringsceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`openSceneItemInteractionDialogById(string<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#opensceneiteminteractiondialogbyidstringsceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`openSceneItemPropertiesDialogById(string<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#opensceneitempropertiesdialogbyidstringsceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`openSceneItemTransformEditorDialogById(string<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#opensceneitemtransformeditordialogbyidstringsceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`queryHostReleaseGroupUpdateAvailability([QuerySoftwareUpdateArgs], ResultCallback<success>)`](host/release-groups.md#queryhostreleasegroupupdateavailabilityquerysoftwareupdateargs-resultcallbacksuccess) | [Release groups](host/release-groups.md) |
| [`queryUserEnvironmentBackupPackageContent(BackupContentInfo, ResultCallback<BackupContentInfo>)`](host/backup-restore.md#queryuserenvironmentbackuppackagecontentbackupcontentinfo-resultcallbackbackupcontentinfo) | [Backup/Restore](host/backup-restore.md) |
| [`queryUserEnvironmentBackupReferencedFiles (BackupRequestInfo, ResultCallback<BackupContentInfo>)`](host/backup-restore.md#queryuserenvironmentbackupreferencedfiles-backuprequestinfo-resultcallbackbackupcontentinfo) | [Backup/Restore](host/backup-restore.md) |
| [`readScopedStorageJsonItem(ScopedStorageItemInfo, ResultCallback< ScopedStorageItemInfo \| null>)`](host/scoped-storage.md#readscopedstoragejsonitemscopedstorageiteminfo-resultcallback-scopedstorageiteminfo-null) | [Scoped Storage](host/scoped-storage.md) |
| [`⚠️ reloadAllBrowserSources(ResultCallback<success>)`](host/browser-sources.md#reloadallbrowsersourcesresultcallbacksuccess) | [Browser Sources](host/browser-sources.md) |
| [`removeAudioCompositionsByIds(Array<audioCompositionId>, ResultCallback<success>)`](host/audio-compositions.md#removeaudiocompositionsbyidsarrayaudiocompositionid-resultcallbacksuccess) | [Audio Compositions](host/audio-compositions.md) |
| [`removeBackgroundWorkersByIds(array<workerId>, ResultCallback<success>)`](host/background-workers.md#removebackgroundworkersbyidsarrayworkerid-resultcallbacksuccess) | [Background workers](host/background-workers.md) |
| [`removeBrowserScopedHttpServersByIds(<string\|string[]>, ResultCallback<success>)`](host/http-server.md#removebrowserscopedhttpserversbyidsstringstring-resultcallbacksuccess) | [HTTP server](host/http-server.md) |
| [`removeCurrentSceneItemsByIds(array<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#removecurrentsceneitemsbyidsarraysceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`removeDockingWidgetsByIds(array<widgetId>, ResultCallback<success>)`](host/docking-widgets.md#removedockingwidgetsbyidsarraywidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`removeHotkeyBindingById(hotkeyId, ResultCallback<success>)`](host/hotkey-bindings.md#removehotkeybindingbyidhotkeyid-resultcallbacksuccess) | [Hotkey bindings](host/hotkey-bindings.md) |
| [`removeRecordingOutputsByIds(Array<id>, ResultCallback<success>)`](host/recording-outputs.md#removerecordingoutputsbyidsarrayid-resultcallbacksuccess) | [Recording Outputs](host/recording-outputs.md) |
| [`removeReplayBufferOutputsByIds(Array<id>, ResultCallback<success>)`](host/replay-buffer-outputs.md#removereplaybufferoutputsbyidsarrayid-resultcallbacksuccess) | [Replay Buffer Outputs](host/replay-buffer-outputs.md) |
| [`removeSceneItemsByIds(array<sceneItemId>, ResultCallback<success>)`](host/scene-items.md#removesceneitemsbyidsarraysceneitemid-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`removeScenesByIds(array<sceneId>, ResultCallback<success>)`](host/scenes.md#removescenesbyidsarraysceneid-resultcallbacksuccess) | [Scenes](host/scenes.md) |
| [`removeScopedStorageJsonItem(ScopedStorageItemInfo, ResultCallback< ScopedStorageItemInfo \| null>)`](host/scoped-storage.md#removescopedstoragejsonitemscopedstorageiteminfo-resultcallback-scopedstorageiteminfo-null) | [Scoped Storage](host/scoped-storage.md) |
| [`removeSharedVideoCompositionsByIds(Array<string>, ResultCallback<success>)`](host/shared-video-compositions.md#removesharedvideocompositionsbyidsarraystring-resultcallbacksuccess) | [Shared Video Compositions](host/shared-video-compositions.md) |
| [`removeStreamingOutputsByIds(Array<id>, ResultCallback<success>)`](host/streaming-outputs.md#removestreamingoutputsbyidsarrayid-resultcallbacksuccess) | [Streaming Outputs](host/streaming-outputs.md) |
| [`removeVideoCompositionsByIds(Array<videoCompositionId>, ResultCallback<success>)`](host/video-compositions.md#removevideocompositionsbyidsarrayvideocompositionid-resultcallbacksuccess) | [Video Compositions](host/video-compositions.md) |
| [`removeVideoCompositionViewOverlay(ResultCallback<success>)`](host/video-composition-view-overlays.md#removevideocompositionviewoverlayresultcallbacksuccess) | [Video Composition View Overlays](host/video-composition-view-overlays.md) |
| [`requestStreamingStart(ResultCallback<success>)`](host/streaming.md#requeststreamingstartresultcallbacksuccess) | [Streaming](host/streaming.md) |
| [`requestStreamingStop(ResultCallback<success>)`](host/streaming.md#requeststreamingstopresultcallbacksuccess) | [Streaming](host/streaming.md) |
| [`restoreUserEnvironmentBackupPackageContent(BackupContentInfo, ResultCallback<success>)`](host/backup-restore.md#restoreuserenvironmentbackuppackagecontentbackupcontentinfo-resultcallbacksuccess) | [Backup/Restore](host/backup-restore.md) |
| [`sendHttpRequestResponse(string<id>, HTTPResponseInfo, ResultCallback<success>)`](host/http-server.md#sendhttprequestresponsestringid-httpresponseinfo-resultcallbacksuccess) | [HTTP server](host/http-server.md) |
| [`setAudioCompositionProperties(AudioCompositionInfo, ResultCallback< AudioCompositionInfo \| null>)`](host/audio-compositions.md#setaudiocompositionpropertiesaudiocompositioninfo-resultcallback-audiocompositioninfo-null) | [Audio Compositions](host/audio-compositions.md) |
| [`setAuxiliaryMenuItems(MenuItemInfo[], ResultCallback<success>)`](host/menu.md#setauxiliarymenuitemsmenuiteminfo-resultcallbacksuccess) | [Menu](host/menu.md) |
| [`⚠️ setContainerForeignPopupWindowsProperties(ForeignPopupWindowsInfo, ResultCallback<success>)`](host/popup-window.md#setcontainerforeignpopupwindowspropertiesforeignpopupwindowsinfo-resultcallbacksuccess) | [Popup window](host/popup-window.md) |
| [`⚠️ setCurrentProfileById(ProfileInfo, ResultCallback<success>)`](host/profiles.md#setcurrentprofilebyidprofileinfo-resultcallbacksuccess) | [Profiles](host/profiles.md) |
| [`setCurrentSceneById(sceneId, ResultCallback<success>)`](host/scenes.md#setcurrentscenebyidsceneid-resultcallbacksuccess) | [Scenes](host/scenes.md) |
| [`setCurrentSceneCollectionById(sceneCollectionId, ResultCallback<success>)`](host/scene-collections.md#setcurrentscenecollectionbyidscenecollectionid-resultcallbacksuccess) | [Scene collections](host/scene-collections.md) |
| [`setCurrentSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`](host/scene-items.md#setcurrentsceneitempropertiesbyidsceneiteminfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`setCurrentSceneItemsAuxiliaryActions(ActionInfo[], ResultCallback<success>)`](host/scene-items.md#setcurrentsceneitemsauxiliaryactionsactioninfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`setDockingWidgetDimensionsById(widgetId, DimensionsInfo, ResultCallback<success>)`](host/docking-widgets.md#setdockingwidgetdimensionsbyidwidgetid-dimensionsinfo-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`setDockingWidgetPositionById(widgetId, PositionInfo, ResultCallback<success>)`](host/docking-widgets.md#setdockingwidgetpositionbyidwidgetid-positioninfo-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`setDockingWidgetTitleById(widgetId, title, ResultCallback<success>)`](host/docking-widgets.md#setdockingwidgettitlebyidwidgetid-title-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`setDockingWidgetUrlById(widgetId, url, ResultCallback<success>)`](host/docking-widgets.md#setdockingwidgeturlbyidwidgetid-url-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`setEncodingSettings(EncodingSettings, ResultCallback<success>)`](host/output-settings.md#setencodingsettingsencodingsettings-resultcallbacksuccess) | [Output settings](host/output-settings.md) |
| [`setHostReleaseGroupProperties(quality<ReleaseGroupInfo>, ResultCallback<success>)`](host/release-groups.md#sethostreleasegrouppropertiesqualityreleasegroupinfo-resultcallbacksuccess) | [Release groups](host/release-groups.md) |
| [`setHotkeyBindingTriggers(Pick<HotkeyBindingInfo, ‘id’ \| ‘triggers’>, ResultCallback<success>)`](host/hotkey-bindings.md#sethotkeybindingtriggerspickhotkeybindinginfo-id-triggers-resultcallbacksuccess) | [Hotkey bindings](host/hotkey-bindings.md) |
| [`setNonModalDialogDimensionsById(id, DimensionsInfo, ResultCallback<success>)`](host/non-modal-dialog.md#setnonmodaldialogdimensionsbyidid-dimensionsinfo-resultcallbacksuccess) | [Non-Modal dialog](host/non-modal-dialog.md) |
| [`⚠️ setRazerWyvrnEventName(RazerWyvrnEventInfo, ResultCallback<success>)`](host/razer-wyvrn.md#setrazerwyvrneventnamerazerwyvrneventinfo-resultcallbacksuccess) | [Razer WYVRN](host/razer-wyvrn.md) |
| [`setSceneItemPositionInViewport(ViewportSceneItemGeometryInfo, ResultCallback< ViewportSceneItemGeometryInfo>)`](host/scene-items.md#setsceneitempositioninviewportviewportsceneitemgeometryinfo-resultcallback-viewportsceneitemgeometryinfo) | [Scene items](host/scene-items.md) |
| [`setSceneItemPropertiesById(SceneItemInfo, ResultCallback<success>)`](host/scene-items.md#setsceneitempropertiesbyidsceneiteminfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`setSceneItemRotationInViewport(ViewportSceneItemRotationInfo, ResultCallback< ViewportSceneItemRotationInfo>)`](host/scene-items.md#setsceneitemrotationinviewportviewportsceneitemrotationinfo-resultcallback-viewportsceneitemrotationinfo) | [Scene items](host/scene-items.md) |
| [`setScenePropertiesById(SceneInfo, ResultCallback<success>)`](host/scenes.md#setscenepropertiesbyidsceneinfo-resultcallbacksuccess) | [Scenes](host/scenes.md) |
| [`setScenesAuxiliaryActions(ActionInfo[], ResultCallback<success>)`](host/scenes.md#setscenesauxiliaryactionsactioninfo-resultcallbacksuccess) | [Scenes](host/scenes.md) |
| [`setSharedVideoCompositionProperties(SharedVideoCompositionInfo, ResultCallback<SharedVideoCompositionInfo \| null>)`](host/shared-video-compositions.md#setsharedvideocompositionpropertiessharedvideocompositioninfo-resultcallbacksharedvideocompositioninfo-null) | [Shared Video Compositions](host/shared-video-compositions.md) |
| [`setShowBuiltInMenuItems(bool<show>, ResultCallback<success>)`](host/menu.md#setshowbuiltinmenuitemsboolshow-resultcallbacksuccess) | [Menu](host/menu.md) |
| [`setStartupFlags(number<flags>, ResultCallback<success>)`](host/startup-flags.md#setstartupflagsnumberflags-resultcallbacksuccess) | [Startup flags](host/startup-flags.md) |
| [`setStreamingSettings(ResultCallback<success>)`](host/output-settings.md#setstreamingsettingsresultcallbacksuccess) | [Output settings](host/output-settings.md) |
| [`setStreamingStartUIHandlerProperties (StreamingStartUIHandlerProperties, ResultCallback<success>)`](host/streaming.md#setstreamingstartuihandlerproperties-streamingstartuihandlerproperties-resultcallbacksuccess) | [Streaming](host/streaming.md) |
| [`setUserInterfaceState(UserInterfaceStateProperties , ResultCallback<success>)`](host/user-interface-state.md#setuserinterfacestateuserinterfacestateproperties-resultcallbacksuccess) | [User interface state](host/user-interface-state.md) |
| [`setVideoCompositionProperties(VideoCompositionInfo, ResultCallback< VideoCompositionInfo \| null>)`](host/video-compositions.md#setvideocompositionpropertiesvideocompositioninfo-resultcallback-videocompositioninfo-null) | [Video Compositions](host/video-compositions.md) |
| [`setVideoCompositionTransition(TransitionInfo, ResultCallback<TransitionInfo \| null \| false>)`](host/transitions.md#setvideocompositiontransitiontransitioninfo-resultcallbacktransitioninfo-null-false) | [Transitions](host/transitions.md) |
| [`setVideoCompositionViewOverlayProperties(VideoCompositionViewOverlayInfo, ResultCallback<VideoCompositionViewOverlayInfo \| null>)`](host/video-composition-view-overlays.md#setvideocompositionviewoverlaypropertiesvideocompositionviewoverlayinfo-resultcallbackvideocompositionviewoverlayinfo-null) | [Video Composition View Overlays](host/video-composition-view-overlays.md) |
| [`showCentralWidget(CentralWidgetInfo, ResultCallback<success>)`](host/central-widget.md#showcentralwidgetcentralwidgetinfo-resultcallbacksuccess) | [Central widget](host/central-widget.md) |
| [`showDockingWidgetById(string<widgetId>, ResultCallback<success>)`](host/docking-widgets.md#showdockingwidgetbyidstringwidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`showModalDialog(DialogInfo, ResultCallback<object>)`](host/modal-dialog.md#showmodaldialogdialoginfo-resultcallbackobject) | [Modal dialog](host/modal-dialog.md) |
| [`showNonModalDialog(DialogInfo, ResultCallback<object>)`](host/non-modal-dialog.md#shownonmodaldialogdialoginfo-resultcallbackobject) | [Non-Modal dialog](host/non-modal-dialog.md) |
| [`showNotificationBar(NotificationBarInfo, ResultCallback<success>)`](host/notification-bar.md#shownotificationbarnotificationbarinfo-resultcallbacksuccess) | [Notification bar](host/notification-bar.md) |
| [`showOutputPreviewFrame (FrameInfo, ResultCallback<success>)`](host/output-preview-area-frame.md#showoutputpreviewframe-frameinfo-resultcallbacksuccess) | [Output preview area frame](host/output-preview-area-frame.md) |
| [`showOutputPreviewTitleBar (NotificationBarInfo, ResultCallback<success>)`](host/output-preview-area-title-bar.md#showoutputpreviewtitlebar-notificationbarinfo-resultcallbacksuccess) | [Output preview area title bar](host/output-preview-area-title-bar.md) |
| [`showPopupMenuAtMousePointerPosition(MenuItemInfo[], ResultCallback<success>)`](host/menus.md#showpopupmenuatmousepointerpositionmenuiteminfo-resultcallbacksuccess) | [Menus](host/menus.md) |
| [`showStatusBarTemporaryMessage(StatusBarMessageInfo, ResultCallback<success>)`](host/status-bar.md#showstatusbartemporarymessagestatusbarmessageinfo-resultcallbacksuccess) | [Status bar](host/status-bar.md) |
| [`showVideoCompositionTransitionPropertiesDialog(object<{ videoCompositionId }>, ResultCallback<success>)`](host/transitions.md#showvideocompositiontransitionpropertiesdialogobject-videocompositionid-resultcallbacksuccess) | [Transitions](host/transitions.md) |
| [`streamingBandwidthTestBegin(BandwidthTestSettings, ServerInfo[], ResultCallback<success>)`](host/streaming-bandwidth-test.md#streamingbandwidthtestbeginbandwidthtestsettings-serverinfo-resultcallbacksuccess) | [Streaming bandwidth test](host/streaming-bandwidth-test.md) |
| [`streamingBandwidthTestEnd(stopIfRunning, ResultCallback<BandwidthTestStatus>)`](host/streaming-bandwidth-test.md#streamingbandwidthtestendstopifrunning-resultcallbackbandwidthteststatus) | [Streaming bandwidth test](host/streaming-bandwidth-test.md) |
| [`streamingBandwidthTestGetStatus(ResultCallback<BandwidthTestStatus>)`](host/streaming-bandwidth-test.md#streamingbandwidthtestgetstatusresultcallbackbandwidthteststatus) | [Streaming bandwidth test](host/streaming-bandwidth-test.md) |
| [`takeScreenshot(Object<{ videoCompositionId?: string }>, ResultCallback<success>)`](host/screenshots.md#takescreenshotobject-videocompositionid-string-resultcallbacksuccess) | [Screenshots](host/screenshots.md) |
| [`toggleDockingWidgetFloatingById(widgetId, ResultCallback<success>)`](host/docking-widgets.md#toggledockingwidgetfloatingbyidwidgetid-resultcallbacksuccess) | [Docking widgets](host/docking-widgets.md) |
| [`triggerRecordingOutputSplitById(string<id>, ResultCallback<success>)`](host/recording-outputs.md#triggerrecordingoutputsplitbyidstringid-resultcallbacksuccess) | [Recording Outputs](host/recording-outputs.md) |
| [`triggerReplayBufferOutputSaveById(string<id>, ResultCallback<success>)`](host/replay-buffer-outputs.md#triggerreplaybufferoutputsavebyidstringid-resultcallbacksuccess) | [Replay Buffer Outputs](host/replay-buffer-outputs.md) |
| [`ungroupCurrentSceneItemGroupById(SceneItemInfo, ResultCallback<success>)`](host/scene-items.md#ungroupcurrentsceneitemgroupbyidsceneiteminfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`ungroupSceneItemGroupById(SceneItemInfo, ResultCallback<success>)`](host/scene-items.md#ungroupsceneitemgroupbyidsceneiteminfo-resultcallbacksuccess) | [Scene items](host/scene-items.md) |
| [`writeScopedStorageJsonItem(ScopedStorageFileInfo, ResultCallback< ScopedStorageFileInfo \| null>)`](host/scoped-storage.md#writescopedstoragejsonitemscopedstoragefileinfo-resultcallback-scopedstoragefileinfo-null) | [Scoped Storage](host/scoped-storage.md) |

## Adding a new API call

The tree mirrors the shape of the API, so a new call touches one file:

1. Add it to the group file under `host/` covering its area — `host/scenes.md`,
   `host/docking-widgets.md`, and so on. Copy a neighbouring entry: a `##`
   heading holding the **full signature in backticks**, then an
   `Available since API version X` line, then the description.
2. If it takes or returns a new object type, add `types/<TypeName>.md` with a
   `Property | Type | Description` table.
3. Add a row to [revision-history.md](revision-history.md), newest last.
4. Add a row to the alphabetical index above.

A new *group* is a new file under `host/`, plus a line in the group list and in
[host/README.md](host/README.md).

Signatures are the headings, so keep them exact — the index links to them by
anchor, and a changed signature silently breaks that link.
