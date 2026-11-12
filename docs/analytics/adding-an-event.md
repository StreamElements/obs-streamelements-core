# Adding an event

There is no registry and no schema: an event name is a string literal at a
call site, so a new event ships the moment someone writes one and a typo
ships just as easily. That makes the decisions below worth making
deliberately, and makes this page's last section — updating the catalogue —
part of the change rather than paperwork.

## Decide three things first

**Is it a new event, or a field on an existing one?** A field is cheaper for
everybody: it needs no new name, it cannot be forgotten by a query that
enumerates names, and it keeps the event count meaningful. A new name is
right when the thing happens at a different time, not when it is a new
detail of something already reported.

**Prop or field?** The convention, followed deliberately at
`SERazerWyvrnManager.cpp:604`: a prop is a **dimension shared across the
whole surface**, a field is **detail of one event**. `sessionId` is a
dimension — every event has one and grouping by it means something.
`wyvrn_rzresult` is detail — meaningless on any other event. New data goes
in `fields` unless it is genuinely a dimension of everything, because a new
prop is a new column on twenty-two events, twenty-one of which will never
set it.

**Can it carry anything personal?** Two payloads already do —
`se_live_report_issue` carries the user's own text, and the dock events
carry a window title — and both are called out in
[Identity](identity.md#what-else-is-personal). Adding a third is a decision,
not an implementation detail. Paths, hostnames, URLs, account identifiers
and channel names are not in the catalogue today.

## In the plug-in

```cpp
m_analyticsEventsManager->trackEvent(
        "se_live_thing_happened",
        json11::Json::object{{"placement", "somewhere"}},
        json11::Json::array{
                json11::Json::array{"thing_id", id},
                json11::Json::array{"thing_duration_ms", buf}});
```

Five rules, each of which the existing call sites follow:

- **Take the manager by injection.** It is published on `SEApiServices` as
  `analyticsEventsManager`, typed `ISEAnalyticsEventsManager`, and a class
  that needs it takes `std::shared_ptr<ISEAnalyticsEventsManager>` in its
  constructor — never the container, and there is no `GetInstance()`. See
  [Services and injection](https://streamelements.github.io/obs-streamelements-core/architecture/services/).
- **Name it `se_live_<subject>_<verb>`**, snake_case. Two existing names
  break this and both are
  [a nuisance to query](reading-the-data.md#two-names-break-the-convention);
  don't add a third.
- **Every field value is a string.** Format numbers yourself —
  `snprintf(buf, sizeof(buf), "%.0f", ms)` — and keep the spelling the same
  as any log line that reports the same value, so a support log and a
  dashboard row can be matched by eye. The WYVRN result is hex in both
  places for exactly that reason.
- **`trackEvent`, not `trackSynchronousEvent`.** The synchronous variant
  posts on the calling thread and exists for the crash handler, which has no
  later; it is deliberately not on the interface. Ordinary events are queued
  and the queue is drained by the manager's destructor, so an event tracked
  during teardown still goes out.
- **Any thread is fine, a null manager is not.** `trackEvent` is safe from
  the Qt thread, an SDK thread or a worker — the shared state is atomic and
  `check_the_analytics_managers_shared_state_is_atomic` keeps it that way.
  But a class whose manager can be absent should guard: both
  `SERazerWyvrnManager::TrackInitOutcome` and the docks menu check for null
  first, because on an SDK thread a null dereference takes OBS down with
  nothing pointing at the call site.

Nothing else has to change. There is no version to bump — the host API
version in `Version.hpp` covers `window.host`, not analytics — and no list
to register the name in.

## In the installer or the uninstaller

```nsis
    Push 'se_live_installer_thing_happened'
    Push '[ "thing_id", "$R9" ], [ "thing_count", "3" ]'
    Call SendTrackingEvent
```

Two pushes, always, in that order: the event name, then a string of extra
field pairs or `''` for none. The string is concatenated onto the end of the
built-in field list, so it must be valid JSON array *elements* — pairs
separated by commas, no enclosing brackets.

**There is no escaping anywhere in this path.** The whole payload is built
by `StrCpy` string concatenation, so a value containing a double quote, a
backslash or a newline produces malformed JSON and the event is lost at the
endpoint rather than rejected locally. Values that come from the environment
— a path, a URL, anything the user typed — need to be known-safe before they
go in. The plug-in has no such hazard: `json11` escapes on `dump()`.

A note on NSIS quoting: single quotes around the pushed string let the
double quotes inside it stand, and `$` still interpolates, so `$R9` above is
the register's value while a literal `$` needs `$$`.

### Adding a screen event

Follow the existing handlers exactly — the guard is what makes the funnel
countable:

```nsis
Function onShowSomething
    IntCmp $g_instCurrentScreenIndex 35 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 35
    StrCpy $g_instCurrentScreenName "Something"

    Push 'se_live_installer_show_something'
    Push ''
    Call SendTrackingEvent
skip:
FunctionEnd
```

The index must sort with the existing ones (10, 20, 30, 40, 50, 60) because
`installation_started` is computed from `>= 50`, and because the guard's
"only going forward" property is index ordering rather than page ordering.
A new screen between two existing ones needs a value between them, which is
why the existing ones are spaced by ten.

### Adding a prop

Props are set in `SendTrackingEvent` itself, not at the call site — there is
no mechanism to pass one in. A conditional prop follows the `source` and
`message` pattern: a global set before the call, and a `StrCmp` against `""`
in `SendTrackingEvent` that appends the prop only when it is non-empty.

## Then update the catalogue

These pages are the only description of the pipeline that exists, so an
event that is not in them is invisible to everyone who is not reading the
source. A new or changed event means an edit to the relevant page here, in
the same commit — the same rule the architecture chapters carry.

Two things to add while the reasoning is fresh:

- **The condition, if the event is conditional.** Build switches, platform
  guards and "only this one status" are the facts that turn an event count
  into a rate, and the WYVRN pair is the worked example: its two deliberate
  silences are documented because without them the numbers mean something
  else entirely.
- **An invariant, if a dashboard will depend on the exact strings.**
  `check_wyvrn_init_reports_its_outcome_to_analytics` pins two event names,
  their field set and both of their silences, so a refactor that renames one
  fails `ctest` rather than silently emptying a chart. It is the precedent
  to copy; see
  [Testing](https://streamelements.github.io/obs-streamelements-core/architecture/testing/).

## Related

- [The envelope](envelope.md) — what a new event gets for free.
- [Reading the data](reading-the-data.md) — the traps a new event can add.
