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

```
obs_module_load            register audio_wrapper_source
obs_module_post_load       InstallObsCloseWatcher(); add_event_callback
   …OBS loads its own UI…
OBS_FRONTEND_EVENT_FINISHED_LOADING
   StreamElementsInitializeScope            ← suppresses event pumping
      SEGlobalStateManager::Create()
      SEGlobalStateManager::Initialize(main window)
         SEConfig                           ← first, everything may read it
         SECrashHandler                     ← second, so a crash in the rest is reportable
         SEApiServices (empty container)
         …~39 services, in dependency order…
         PublishApiServices()               ← fills the container
         WatchServicesForTeardownAudit()
         apiServices->ready = true          ← the dispatch gate
         …docks, background workers, central widget…
         SENoteInitializeCompleted()
   "init done"
```

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

`EXIT` normally means: drop the root `shared_ptr`, which runs
`~SEGlobalStateManager()` → `Shutdown()` → member destruction → the container
is released → the teardown audit reports. `Shutdown()` is private and only the
destructor calls it (`check_only_the_destructor_starts_teardown`), so there is
exactly one teardown path.

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
