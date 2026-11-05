# Module lifecycle

`obs-streamelements-core-plugin.cpp` is the whole of the plug-in's boundary
with OBS: four module entry points, one frontend event callback, and the one
`std::shared_ptr` that owns everything else.

## The four entry points

| entry point | what it does |
| --- | --- |
| `obs_module_load` | logs the version and registers one source type, `audio_wrapper_source`. **Nothing else.** No object graph, no UI, no services. |
| `obs_module_post_load` | installs the OBS close watcher (below) and registers the frontend event callback. The earliest point at which the main window may exist. |
| `handle_obs_frontend_event` | the real lifecycle: `FINISHED_LOADING` builds everything, `EXIT` / `SCRIPTING_SHUTDOWN` tears it down. |
| `obs_module_unload` | logs remaining objects, prints `shutdown complete`, and runs `SETRACE_DUMP()`. |

That last line is why a measured run has to end in a *clean* shutdown: the
reference-accounting dump and the leak log only exist if `obs_module_unload`
runs. Killing the process throws away the result. See
[Diagnostics](diagnostics.md).

`OBS_DECLARE_MODULE()` and
`OBS_MODULE_USE_DEFAULT_LOCALE("obs-streamelements-core", "en-US")` sit at the
top of the same file; user-visible strings go through `obs_module_text()` with
keys in `data/locale/en-US.ini`.

## Start-up

<figure class="se-fig">
<div class="se-fig-scroll">
<svg viewBox="0 0 880 650" role="img" aria-label="Start-up in four steps. obs_module_load registers audio_wrapper_source and nothing else. obs_module_post_load installs the OBS close watcher and the frontend event callback. OBS then loads its own UI. On OBS_FRONTEND_EVENT_FINISHED_LOADING, inside a StreamElementsInitializeScope that suppresses event pumping, SEMain is created and initialised: SEConfig first because everything may read it, SECrashHandler second so a crash in the rest is reportable, then the empty SEApiServices container, then about 39 services in dependency order, PublishApiServices, WatchServicesForTeardownAudit, then apiServices ready is set to true which is the dispatch gate, then docks, background workers and the central widget, then SENoteInitializeCompleted. Finally init done is logged.">
<title>The start-up sequence, and where the dispatch gate opens</title>
<path class="se-edge" d="M 54,20 V 142 M 54,178 V 618"/>
<path class="se-frame" d="M 54,142 V 178"/>
<path class="se-head" d="M 54,88 L 48.5,79 L 59.5,79 Z M 54,174 L 48.5,165 L 59.5,165 Z"/>
<circle class="se-head" cx="54" cy="45" r="5"/>
<circle class="se-head" cx="54" cy="117" r="5"/>
<circle class="se-head" cx="54" cy="196" r="5"/>
<circle class="se-head" cx="54" cy="618" r="5"/>
<rect class="se-box" x="78" y="20" width="460" height="50" rx="6"/>
<text class="se-id" x="98" y="42">obs_module_load</text>
<text class="se-sub" x="98" y="60">registers audio_wrapper_source, and nothing else</text>
<rect class="se-box" x="78" y="92" width="460" height="50" rx="6"/>
<text class="se-id" x="98" y="114">obs_module_post_load</text>
<text class="se-sub" x="98" y="132">InstallObsCloseWatcher(); add_event_callback</text>
<text class="se-note" x="78" y="166">&hellip; OBS loads its own UI &hellip;</text>
<rect class="se-box" x="78" y="178" width="460" height="36" rx="6"/>
<text class="se-id" x="98" y="201">OBS_FRONTEND_EVENT_FINISHED_LOADING</text>
<rect class="se-frame" x="78" y="226" width="778" height="360" rx="6"/>
<text class="se-id" x="100" y="252">StreamElementsInitializeScope</text>
<text class="se-sub" x="100" y="270">suppresses event pumping for everything below</text>
<path class="se-frame" d="M 112,344 V 574"/>
<text class="se-id" x="100" y="306">SEMain::Create()</text>
<text class="se-id" x="100" y="332">SEMain::Initialize(main window)</text>
<text class="se-id" x="124" y="358">SEConfig</text>
<text class="se-note" x="380" y="358">first &mdash; everything below may read it</text>
<text class="se-id" x="124" y="384">SECrashHandler</text>
<text class="se-note" x="380" y="384">second &mdash; so a crash in the rest is reportable</text>
<text class="se-id" x="124" y="410">SEApiServices</text>
<text class="se-note" x="380" y="410">the container, still empty</text>
<text class="se-note" x="124" y="436">&hellip; ~39 services, in dependency order &hellip;</text>
<text class="se-id" x="124" y="462">PublishApiServices()</text>
<text class="se-note" x="380" y="462">fills the container</text>
<text class="se-id" x="124" y="488">WatchServicesForTeardownAudit()</text>
<rect class="se-box-em" x="108" y="498" width="740" height="26" rx="4"/>
<text class="se-id" x="124" y="514">apiServices-&gt;ready = true</text>
<text class="se-note-em" x="380" y="514">the dispatch gate opens here</text>
<text class="se-note" x="124" y="540">&hellip; docks, background workers, central widget &hellip;</text>
<text class="se-id" x="124" y="566">SENoteInitializeCompleted()</text>
<text class="se-id" x="78" y="616">"init done"</text>
<text class="se-sub" x="78" y="634">logged last, so the log says whether any of this ran</text>
</svg>
</div>
<figcaption>Time runs down the spine, and the dashed gap is OBS building its own UI. Everything in the panel happens inside one <code>StreamElementsInitializeScope</code>, with event pumping suppressed; the accented line is the dispatch gate, and the three orderings below say why it and the two constructions above it sit exactly where they do.</figcaption>
</figure>

