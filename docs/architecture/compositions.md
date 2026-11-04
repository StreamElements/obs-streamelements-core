# Compositions and scenes

OBS has one video canvas. SE.Live adds more, each with its own scenes,
transition, encoders and outputs — "canvases" to a user, *compositions* in the
code. Most of the subtlety in the plug-in lives here, because every custom
composition has to reimplement, against libobs primitives, what OBS's frontend
does for its own.

## The two kinds, and why the difference matters

| | native | custom |
| --- | --- | --- |
| class | `SEObsNativeVideoComposition` | `SECustomVideoComposition` |
| is | OBS's own canvas, wrapped | a composition the plug-in owns |
| scene change | `obs_frontend_set_current_scene()` — no lock, no second call | swap under a lock, then drive the transition itself |
| transition | **selects** one OBS already has configured | **creates** a private one from `class` + `settings` |
| audio | `SEObsNativeAudioComposition` | `SECustomAudioComposition` |

Both derive from `SEVideoCompositionBase`, which implements
`ISEVideoComposition` — the second-level contract a handler body reaches after
`GetVideoCompositionById()` or `GetObsNativeVideoComposition()`:
`TakeScreenshot`, `SerializeTransition`, `DeserializeTransition`,
`ShowTransitionPropertiesDialog`. `SEVideoCompositionEventListener` is the
observer interface for composition events.

`SEVideoCompositionManager` owns the set and answers the host API;
`SESharedVideoCompositionManager` handles compositions shared with other
processes; `SEAudioCompositionManager` is the audio counterpart.
`SEObsSceneManager` is the scene and scene-item surface — the single largest
contract in the plug-in. `SEObsActiveSceneTracker` watches the **main canvas's
transition**: it hooks OBS's frontend events plus `transition_start` and
`transition_changed`, holds the current transition source, and announces
changes through the message bus and the API server.

### The transition asymmetry is page-visible

`DeserializeObsTransition()` takes a `useExisting` flag, which is
`IsObsNativeComposition()`:

- **native**: `GetExistingObsTransition(id)` walks
  `obs_frontend_get_transitions()` and matches **by obs source id**. If the
  user has not configured a transition of that class, the call *fails* — it
  does not create one. So on a profile whose scene collection has
  `"transitions": []`, setting the main canvas to `obs_stinger_transition`
  correctly answers null, and the previous transition stays in place.
- **custom**: `obs_source_create_private(id, id, settings)` — the composition
  owns a private transition that OBS's UI knows nothing about.

That asymmetry is not a bug and it is the right behaviour on both sides: a
private transition on the main canvas would be invisible to the OBS transition
dropdown. It is also the single most common way a test harness reports a
defect that is not there.

## Scene changes, transitions, and the lock that must not be held

This is the part to read before touching `SECustomVideoComposition`.

`m_currentSceneMutex` guards `m_currentScene`. The rule is:

> **Swap under the lock. Call libobs outside it.**

Because `obs_transition_set()` emits `transition_stop`
**synchronously, on the calling thread**, whenever a transition is already
active (libobs `obs-source-transition.c`), and the plug-in's own
`handle_transition_stop()` calls `GetCurrentSceneRef()`, which takes a
`shared_lock` on that same mutex. MSVC backs `std::shared_mutex` with an
SRWLOCK that Microsoft documents as **non-recursive**, so the nested acquire
never returns: the OBS UI thread parks, the process stops answering
`WM_CLOSE`, and there is no shutdown, no `obs_module_unload` and no trace dump.

It used to hold the lock across `obs_transition_start()`, and the result was a
hard hang reachable from any page with `setCurrentSceneById` — or from a human
clicking a scene in a canvas dock. The stinger made it *certain* rather than
occasional, because that branch starts the transition twice on purpose (a
documented workaround for the stinger ignoring the first call), so the second
call always finds one active. **Any** transition reaches the same place given
two scene changes inside one transition's duration, which is why the fix was
the lock scope and not the stinger branch.

`check_no_transition_call_runs_under_the_current_scene_lock` bans a libobs
transition call inside any `m_currentSceneMutex` scope.
`check_no_shared_mutex_is_acquired_recursively` cannot see this case, because
the recursion leaves through a libobs signal callback rather than a call a
scanner can follow — which is the general lesson: **a signal callback is a
call edge your static analysis does not have.**

Swapping first has a second benefit worth keeping: a re-entrant handler sees
the scene being switched *to*, which is what an active-scene-changed event
should name.

The same shape appeared twice more as a latent hazard — a `shared_lock`
re-acquired through a second getter in `GetVideoCompositionById()` and
`GetAudioCompositionById()`. Two shared acquisitions only deadlock when a
writer arrives between them, so it works until it does not; the fix is a
lock-free `*Internal()` called under the lock the outer method already holds,
which is the shape `GetAllScenes`/`GetAllScenesInternal` already uses.

## Scene signal handlers

Scene and scene-item events arrive as libobs signals, which are file-static
callbacks with a `void *data` context. `SESignalHandlerData` is that context,
and three invariants hold its lifetime together:

- `check_scene_signal_handler_lifetime` — the data outlives the connection.
- `check_signal_handler_data_holds_its_owners_weakly` — it does not keep its
  owners alive.
- `check_signal_handler_data_holds_the_composition_per_path` — the composition
  is carried per signal path rather than assumed.
- `check_the_scene_manager_is_attached_where_it_is_built` — the manager is
  connected at the point it is constructed, not later.

And one about doing work in them:
`check_the_scene_manager_drains_outside_its_mutex`. A signal handler runs on
whichever thread libobs raised it from, frequently while libobs holds locks of
its own, so the pattern is: take what you need under your lock, release, then
act.

## Related

- [Threading and locks](threading.md) — the general rules this page is the
  worst-case instance of.
- [Outputs and encoders](outputs.md) — what a composition feeds.
- The API reference's
  [scenes](https://streamelements.github.io/obs-streamelements-core/api/host/scenes/)
  and
  [shared video compositions](https://streamelements.github.io/obs-streamelements-core/api/host/shared-video-compositions/)
  groups — the page-facing surface.
