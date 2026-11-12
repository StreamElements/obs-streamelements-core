# The envelope

Every event is one JSON object with the same handful of top-level
properties, plus a `fields` array carrying the per-event detail. The three
producers build it in three separate places and agree about most of it.

| producer | builds the envelope in |
| --- | --- |
| the plug-in | `SEAnalyticsEventsManager::AddRawEvent`, `SEAnalyticsEventsManager.cpp:62` |
| the installer | `SendTrackingEvent`, `CI/obs-streamelements-installer/main.nsi:1299` |
| the uninstaller | `SendTrackingEvent`, `CI/obs-streamelements-uninstaller/main.nsi:468` |

There is one of each. No event is assembled anywhere else, which is why
"what does every event carry" has a single answer per producer rather than a
per-call-site one.

## Props

| prop | the plug-in | the installer and uninstaller |
| --- | --- | --- |
| `feature` | `obs_selive_core_plugin` | `obs_core_plugin_installer` / `obs_core_plugin_uninstaller` |
| `name` | the event name | the event name |
| `source` | always `obs_plugin` | **only on an abort**, and then the abort cause |
| `placement` | `obs`, unless the caller overrides it | never set |
| `sessionId` | a GUID minted once per plug-in load | a GUID minted once per run |
| `hostMachineId` | the machine identity — see [Identity](identity.md) | the same value, resolved independently |
| `message` | only where a caller passes one | only on an abort: the abort reason text |
| `_meta` | see below | see below |
| `fields` | the array below | the array below |

`name` is the event name and the only prop that varies per event. Everything
else is either constant for a producer or constant for a run.

### `source` means two different things

In the plug-in it is the producer: the literal `obs_plugin`, on every event.
In the NSIS pair it is absent on the ordinary path and carries the *cause of
an abort* when there is one — `user`, `network` or `local_machine`. So a
query that groups by `source` mixes a producer name with a failure
classification, and silently drops every successful install. Group by
`feature`.

## `fields`

`fields` is an array of two-element arrays, each `[name, value]`, and **every
value is a string** — including the numbers. It is not an object, so a
consumer looks a field up by scanning the pairs.

These four are on every event from every producer:

| field | value |
| --- | --- |
| `plugin_version` | the SE.Live version, `major.minor.patch.build` |
| `obs_version` | the host OBS version (plug-in only — see below) |
| `seconds_since_start` | whole seconds since the producer started |
| `seconds_since_previous_event` | whole seconds since that producer's previous event |

