# UI and widgets

Everything the user sees that is not OBS's own chrome comes from here: docks,
the central widget, the notification bar, pop-up windows, modal and non-modal
dialogs, menu entries, and the decorations on OBS's own Start Streaming
button. Nearly all of them are a web page in a CEF widget.

## The widget hierarchy

<figure class="se-fig">
<div class="se-fig-scroll">
<svg viewBox="0 0 880 430" role="img" aria-label="The widget family. SEObsAppMonitor is the base; SEWidgetManager derives from it and handles widgets, docks and the central widget; SEBrowserWidgetManager inherits that plumbing protected and implements ISEBrowserWidgetManager. Eight sibling managers, each published behind its own ISE contract, cover off-screen workers, modal and non-modal dialogs, pop-up windows, the SE.Live menu, context menus and auxiliary buttons, OBS's own UI controls, status-bar messages and state persistence. SEBrowserWidget, one CEF panel borrowed from the sibling obs-browser plug-in, is created by the browser widget manager, and the worker, dialog and pop-up managers each wrap one too.">
<title>The widget managers, their contracts, and the panel they all wrap</title>
<text class="se-cap" x="24" y="34">QT PLUMBING</text>
<rect class="se-box" x="24" y="46" width="260" height="52" rx="6"/>
<text class="se-id" x="44" y="70">SEObsAppMonitor</text>
<text class="se-sub" x="44" y="88">OBS app lifecycle hooks</text>
<path class="se-edge" d="M 154,98 V 119"/>
<path class="se-head" d="M 154,128 L 148.5,119 L 159.5,119 Z"/>
<rect class="se-box" x="24" y="128" width="260" height="52" rx="6"/>
<text class="se-id" x="44" y="152">SEWidgetManager</text>
<text class="se-sub" x="44" y="170">widgets, docks, central widget</text>
<path class="se-edge-em" d="M 154,180 V 201"/>
<path class="se-head-em" d="M 154,210 L 148.5,201 L 159.5,201 Z"/>
<text class="se-note-em" x="166" y="198">protected</text>
<rect class="se-box" x="24" y="210" width="260" height="66" rx="6"/>
<text class="se-id" x="44" y="234">SEBrowserWidgetManager</text>
<text class="se-sub" x="44" y="252">public ISEBrowserWidgetManager</text>
<text class="se-sub" x="44" y="268">the same, for browser widgets</text>
<rect class="se-frame" x="316" y="18" width="540" height="298" rx="6"/>
<text class="se-cap" x="332" y="42">SIBLING MANAGERS</text>
<text class="se-sub" x="332" y="60">each published behind its own ISE&lt;Class&gt; contract</text>
<rect class="se-box" x="332" y="76" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="346" y="96">SEWorkerManager</text>
<text class="se-sub" x="346" y="114">off-screen browser widgets</text>
<rect class="se-box" x="594" y="76" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="608" y="96">SEBrowserDialogManager</text>
<text class="se-sub" x="608" y="114">modal + non-modal dialogs</text>
<rect class="se-box" x="332" y="134" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="346" y="154">SEPopupWindowManager</text>
<text class="se-sub" x="346" y="172">separate top-level windows</text>
<rect class="se-box" x="594" y="134" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="608" y="154">SEMenuManager</text>
<text class="se-sub" x="608" y="172">the SE.Live menu</text>
<rect class="se-box" x="332" y="192" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="346" y="212">SEAuxiliaryControlsBuilder</text>
<text class="se-sub" x="346" y="230">context menus, aux buttons</text>
<rect class="se-box" x="594" y="192" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="608" y="212">SENativeObsUIControlsManager</text>
<text class="se-sub" x="608" y="230">OBS's own controls</text>
<rect class="se-box" x="332" y="250" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="346" y="270">SEStatusBarMessageManager</text>
<text class="se-sub" x="346" y="288">temporary status-bar text</text>
<rect class="se-box" x="594" y="250" width="246" height="48" rx="6"/>
<text class="se-id-sm" x="608" y="270">SEStatePersistence</text>
<text class="se-sub" x="608" y="288">what survives a restart</text>
<path class="se-edge" d="M 154,276 V 343 M 586,316 V 343"/>
<path class="se-head" d="M 154,352 L 148.5,343 L 159.5,343 Z M 586,352 L 580.5,343 L 591.5,343 Z"/>
<text class="se-note" x="166" y="316">creates</text>
<text class="se-note" x="598" y="338">workers, dialogs and pop-ups wrap one too</text>
<rect class="se-box" x="24" y="352" width="832" height="58" rx="6"/>
<text class="se-id" x="440" y="376" text-anchor="middle">SEBrowserWidget</text>
<text class="se-sub" x="440" y="394" text-anchor="middle">one CEF panel, borrowed from the sibling obs-browser plug-in</text>
</svg>
</div>
<figcaption>Eleven classes and one panel. Every manager in the frame is reached through its interface rather than its class, which is what lets a handler body compile in a test binary with no Qt at all; the four that put a page on screen all create the same <code>SEBrowserWidget</code>.</figcaption>
</figure>

