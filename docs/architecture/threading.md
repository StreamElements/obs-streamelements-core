# Threading and locks

Most of the hard bugs in this plug-in have been threading bugs, and almost all
of them were one of two shapes: work done while holding a lock that something
re-entrant wanted, or work done on the wrong thread at a moment when that
thread was no longer willing to serve it.

## The threads

| thread | what runs on it |
| --- | --- |
| **Qt main / UI thread** | all widget work, all host API handler bodies, `obs_frontend_*` calls, the modal `exec()` loops |
| **WebSocket server threads** | `SEWebsocketApiServer` (websocketpp) accepting and reading; an incoming API call is handed to the Qt thread |
| **libobs signal threads** | scene, scene-item, output, transition and hotkey callbacks, raised from whichever libobs thread caused them — often with libobs locks held |
| **detached one-shots** | `showNonModalDialog`'s waiting thread, and similar park-until-resolved work |
| **worker threads of specific services** | the bandwidth test client, the Razer WYVRN SDK thread, `SEAsyncTaskQueue` |

Two rules follow, and they are the ones to remember:

1. **Handler bodies run on the Qt thread.** They may touch widgets and
   `obs_frontend_*` directly. They must not block: the API call lock is held
   for the body's whole length — see
   [the host API bridge](host-api-bridge.md).
2. **A signal callback runs on someone else's thread, with someone else's
   locks held.** Take what you need under your own lock, release it, then act.
   `check_the_scene_manager_drains_outside_its_mutex` enforces exactly that
   for the scene manager.

## Getting onto the Qt thread

Three macros, from `SEUtils/SEAsyncTaskUtils.hpp` (or the `SEUtils.hpp`
umbrella):

| macro | semantics |
| --- | --- |
| `QtPostTask(task)` | queue it, return a `std::future<void>` |
| `QtExecSync(task)` | run it and wait; runs inline if already on the Qt thread |
| `QtDelayTask(task, ms)` | queue it after a delay |

**They are macros, not functions, and the difference is the point:** they
capture `__FILE__` / `__LINE__` into an async-call context stack that the crash
handler reads, so a crash inside a queued task reports where it was posted
from, not just where it ran. Never substitute raw
`QMetaObject::invokeMethod` — CLAUDE.md mandates the macros for this reason.

Their contracts have more branches than they look like, and the unhappy ones
are the incident-shaped ones:

- **`__QtPostTask_Impl`**: pushes and removes the async-call context; satisfies
  the promise **even if the task throws**, rethrowing afterwards; and skips the
  task but still satisfies the promise when `IsCrashReportingInProgress()`
  (CORE-862) — because a crash report in flight must not be perturbed by
  queued work, and a never-satisfied promise would hang whoever waited.
- **`__QtExecSync_Impl`**: runs inline when already on the Qt thread;
  otherwise, if `IsShuttingDown()`, it satisfies the promise and does not
  queue (CORE-1131) — after shutdown a cross-thread sync call can never be
  served, so waiting for it is waiting for ever.

A test fake that drops those branches encodes a weaker contract than
production has, which is why the suite's stand-in mirrors all three.

## Event pumping

Pumping the Qt event queue is how re-entrancy gets in, and the plug-in used to
do it in several places. Two gates now exist:

- `SEIsEventPumpAllowed()` is false for the whole of `Initialize()`, enforced
  by `StreamElementsInitializeScope` — see the CORE-786 trap in
  [Module lifecycle](module-lifecycle.md).
- `check_event_pumps_are_counted` keeps the number of pump sites countable, so
  a new one is a deliberate decision rather than a detail.

`check_consent_prompt_does_not_pump_foreign_messages` and
`check_consent_prompt_is_bounded` are the same concern on the crash path,
where pumping is least safe of all.

## Locks, and the two hazards that have actually bitten

The plug-in has no global lock hierarchy. What it has is a small number of
documented rules, each written where a violation hurt.

### 1. `std::shared_mutex` is not recursive — and on MSVC it is worse

MSVC backs `std::shared_mutex` with an SRWLOCK that Microsoft documents as
non-recursive, so recursive acquisition is undefined behaviour that parks for
ever in practice. Two forms have appeared:

- **A nested exclusive acquire through a libobs signal callback.** Holding
  `m_currentSceneMutex` across `obs_transition_start()` let libobs emit
  `transition_stop` synchronously on the same thread, and the handler for it
  takes a `shared_lock` on the same mutex. That was a hard hang of the OBS UI
  thread, reachable from any page. Full story in
  [Compositions and scenes](compositions.md).
- **Two shared acquires with a writer able to arrive between them.** A getter
  taking `shared_lock` and then calling a second getter that takes the same
  mutex works until a writer lands in the gap. The fix is a lock-free
  `*Internal()` called under the lock the outer method already holds.

Two invariants cover these: `check_no_shared_mutex_is_acquired_recursively`
for what a scanner can follow, and
`check_no_transition_call_runs_under_the_current_scene_lock` for what it
cannot — the recursion that leaves through a signal callback.

> **The transferable lesson:** a libobs signal is a call edge your static
> analysis does not have. If libobs can call you back from inside the function
> you are calling, you are re-entrant whether or not the code looks like it.

### 2. The API call lock is per translation unit, deliberately

`s_sync_api_call_mutex` is a `static std::recursive_mutex` defined *separately*
in each translation unit that expands `API_HANDLER_BEGIN`. Unifying them
deadlocks every modal dialog on its own exit path, because the handler that
opens one holds its TU's lock across `exec()` and the handler that closes it
runs on a different thread. `check_the_api_call_lock_stays_per_translation_unit`
exists to stop that "simplification". Details in
[the host API bridge](host-api-bridge.md).

### Other locks worth knowing

- `m_processMessageReceivedMutex` serialises `OnProcessMessageReceived`, so
  concurrent context-created messages cannot race handler registration.
- The composition and audio managers guard their collections with
  `std::shared_mutex` and expose `*Internal()` variants for use under the lock.
- `SEBrowserDialogManager` guards its dialog map, and its destructor swaps the
  map out under that lock rather than iterating it.
- `SEReentrancyUtils` is the small helper for "do not re-enter this".

## Shutdown is a threading problem too

Once `Shutdown()` begins, `ready` is false, cross-thread `QtExecSync` stops
being served, and widgets start disappearing. Anything that queues work must
assume it may never run; anything that waits must assume it may never be
woken. The weak-edge rule in [Services and injection](services.md) is what
makes "may never run" safe rather than a dangling dereference.

## Related

- [Compositions and scenes](compositions.md) — the worst instance of hazard 1.
- [The host API bridge](host-api-bridge.md) — hazard 2, and which thread a
  handler body is on.
- [Diagnostics](diagnostics.md) — how a hang is told apart from a modal window.