The installer and uninstaller add their own on top, and both add
`prev_event_name`, `curr_step_index` and `curr_step_name`. Those are listed
with the events that carry them, in [Installer events](installer.md#fields)
and [Uninstaller events](uninstaller.md#fields).

### Why `fields` and not more props

The convention, stated at `SERazerWyvrnManager.cpp:604` where it was
followed deliberately: a prop is a **dimension shared across the whole
surface**, a field is **detail of one event**. `sessionId` is a dimension —
every event has one and grouping by it is meaningful. `wyvrn_rzresult` is
detail — it is meaningless on any other event. New data belongs in `fields`
unless it is genuinely a dimension of everything.

### The two timing fields are per producer, not per machine

`seconds_since_start` counts from when *that producer* started: the
analytics manager's construction early in the plug-in's `Initialize()`, or
`scienceInit` in the NSIS `.onInit`. `seconds_since_previous_event` counts
from the previous event of the same producer. Neither crosses a process
boundary, so the installer's "previous event" is never a plug-in event even
though the two share a `hostMachineId`.

In the plug-in both are derived from one `std::atomic::exchange` of the
previous timestamp, because `trackEvent` is called from several threads at
once; `check_the_analytics_managers_shared_state_is_atomic` pins that. The
subtraction is also guarded against a negative interval, which is reachable
because the exchange orders the writes but not the two `os_gettime_ns()`
reads that precede them — unsigned, an unguarded underflow would report
roughly 584 years rather than 0.

## `_meta`

| key | the plug-in | the installer and uninstaller |
| --- | --- | --- |
| `mobile` | `false` (a JSON boolean) | `false` (a JSON boolean) |
| `platform` | `"desktop"` | `"desktop"` |
| `os` | `"Windows"`, `"macOS"`, `"linux"` or `"unknown"` | `"Windows"`, hardcoded |
| `osversion` | **absent** | the result of NSIS `GetWindowsVersion` |

**`platform` does not carry the platform.** It is the literal string
`"desktop"` in all three producers and never varies. The operating system is
`_meta.os`, and in the plug-in it is chosen by the preprocessor —
`#ifdef WIN32` / `__APPLE__` / `__linux__` — so it describes the build, not
the host. There is no architecture anywhere in the envelope, so an x64 and
an arm64 session are indistinguishable.

Only the NSIS pair report an OS version. The plug-in has far richer host
detail available — `SerializeSystemHardwareProperties` and friends, exposed
to pages as `getSystemHardwareProperties` — and sends none of it.

## Where the three disagree

Four differences, each one a thing to check before writing a query across
producers:

1. **`osversion` exists only on installer and uninstaller events** (above).
2. **The plug-in wraps its payload in a one-element array.** The body is
   `[ { … } ]`, built at `SEAnalyticsEventsManager.cpp:151`. The NSIS pair
   post a bare `{ … }`. Same endpoint, two body shapes.
3. **`source` means different things** (above).
4. **`obs_version` is only meaningful from the plug-in.** It comes from
   `obs_get_version_string()`, which the installer cannot call; the NSIS
   pair carry the field name only as part of the shared four, with the
   installer's own `setup_version` beside it.

## Overriding: caller props win, caller fields are appended

In the plug-in, `trackEvent(name, props, fields)` merges the caller's props
**after** the built-in ones (`AddRawEvent`, `:139`), so a caller can replace
any of them — `placement`, `sessionId`, `hostMachineId`, even `_meta`
wholesale. Three callers do override `placement`, deliberately, to say where
in the UI the event came from.

`fields` is the exception and the asymmetry is intentional:
`props["fields"] = fields` is assigned *after* the merge (`:147`), so a
caller passing a `fields` **prop** has it discarded, while the `fields`
**argument** is appended to the built-in four. There is no way for a caller
to remove or reorder those four.

The NSIS pair have the same shape with one argument instead of two:
`SendTrackingEvent` takes the event name and a string of additional field
pairs, which is concatenated onto the end of the built-in list. There is no
way to override a prop from a call site.

## Transport

The plug-in queues. `trackEvent` pushes a task onto a
`BlockingConcurrentQueue` drained by **four worker threads**, so a caller
never blocks on the network, and the queue is drained by the manager's own
destructor at shutdown — which is why an event tracked during teardown still
goes out. `trackSynchronousEvent` posts on the calling thread instead and
exists for one caller, the crash handler, which has no later to post from.
It is deliberately not on `ISEAnalyticsEventsManager`.

The NSIS pair call `NSISHTTP::HttpPostStringWait` and then `scienceFlush`
(`NSISHTTP::HttpFlushAllAsyncRequests`) at the points where the process is
about to end. `NSISHTTP.dll` and `NSISConfig.dll` are **not built from this
repository** — the installer workflows delete the vendored copies and fetch
them from `cdn.streamelements.com/obs/dist/nsis-http` and `.../nsis-config`
at build time, so the transport implementation is not readable here.

## A whole payload, three times

From the plug-in (keys as `json11` emits them, sorted, since
`Json::object` is a `std::map`):

```json
[
  {
    "_meta": { "mobile": false, "os": "Windows", "platform": "desktop" },
    "feature": "obs_selive_core_plugin",
    "fields": [
      ["plugin_version", "26.11.11.1100"],
      ["obs_version", "32.2.2"],
      ["seconds_since_start", "4"],
      ["seconds_since_previous_event", "4"],
      ["is_onboarding", "false"]
    ],
    "hostMachineId": "WUID/4C4C4544-0051-3610-8056-B4C04F4D3732",
    "name": "se_live_initialized",
    "placement": "obs",
    "sessionId": "b0f8c0e2-5a1d-4f3b-9c7e-2d6a1f0b4e88",
    "source": "obs_plugin"
  }
]
```

From the installer, on the ordinary path — note the bare object, the absent
`source`, and `osversion`:

```json
{
  "feature": "obs_core_plugin_installer",
  "name": "se_live_installer_show_choose_components",
  "sessionId": "7d1e3a55-9b02-4c8e-bf41-0a9d2c7e5513",
  "_meta": {
    "mobile": false, "os": "Windows",
    "platform": "desktop", "osversion": "10.0"
  },
  "hostMachineId": "WUID/4C4C4544-0051-3610-8056-B4C04F4D3732",
  "fields": [
    ["plugin_version", "26.11.11.1100"],
    ["setup_version", "26.11.11.1100"],
    ["seconds_since_start", "9"],
    ["seconds_since_previous_event", "3"],
    ["prev_event_name", "se_live_installer_show_select_target_folder"],
    ["curr_step_index", "40"],
    ["curr_step_name", "Choose Components"],
    ["auto_start", ""],
    ["installation_started", "false"],
    ["obs_download_requested", ""],
    ["obs_download_requested_by", ""]
  ]
}
```

From the uninstaller, where `plugin_version` and `setup_version` are sent
**empty** — see [Uninstaller events](uninstaller.md#it-reports-no-version):

```json
{
  "feature": "obs_core_plugin_uninstaller",
  "name": "se_live_uninstaller_abort",
  "sessionId": "1f44b8c7-2e90-4a16-8d35-6c0b9e7a2d41",
  "_meta": {
    "mobile": false, "os": "Windows",
    "platform": "desktop", "osversion": "10.0"
  },
  "hostMachineId": "WUID/4C4C4544-0051-3610-8056-B4C04F4D3732",
  "source": "user",
  "message": "User cancelled uninstaller",
  "fields": [
    ["plugin_version", ""],
    ["setup_version", ""],
    ["seconds_since_start", "12"],
    ["seconds_since_previous_event", "5"],
    ["prev_event_name", "se_live_uninstaller_show_progress"],
    ["curr_step_index", "20"],
    ["curr_step_name", "Uninstall Progress"]
  ]
}
```

The `hostMachineId` and `sessionId` values above are illustrative shapes,
not real ones.

## Related

- [Identity and sessions](identity.md) — where the two ids come from.
- [Reading the data](reading-the-data.md) — the traps this shape creates.