The inheritance is `protected`: the dock plumbing is an implementation detail
of the browser widget manager, not part of its contract. The seven methods it
inherits are redeclared with `override` on the derived class, so the interface
needs no forwarding wrappers.

`SEScenesListWidgetManager`, `SEVideoCompositionViewWidget`, `SEPreviewManager`
and the `.ui`-backed dialogs (`SENetworkDialog`, `SEProgressDialog`,
`SEReportIssueDialog`, `SEPleaseWaitWindow`) are the non-browser Qt surface.

## Browser widgets

`SEBrowserWidget` wraps a CEF panel borrowed from the sibling `obs-browser`
plug-in — it includes `../../obs-browser/panel/browser-panel.hpp` **by relative
path**, which is why this plug-in only builds inside an obs-studio tree with
that sibling present.

Each widget creates an `SEApiMessageHandler` so its page gets `window.host`.
The widget holds the service container **weakly**; the handler does too. That
is not a style choice — the widget is destroyed by the dock, the dock by the
widget manager, and the widget manager lives in the container, so a strong
edge closes a ring that refcounting cannot unwind. See
[Services and injection](services.md).

A widget that is not incognito opens on the shared `QCefCookieManager`; both
it and `QCef` are borrowed from the root, which deletes them in `Shutdown()`.

**Background workers** are browser widgets the user never sees, and the way
that is achieved is a documented hack worth knowing before you "fix" it:
`QCefWidget` only creates its browser **if the widget is visible**, so the
worker manager wraps it in a `QDockWidget`, marks the dock visible, and then
sets its geometry to twice the primary screen's width and height — off the
desktop entirely. Pages use workers for work that must keep running when no
dock is open.

## Docks, and the teardown rules

`TrackedDockWidget`, a **local class** inside `SEWidgetManager::AddDockWidget`,
is the dock type. Being local, it cannot capture the enclosing `this` — so
anything it needs (the host event dispatcher, for the
`hostUserInterfaceStateChanged` advisory it raises from `resizeEvent` and
`moveEvent`) is passed in explicitly.

Four invariants govern dock and widget teardown, and every one of them exists
because of a crash:

| invariant | rule |
| --- | --- |
| `check_widget_maps_hold_qpointer` | widget maps hold `QPointer`, so a widget Qt deleted is observably gone rather than a dangling pointer |
| `check_dock_deletion_is_gated` | a dock is only deleted when UI teardown is safe — see the CORE-786 trap in [Module lifecycle](module-lifecycle.md) |
| `check_widget_manager_dtor_does_not_drain_events` | the destructor does not pump the event queue |
| `check_widget_teardown_leaves_the_paint_tree` | teardown leaves the paint tree in a state Qt can still destroy |
| `check_no_dock_is_added_to_an_invalid_area` | a dock area is validated before `addDockWidget` |

`check_deferred_deletes_hold_services_weakly` covers the other half: a
`deleteLater` that outlives the services it captured.

## Dialogs

`SEBrowserDialogManager` owns both kinds, and the design is about **promises**:

- `showNonModalDialog` parks a `std::promise` and resolves it when the dialog
  closes, so the page's call completes with the dialog's result. The map's
  value therefore holds the promise as well as the dialog; keeping it only in
  the `finished` lambda's capture meant a dialog destroyed without emitting
  `finished` left the waiting thread parked for ever.
