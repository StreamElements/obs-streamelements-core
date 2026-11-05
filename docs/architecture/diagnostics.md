# Diagnostics

Three independent systems: reference accounting that answers "did we leak?",
crash reporting that answers "what killed us?", and logging/analytics that
answer "what was happening?". They are separate on purpose and fail
separately.

## `SETrace` — reference accounting

`streamelements/SETrace.hpp` is a build-time switch with three settings:

| `ENABLE_SETRACE` | behaviour |
| --- | --- |
| `0` | off; every macro is the identity |
| **`1`** | **the shipping value.** One global counter: `SETRACE_ADDREF` increments, `SETRACE_DECREF` decrements |
| `2` | full trace: a map keyed by pointer, recording every acquire and release with file, line, statement and count |

The macros: `SETRACE_ADDREF`, `SETRACE_DECREF`, `SETRACE_AUTODECREF`,
`SETRACE_SCOPEREF`, `SETRACE_NOREF`, `SETRACE_IMPLICITDECREF`, `SETRACE_DUMP`.
The last three are documentation-only in every mode — they mark intent at sites
that deliberately do not count.

`SETRACE_DUMP()` runs in `obs_module_unload`, so **the result only exists after
a clean shutdown**, and the line to read is:

```
[obs-streamelements-core]: reference count balance = 0 (0 is good)
```

### What that number does and does not mean

It is the count of **annotated acquires that went unpaired**. It is *not* a
byte count and *not* proof of a leak: an acquire paired through an
*unannotated* release path moves the counter without leaking anything.

That distinction is not pedantry — it was the whole of a day. A balance of 3
and then 2 across two end-to-end runs read as a streaming leak. Four targeted
streaming runs came back at 0, including one on the unfixed code. The real
cause was a counted getter (`return SETRACE_ADDREF(...)`) parked in an
`OBS*AutoRelease` whose destructor's release is unannotated, so the counter
gained +1 per call while the obs refcount was always correct. The balance was
accusing the plug-in of a leak that did not exist, once per successful
transition change on the main canvas.

**A false positive in the only leak signal there is, is worse than a small
leak**, because every later reading of it is worthless. Hence
`check_counted_references_are_given_back_to_the_counter`.

### Finding out *which* object, with mode 2

Set `ENABLE_SETRACE 2`, rebuild, reproduce, close cleanly. Then:

- Read it by **summing each site name across all reported objects and pairing
  acquires against releases**, not by reading one object's block — pointer
  reuse merges unrelated objects into one entry, so per-object attribution
  misleads while per-site totals do not.
- Two properties make mode 2 comparable with mode 1: it never erases entries,
  so the per-site sum equals the global counter; and that holds only if no
  `release reference of data which was not previously allocated` lines appear.
  Grep for them first.
- **Reproduce with the exact script that produced the number**, not a
  reduction of it. Four reductions in a row came back clean because none of
  them performed the operation that actually did it.

!!! danger "Mode 2 is a local diagnostic and must never be committed"
    The shipping value is `1`. Check `git diff streamelements/SETrace.hpp`
    before committing anything from a session that used mode 2.

In mode 0 every macro expands to its own argument and costs nothing; in mode 1
`SETRACE_ADDREF` / `SETRACE_DECREF` are a single `os_atomic_inc_long` /
`os_atomic_dec_long` on one global, which is why the shipping build can afford
to leave the accounting on.

## Crash reporting

**The backend is a build-time choice**, not a runtime one:
`-DSTREAMELEMENTS_CRASH_HANDLER=bugsplat|sentry|none` (default `bugsplat`).
One CMake variable drives which handler compiles, which SDK links, and the
`SE_CRASH_HANDLER_*` define that `SECrashHandler::Create()` reads — keep those
three together. Only one backend is ever linked, so two can never contend for
the process exception filter.

`SECrashHandler` is the interface; `SEBugSplatCrashHandler` and
`SESentryCrashHandler` are the implementations (each with a `.mm` for macOS).
Supporting pieces:

- **`SECrashContext`** — the breadcrumbs attached to a report, including the
  async-call context stack the `QtPostTask` family maintains, so a crash inside
  a queued task reports where it was posted from.
- **`SECrashConsentDialog`** — asks before sending, and must disclose that
  reports are also sent automatically
  (`check_prompt_discloses_automatic_reports`). It is on the crash path, so it
  is bounded and must not pump foreign messages
  (`check_consent_prompt_is_bounded`,
  `check_consent_prompt_does_not_pump_foreign_messages`).
