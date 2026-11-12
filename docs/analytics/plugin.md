# Plug-in events

Eight event names, `feature: obs_selive_core_plugin`. Every one goes through
`SEAnalyticsEventsManager::AddRawEvent`, so each carries the whole
[envelope](envelope.md) plus whatever it adds below.

| event | fires when | always sent? |
| --- | --- | --- |
| [`se_live_initialized`](#se_live_initialized) | the plug-in finished initialising | yes, once per OBS run |
| [`se_live_dock_show_click`](#se_live_dock_show_click-se_live_dock_hide_click) | a dock was shown from the docks menu | yes |
| [`se_live_dock_hide_click`](#se_live_dock_show_click-se_live_dock_hide_click) | a dock was hidden from the docks menu | yes |
| [`live_support_click`](#live_support_click) | the Live Support button was clicked | yes |
| [`se_live_report_issue`](#se_live_report_issue) | an issue report was submitted | yes |
| [`se_live_razer_wyvrn_initialized`](#the-two-wyvrn-events) | the WYVRN SDK initialised | Windows builds only |
| [`se_live_razer_wyvrn_init_failed`](#the-two-wyvrn-events) | it ran and refused | Windows builds, one status only |
| [`OBS Studio Crashed`](#obs-studio-crashed) | the crash handler caught a crash | macOS, bugsplat builds only |

Two of those names break the `se_live_*` convention the other six follow;
see [Reading the data](reading-the-data.md#two-names-break-the-convention).

## `se_live_initialized`

`SEMain.cpp:852`, at the very end of `Initialize()` — after the `ready`
gate opens, after state is restored or onboarding is reset, after the event
queue is drained and the menu rebuilt.

| adds | where | value |
| --- | --- | --- |
| `is_onboarding` | field | `"true"` or `"false"` |
| `onboarding_reason` | field | present **only** when onboarding |

The four reasons, in the order they are tested:

| `onboarding_reason` | means |
| --- | --- |
| `Failed creating new cookie storage folder` | the cookie path could not be created |
| `New cookie storage folder` | a fresh cookie storage path, i.e. a first run |
| `State saved by other version of the plug-in` | the stored config version differs from this build |
| `Start-up flags indicate on-boarding mode` | `STARTUP_FLAGS_ONBOARDING_MODE` was set, e.g. by a sign-out |

**It counts successful initialisations, not launches.** Because it is the
last statement of `Initialize()`, a start-up that fails earlier reports
nothing at all — so this event is not a reliable denominator for "OBS was
started with SE.Live installed". Nothing else reports a launch either; see
[Reading the data](reading-the-data.md#what-no-event-answers).

## `se_live_dock_show_click` / `se_live_dock_hide_click`

`SEAuxiliaryControlsBuilder.cpp:253`, from the docks menu item, before the
visibility is actually changed. Which of the two names is used is the
dock's state *before* the click, so `..._hide_click` means it was visible
and is being hidden.

| adds | where | value |
| --- | --- | --- |
| `type` | prop | `button_click` |
| `placement` | prop | `menu` — **overrides** the default `obs` |
| `dock_widget_title` | field | the dock's live `windowTitle()` |

`dock_widget_title` is the Qt window title as it stands at click time, not
the stored title from the dock's persisted info — the two can differ. It is
also user-visible text, which [Identity](identity.md#what-else-is-personal)
notes as one of the two payloads carrying something a user or a page chose.

Only the **docks menu** path reports. Closing a dock with its own close
button, or toggling it from OBS's own Docks menu, reports nothing.

## `live_support_click`

`SEMain.cpp:681`, from the Live Support button the plug-in adds to OBS's own
controls dock, fired before the support URL is opened.

| adds | where | value |
| --- | --- | --- |
| `type` | prop | `button_click` |
| `placement` | prop | `live_support_button` — **overrides** `obs` |

The one plug-in event with no `se_live_` prefix.

## `se_live_report_issue`

`SEReportIssueDialog.cpp:734`, after the report archive has been built and
only if the dialog was not cancelled.

| adds | where | value |
| --- | --- | --- |
| `message` | prop | the user's own description text, verbatim |
| `placement` | prop | `issue_report_dialog` — **overrides** `obs` |

`message` is free text typed by the user. It is the most sensitive value in
the whole catalogue.

## The two WYVRN events

`SERazerWyvrnManager::TrackInitOutcome`, called from the SDK thread seconds
after start-up. These exist because the outcome of Razer WYVRN
initialisation is otherwise invisible: it lands on a page that may not be
loaded yet and in a `LOG_INFO` line on the user's own disk, and neither
answers how many users have Razer software at all.

| event | `SERazerWyvrnStatus` | call site |
| --- | --- | --- |
| `se_live_razer_wyvrn_initialized` | `Ok` | `SERazerWyvrnManager.cpp:425` |
| `se_live_razer_wyvrn_init_failed` | `InitFailed` | `SERazerWyvrnManager.cpp:469` |

Both carry three fields and no extra props:

| field | value |
| --- | --- |
| `wyvrn_status` | the status string — `ok` or `initFailed` |
| `wyvrn_rzresult` | the `CoreInitSDK` result as `0x%08lX` |
| `wyvrn_init_duration_ms` | whole milliseconds, `%.0f` |

`wyvrn_status` comes from the same `SERazerWyvrnStatusToString` the host API
and the log line use, so a support log and a dashboard row meet on one
value. The full vocabulary of that function is `notCompiledIn`,
`notSupportedOnPlatform`, `initializing`, `dllNotFound`,
`dllInvalidSignature`, `initFailed`, `ok` and `shuttingDown` — but only two
of those eight ever reach analytics, because only two outcomes are
reported.

**These two do not add up to the number of sessions, by design.** Two
outcomes are deliberately silent, because neither has a `CoreInitSDK`
result to report: a machine with no usable Razer SDK (`dllNotFound`, which
also covers `InitAPI` never loading the API layer, and
`dllInvalidSignature`), and a shutdown that arrived while the call was still
running. The first of those is the overwhelming majority of machines, and
reporting it beside `initFailed` would bury the few genuinely broken
installations. So the denominator here is *machines that have the Razer SDK*,
not machines that run SE.Live.

`check_wyvrn_init_reports_its_outcome_to_analytics` pins both event names,
the field set and both silences, so this contract fails the test suite
rather than drifting.

The whole subsystem is Windows-only — CMake adds
`SERazerWyvrnManager.cpp` to the sources only when
`STREAMELEMENTS_ENABLE_WYVRN` is on, and forces it off everywhere else — so
neither event can occur from a macOS build.

## `OBS Studio Crashed`

`SEBugSplatCrashHandler.mm:131`, from `TrackCrash`, using
`trackSynchronousEvent` rather than `trackEvent`: the queue's worker threads
will not get another chance to run, so this one posts on the calling thread.

| adds | where | value |
| --- | --- | --- |
| `platformCallerReference` | prop | which handler caught it |

**macOS only, and only in a bugsplat-configured build.** The code is in the
Objective-C++ file, so no Windows build sends it, and a
`-DSTREAMELEMENTS_CRASH_HANDLER=sentry` macOS build does not compile that
file at all. The Windows BugSplat handler and the Sentry handler send no
analytics event on a crash.

So this is **not** a crash count. It is a partial count from one platform
and one backend, and a crash rate computed from it against
`se_live_initialized` would be wrong by the whole Windows population. The
real crash data is in Sentry or BugSplat; see
[Diagnostics](https://streamelements.github.io/obs-streamelements-core/architecture/diagnostics/).

It also only fires when the analytics manager is still alive —
`TrackCrash` logs and returns if it is not — which is normally true,
because the crash handler is one of the three services deliberately kept
alive past teardown for exactly this reason.

## What the plug-in does not report

Worth stating, because the absences are easy to mistake for a gap in the
data rather than a gap in the instrumentation. None of these is reported:
OBS starting or stopping, streaming starting or stopping, a scene or
composition change, a host API call, a dialog or pop-up opening, a dock
being closed by its own button, signing in or out, or an update being
installed. The host API surface is 210 entries and none of them tracks an
event as a side effect.

## Related

- [The envelope](envelope.md) — what the eight events have in common.
- [Adding an event](adding-an-event.md#in-the-plug-in) — the recipe.