- The destructor is the point of the class: disarm the finished handlers
  (through a `shared_ptr<bool> m_alive` captured weakly, because a queued
  reject can land after the manager is gone), swap the map out under its lock,
  then for each entry reject the dialog and satisfy its promise.
  `check_non_modal_dialogs_are_closed_by_their_owner` requires both `reject()`
  and `set_value` in that destructor body. Keep the disarm and the swap
  adjacent: `set_value` on an already-satisfied promise throws
  `std::future_error`, which is why that string is one of the things a
  shutdown log is grepped for.
- **An invariant worth knowing about before you "fix" a refusal path:** every
  promise the manager returns is either stored against a live dialog or
  already satisfied. `DeserializeNonModalDialog` satisfies it itself in its
  else branch, so a description it refuses resolves with null. Adding an early
  return above that branch reintroduces the parked-thread leak.
- Modal dialogs need no teardown handling: `exec()` is on the stack while one
  is open, and a modal SE.Live window blocks `CloseMainWindow()` outright, so
  one cannot be open when the destructor runs. It is also why the API call
  lock is per translation unit — see
  [the host API bridge](host-api-bridge.md).

## Menus and auxiliary controls

`SEMenuManager` owns the SE.Live menu; `SEAuxiliaryControlsBuilder` builds
context menus and auxiliary buttons from a page-supplied description and
invokes the actions. `check_menu_manager_update_guarded` guards `Update()`,
which was once called on a null manager during construction.

`showPopupMenuAtMousePointerPosition` calls `QMenu::exec()`, which blocks until
a human dismisses the menu. That makes it the one host API method a CDP harness
must not call — it would hang OBS — and therefore the one path through
`DeserializeDocksMenu` with no live coverage. Its contract-level behaviour is
tested with a fake instead.

## OBS's own controls

`SENativeObsUIControlsManager` is about *OBS's* UI, not SE.Live's: find, click
and style the native Start/Stop Streaming button and track its six states;
show or hide the manage-broadcast button; the preview frame and preview title
bar and their hotkeys; and the start-streaming handover protocol
(`SetStartStreamingMode`, `AdviseRequestStartStreamingAccepted`/`Rejected`,
an acknowledge-timeout tracker).

Its `StartStreaming()` does not start a stream — it drives the native button
through that handover. The preview decorations sit behind
`SE_ENABLE_CENTRAL_WIDGET_DECORATIONS`, which is `0`; the switch is defined on
`ISENativeObsUIControlsManager.hpp` so that all three guarded regions see the
same name. (An undefined name in an `#if` evaluates to `0`, so a guard whose
define has drifted out of reach compiles green and silently stops meaning
anything — which is exactly what nearly happened here.)

## State persistence

`SEStatePersistence` serialises what must survive a restart: docks, workers,
the central widget, UI state. It **borrows** the four managers it serialises as
raw pointers rather than owning them
(`check_state_persistence_borrows_the_managers_it_serializes`), and the widget
manager holds *it* weakly
(`check_widget_manager_does_not_own_state_persistence`). Both for the same
reason: container fields are destroyed in reverse declaration order and
`statePersistence` is declared last, so it dies first while the managers it
borrows are still alive. A strong handle in the widget manager inverts that and
leaves four dangling raw pointers.

**There are four different rules about when the layout gets persisted**, and
that is a finding rather than a design: sixteen dock-related handler bodies end
in "rebuild the menu, then persist", but `addDockingWidget` gates the save on
the dock existing afterwards, `removeDockingWidgetsByIds` persists
unconditionally, eight gate on the widget manager's return, and the two preview
`show*` handlers save whether or not the deserialize succeeded. The order
matters too: menu before persist, because the menu is rebuilt from live widgets
and the persisted state is what a relaunch reads.

## Reaching the UI thread

Always through the macros from `SEUtils/SEAsyncTaskUtils.hpp` —
`QtPostTask`, `QtExecSync`, `QtDelayTask` — never raw
`QMetaObject::invokeMethod`. They are macros, not functions: they capture
`__FILE__`/`__LINE__` into an async-call context the crash handler reads. See
[Threading and locks](threading.md).

## Related

- [Module lifecycle](module-lifecycle.md) — why dock deletion is gated.
- [Services and injection](services.md) — the weak-edge rule these classes obey.
- [Threading and locks](threading.md) — the posting macros and their contract.