- **`se-crash-wer.dll`** (`SEWerModule.cpp`) — a Windows Error Reporting
  runtime exception module, which exists because **heap corruption and every
  `__fastfail` bypass SEH entirely**: the top-level filter never sees them, the
  SIGABRT door never sees them, and neither does OBS's. A WER module is the
  only mechanism that can capture them, because it runs out of process inside
  `WerFault.exe` after the crashed process is suspended. sentry-native ships
  one, but it claims *every* fast-fail in the host process — OBS's own and
  every other plug-in's — so ours is registered instead and sentry's
  deliberately is not: sentry looks for its module beside the host executable
  and we ship ours beside the plug-in, where sentry will not find it.
  `check_sentry_wer_is_not_beside_the_host_executable` pins that, and
  `check_wer_module_gates_before_forwarding` pins the gate: decline unless the
  crashed thread's stack passes through one of our modules.

Four more invariants cover the crash path, where ordinary code is not safe:
`check_crash_path_filesystem_calls_never_throw`,
`check_queued_tasks_suppressed_during_crash`,
`check_oom_handler_observes_and_chains`, `check_abort_path_drops_own_frames`,
plus `check_both_sentry_doors_are_owned` and
`check_sentry_init_is_diagnosable`.

**The crash handler is deliberately leaked.** It keeps `config`,
`performanceHistoryTracker` and `analyticsEventsManager` alive for the life of
the process, so any teardown work it needs must be an explicit call in
`Shutdown()` rather than something in a destructor. This is why a clean
shutdown reports `36 of 39 services released cleanly, 3 held by the crash
handler`.

!!! info "The Sentry DSN is not a secret"
    It is baked into `STREAMELEMENTS_SENTRY_DSN`; a DSN only permits
    *submitting* events. `SENTRY_AUTH_TOKEN` / `SENTRY_ORG` /
    `SENTRY_PROJECT` are CI-only, for symbol upload.

## Logging

`SEUtils/SELogUtils.hpp` carries `SELog` and `IsTraceLogLevel()`. Both were
moved out of headers that dragged Qt in, so that the API handler
infrastructure could reach them without reaching Qt — the general lesson being
that the blocker on a "pure" cluster is usually one declaration in the wrong
header.

`IsTraceLogLevel()` memoises, and the memo is computed once
(`check_the_trace_log_level_memo_is_computed_once`) — it was an unsynchronised
read-then-write from every logging thread.

Log lines worth grepping for after any run:

| pattern | meaning |
| --- | --- |
| `reference count balance` | the SETrace result (above) |
| `services released cleanly` | the teardown audit; a fourth held service is a finding |
| `shutdown complete` | `obs_module_unload` ran — without it, nothing was measured |
| `future_error` | a promise satisfied twice, usually dialog teardown |
| `AddConsumer` / `RemoveConsumer` | encoder provider counts; they must balance |
| `Number of memory leaks` | **libobs's own** counter, a host baseline (9–12 on OBS 33.0.1), not ours |
| `WASAPI: Device '' Terminated` | normal on every launch; not a crash |

## Analytics and performance history

`SEAnalyticsEventsManager` (`ISEAnalyticsEventsManager`) carries product
analytics and owns the machine and session unique ids that
`ISEHostInfoProvider` reports. `SEPerformanceHistoryTracker` keeps the rolling
performance series the crash context attaches. Both are among the three the
crash handler keeps alive.

Events are queued and posted by a worker pool, so a caller never blocks on
the network. `trackSynchronousEvent` is the exception and exists for the
crash handler, which has no later to post from.

**The one subsystem whose outcome only analytics can see is Razer WYVRN.**
Init runs on the SDK thread seconds after startup and ends in “unavailable”
on the overwhelming majority of machines, so the status reaches a page that
may not be loaded yet and a `LOG_INFO` line on the user’s own disk — neither
of which answers how many users have Razer software at all.
`SERazerWyvrnManager::TrackInitOutcome` fires one event per terminal branch
of the init decision:

| event | when |
| --- | --- |
| `se_live_razer_wyvrn_initialized` | `CoreInitSDK` succeeded |
| `se_live_razer_wyvrn_init_failed` | it did not — `dllNotFound`, `dllInvalidSignature` or `initFailed` |
| `se_live_razer_wyvrn_init_abandoned` | OBS closed inside the ~3.3 s init, so the outcome was discarded |

Each carries `wyvrn_status` from the same `SERazerWyvrnStatusToString` the
page and the log use, plus `wyvrn_sdk_initialized`,
`wyvrn_init_api_result`, `wyvrn_rzresult` and `wyvrn_init_duration_ms`. The
third event exists so the three add up to the number of sessions that
started an init; without it the gap reads as telemetry being lost rather
than as a cohort. `check_wyvrn_init_reports_its_outcome_to_analytics` pins
all three names and the field set.

## Related

- [Module lifecycle](module-lifecycle.md) — why the dump needs a clean exit.
- [Outputs and encoders](outputs.md) — the counted-reference pattern.
- [Testing](testing.md) — the live harness that reads all of this back.
