# Reading the data

The catalogue tells you what exists. This page is the set of traps between
that and a correct query — each one a place where the obvious reading of a
field is wrong, or where a number that looks like a rate has the wrong
denominator.

## `feature` is the producer, `source` is not

`feature` is the only field that reliably identifies which program sent an
event: `obs_selive_core_plugin`, `obs_core_plugin_installer`,
`obs_core_plugin_uninstaller`.

`source` looks like it should do the same job and does not. The plug-in sets
it to the literal `obs_plugin` on every event; the installer and uninstaller
set it **only on an abort**, where it carries the abort cause — `user`,
`network` or `local_machine`. So grouping by `source` mixes a producer name
with a failure classification and silently excludes every successful
install.

## Two names break the convention

Six of the eight plug-in events are `se_live_*` snake_case. Two are not:

- `live_support_click` — no prefix
- `OBS Studio Crashed` — title case, with spaces

So `name LIKE 'se_live_%'` drops the crash event and the Live Support
button, and `name LIKE '%_click'` drops the crash event and catches the dock
pair. There is no field that classifies events, so a complete plug-in query
has to enumerate the names.

## Three plug-in events do not exist in every build

| event | present when |
| --- | --- |
| `se_live_razer_wyvrn_initialized` | Windows build (`STREAMELEMENTS_ENABLE_WYVRN`) |
| `se_live_razer_wyvrn_init_failed` | the same, **and** the status is exactly `initFailed` |
| `OBS Studio Crashed` | macOS build **and** `STREAMELEMENTS_CRASH_HANDLER=bugsplat` |

Absence in the data is therefore not absence in the field. Two of those are
worth restating as denominators:

- **The two WYVRN events do not sum to sessions.** `dllNotFound` and
  `dllInvalidSignature` are deliberately silent, and together they are the
  overwhelming majority of machines, so the denominator is machines that
  have the Razer SDK — a population nothing in the data measures.
- **`OBS Studio Crashed` is not a crash count.** No Windows build sends it,
  and no Sentry-configured macOS build does either. A crash rate computed
  from it is wrong by the whole Windows population; the real data is in
  Sentry and BugSplat.

## Every value in `fields` is a string

Including the numbers. `seconds_since_start` is `"4"`,
`curr_step_index` is `"40"`, `wyvrn_init_duration_ms` is `"118"`. Cast
before comparing or ordering, or `"9" > "40"` lexically.

`fields` is also an array of `[name, value]` pairs rather than an object, so
a field is found by scanning the pairs — and nothing prevents a duplicate
name, because a caller's fields are appended to the built-in four without a
uniqueness check.

Props are properly typed, with one exception worth knowing: `_meta.mobile`
is a real JSON boolean `false`, not the string `"false"`.

## Two body shapes at one endpoint

The plug-in posts `[ { … } ]`, a one-element array. The installer and
uninstaller post a bare `{ … }`. Anything parsing raw request bodies has to
accept both.

## `_meta.platform` is not the platform

It is the literal `"desktop"` everywhere and never varies. The operating
system is `_meta.os`, it is a build-time constant in the plug-in rather than
a runtime read, and there is no architecture field at all — x64 and arm64
sessions are indistinguishable. Only installer and uninstaller events carry
`_meta.osversion`.

## The timing fields are per producer

`seconds_since_start` counts from the producer's own start — the analytics
manager's construction, or `scienceInit` in the NSIS `.onInit` — and
`seconds_since_previous_event` from that producer's previous event. Neither
crosses a process boundary. An installer event's "previous event" is never a
plug-in event, even on the same machine in the same minute.

Both are whole seconds, truncated, so a burst of events within one second
reports `0` rather than something sub-second.

## Funnels

The installer and uninstaller screen events are built to be read as a
funnel, and three properties govern how:

1. **Once per run, forward only.** The `IntCmp` guard fires a stage only
   when the furthest screen reached is *below* it, so Back-then-Next does
   not re-fire. Stage counts are distinct runs that reached a stage.
2. **`/AUTOSTART` skips three installer stages.** On that path the user
   agreement, target folder and components events never fire at all.
   Filter on the `auto_start` field before reading a drop-off between
   steps 10 and 50, or an unattended install looks like three abandonments.
3. **`curr_step_index` is the furthest screen reached**, not the screen on
   screen. After a Back, an abort reports the higher number.

`prev_event_name` makes the chain readable from a single row without a
self-join, and it is empty on a run's first event — except for the
second-instance abort, which is the one event that can be a run's first
without `..._start` having fired.

**An abort is not an error.** Closing the installer window is reported as
`se_live_installer_abort` with `source: user` and the message
`User cancelled installation`, so aborts are abandonment plus failure
together. Split on `message`, not on `source`:
`User refused to close running OBS components` is classified
`local_machine` although it is a user decision.

## What no event answers

Absences that are easy to mistake for missing rows:

| question | why not |
| --- | --- |
| how many times was OBS launched with SE.Live installed | `se_live_initialized` is the **last** statement of `Initialize()`, so a start-up that fails earlier reports nothing; it counts successful initialisations |
| what is the crash rate | see `OBS Studio Crashed` above |
| which version was uninstalled | the uninstaller sends `plugin_version` empty |
| which install produced this session | the producers share a machine id but not a session; there is no linking field |
| who, or which account | nothing in any payload identifies an account, channel or person — the plug-in knows the signed-in state and does not report it |
| did they stream, change scenes, open a dialog | not instrumented; the 210-entry host API tracks nothing as a side effect |
| what happened on macOS before the plug-in loaded | there is no macOS installer or uninstaller in this repository |

## Events can be lost, and the loss is biased

The plug-in queues events onto four worker threads. The queue is drained by
the analytics manager's destructor, so an ordinary shutdown flushes — but a
**crash takes the queue with it**. The events most likely to be lost are
therefore the ones from sessions that ended badly, which is exactly the
population an analysis of crashes or hangs would care about.
`trackSynchronousEvent` exists for that reason and has one caller.

The NSIS pair post with `HttpPostStringWait` and flush before the process
ends, so their loss mode is a dead network rather than a lost queue.

## Two fields that look missing because they were never sent

There is no analytics app id anywhere in the payload. The plug-in reads one
in the analytics manager's constructor —
`m_appId = config->GetHeapAnalyticsAppId()` — and never uses it, and the
installer defines a `HEAP_EVENT_PREFIX` that nothing references. Both are
vestiges of the Heap integration that the `science` endpoint replaced, and
neither reaches the wire. Don't look for an app id or a prefixed event
name; there are none.

## Related

- [The envelope](envelope.md) — the shape all of this is read against.
- [Identity and sessions](identity.md) — what can be joined to what.