Three orderings in that list are load-bearing and each is pinned by a test.

**`SEConfig` first.** Every service may read configuration, and the config is
also the one thing `Shutdown()` closes explicitly and last
(`CloseAndPreventFurtherReopens()`), after which it answers defaults instead
of crashing.

**`SECrashHandler` second.** It is constructed with the config and before the
rest of the graph, so a crash anywhere in the remaining construction is
reportable. It is also *deliberately leaked* — it keeps `config`,
`performanceHistoryTracker` and `analyticsEventsManager` alive for the life of
the process, which is why the teardown audit reports
`36 of 39 services released cleanly, 3 held by the crash handler` and that is
a pass, not a leak.

**`ready = true` above the first browser widget.** This is the dispatch gate:
an API message handler answers nothing until it is set. Every client of the
WebSocket API server is one of our own browser widgets, so setting the flag
*before the first one exists* makes it impossible for a page to find it
false — and a page that does find it false gets no `window.host` for the life
of its context, because the context-created message arrives once and nothing
retries. `check_the_dispatch_gate_opens_before_the_first_browser_widget` pins
the order; the long comment on `SEApiServices::ready` is the full argument.

## Shutdown, and the path that is a trap

`EXIT` normally means: drop the root `shared_ptr`, which runs `~SEMain()` →
`Shutdown()` → member destruction → the container is released → the teardown
audit reports. `Shutdown()` is private and only the destructor calls it
(`check_only_the_destructor_starts_teardown`), so there is exactly one teardown
path.

The trap is **CORE-786**: OBS's own update checker is started *before* OBS
tells the plug-in the UI is ready, and it reaches back onto the main thread
twice — once to run the update dialog's modal `exec()`, once to queue
`close()` on the main window. Either can be delivered by an event pump running
*inside* `Initialize()`, so `closeEvent()` → `OBS_FRONTEND_EVENT_EXIT` runs
nested inside the construction it is trying to tear down. Every crash chased
under that issue was a destructor touching something `Initialize()` had just
created.

Two defences, and they are different in kind:

- **No event pumping while `Initialize()` is on the stack.**
  `StreamElementsInitializeScope` turns it off for the whole of construction,
  because gating the pump on a flag could not work: the flag is only set while
  the close is being delivered, by the very pump delivering it.
  `check_event_pumps_are_counted` keeps the pump sites countable.
- **A close that beats `init done` poisons UI teardown.** An event filter on
  the OBS main window watches `QEvent::Close` — filters run before the
  widget's own `closeEvent()`, which is what fires `EXIT`. If the close wins,
  `SEIsUiTeardownSafe()` answers false and the plug-in **leaks its entire
  state on purpose**, by heap-allocating another `shared_ptr` to the root so
  no destructor ever runs:

  > Running the destructors is what corrupts the widget tree, so do not run
  > them at all. The process is exiting and about to be replaced by the
  > updater; a leak is strictly better than a crash.

  The flag is deliberately narrow — a close *after* `Initialize()` completed is
  an ordinary shutdown and must behave exactly as it always has, or every
  normal exit would leak. `check_obs_close_is_watched` and
  `check_dock_deletion_is_gated` pin both halves.

`EXIT` also does **not** deregister the frontend event callback when teardown
is unsafe. `OBSStudioAPI::on_event()` iterates its callback vector with the
size captured once, and removing from it during a nested dispatch shrinks the
vector under the outer loop, which then indexes past the end and calls through
garbage. An `isRunning` flag makes any further event a no-op instead.

## What a clean shutdown looks like

```
==== Shutting down ==================================================
[obs-streamelements-core]: shutting down
[obs-streamelements-core]: config is still held after teardown, as expected: …
[obs-streamelements-core]: performanceHistoryTracker is still held after teardown, as expected: …
[obs-streamelements-core]: analyticsEventsManager is still held after teardown, as expected: …
[obs-streamelements-core]: 36 of 39 services released cleanly, 3 held by the crash handler
[obs-streamelements-core]: shutdown complete
[obs-streamelements-core]: reference count balance = 0 (0 is good)
```

Reading that: the three held services are the crash handler's, by design; a
*fourth* name appearing there is a real finding. `reference count balance`
counts unpaired `SETRACE_ADDREF`s, not leaked bytes — see
[Diagnostics](diagnostics.md) for what it does and does not prove. libobs
prints its own `Number of memory leaks` line afterwards, which is a host
baseline (9–12 on OBS 33.0.1) and not the plug-in's.

**A process that reports `Responding: True` with no shutdown lines is usually
not a hang in teardown.** `CloseMainWindow()` does nothing while a modal
window is up, because the modal disables the main window and Windows ignores
`WM_CLOSE` on a disabled window. A real hang has the teardown lines and no
`shutdown complete`. See [Testing](testing.md) for the harness that gets this
right.

## Related

- [Services and injection](services.md) — what `Initialize()` builds, and the
  ownership rule that makes `Shutdown()` terminate.
- [Threading and locks](threading.md) — why event pumping is dangerous at all.
- [Enforced invariants](invariants.md) — the lifecycle rules, with the names of
  the checks that hold them.
