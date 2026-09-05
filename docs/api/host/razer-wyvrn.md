# Razer WYVRN

> ⚠️ **Not available in this release.** The integration is complete and lives in
> the repository, but it is compiled out by default
> (`STREAMELEMENTS_ENABLE_WYVRN=OFF`) while its behaviour is confirmed against
> real hardware. In a build without it, none of the calls on this page are
> registered and `getHostCapabilities` is not registered either. This page
> documents what returns when the option is turned back on.

`window.host`

Chroma RGB lighting and Sensa HD haptics, driven by *naming* an event. What that
event looks and feels like is decided by the WYVRN configurations installed on
the viewer's machine, not by SE.Live.

**The integration is optional and Windows-only.** It needs Razer Synapse 4 and
the Chroma App, which most OBS users do not have. Every failure path ends in
"unavailable" — the calls below still answer normally, they simply report that
nothing is there. Check
[`getHostCapabilities`](host-information.md#gethostcapabilitiesresultcallbackhostcapabilities)
before assuming otherwise.

**Initialization is asynchronous and takes about 3.4 seconds.** It never blocks
OBS start, so for the first few seconds of a session the status is
`initializing` and `setRazerWyvrnEvent` returns `false`. Call
`getHostCapabilities` first, then subscribe to
[`hostRazerWyvrnStatusChanged`](../window.md#hostrazerwyvrnstatuschanged) — a
page that only subscribes will miss the event entirely if it loads after
initialization has already finished.

**SE.Live ships no WYVRN configuration of its own.** Anything that renders does
so through a configuration some other application installed, which Synapse
distributes automatically. This is by design, not a defect.

## `getAllRazerWyvrnEvents(ResultCallback<RazerWyvrnEventInfo[]>)`

**Available since API version 6.8**

Every event declared by every WYVRN configuration installed on this machine.

Each entry carries its Chroma and haptic components along with session-signed
URLs for their assets, so one call answers both "what can I fire" and "what
would firing it do". There is no need for a second call to inspect an event.

Returns an empty array when the integration is unavailable — never an error.

**Filtering.** A machine with Synapse installed typically declares around 4,000
events, so an optional filter object is accepted:

```js
window.host.getAllRazerWyvrnEvents(
    { source: '007 First Light', idPrefix: 'Aim_' },
    function (events) { /* ... */ });
```

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| source | string | Match the containing configuration folder exactly, case-insensitively. Omit or leave empty for all. |
| idPrefix | string | Match the beginning of the event id, case-insensitively. Omit or leave empty for all. |
| components | bool | Include each event's components and asset URLs. Defaults to `true`. |

### Cost

`components` is not a cosmetic flag. Including them costs one filesystem probe
and one URL signature per component, and the whole request runs inside the
process-wide API lock — so a slow call makes OBS unresponsive, not just this
caller.

Measured on a machine with Synapse installed (4,044 events, 24,698 components):

| Call | Time | Payload |
| --- | --- | --- |
| Filtered by source, with components | ~50 ms | 0.13 MB |
| Unfiltered, `components: false` | ~65 ms | 0.28 MB |
| Unfiltered, with components | ~2.1 s | 7.7 MB |

**Filter, or turn components off.** Asking for all 4,044 events with their
components is supported and correct, but it is a two-second request returning
nearly eight megabytes, and nothing else can call the host API while it runs.
The intended shape is a cheap ids-only sweep followed by a filtered call for
whatever the user actually selected.

**Data structures:** [`RazerWyvrnEventInfo`](../types/RazerWyvrnEventInfo.md)

## `setRazerWyvrnEvent(RazerWyvrnEventInfo, ResultCallback<success>)`

**Available since API version 6.8**

Fire an event.

Takes the whole [`RazerWyvrnEventInfo`](../types/RazerWyvrnEventInfo.md) object
rather than a bare string, so an item obtained from `getAllRazerWyvrnEvents` can
be handed back unmodified; only `id` and `fallback` are read. A bare string is
also accepted.

`id` is matched case-insensitively, and the event is fired under the spelling
the configuration uses — so `aim_on` reaches the SDK as `Aim_On`.

### Stopping playback

Three spellings mean the same thing, because a caller clearing an event should
not have to remember which shape the API wanted:

```js
window.host.setRazerWyvrnEvent(null, cb);       // null
window.host.setRazerWyvrnEvent(cb);             // no argument at all
window.host.setRazerWyvrnEvent({ id: '' }, cb); // an empty id
```

### `fallback`

`fallback` names another event to try when this one is not declared by any
configuration on the machine. It takes the same shape and nests to arbitrary
depth:

```js
window.host.setRazerWyvrnEvent({
    id: 'Headshot',
    fallback: { id: 'Hit',
                fallback: { id: 'Generic_Impact' } }
}, cb);
```

The first entry in the chain that names a real event wins, and only that one is
fired. If nothing in the chain exists on this machine, **nothing is sent** and
the call returns `false` — the chain that was tried is written to the OBS log.

The chain is resolved against the scanned configurations, not against the SDK.
That is deliberate: `CoreSetEventName` accepts an event belonging to a different
application and reports success, so asking the SDK "did that work?" would always
answer yes and the fallback would never fire.

Nesting is bounded at 16 levels, since the whole call runs inside the
process-wide API lock.

### Return value

Returns `false` when the integration is not ready — including during the first
few seconds of a session, while initialization is still running — and when no
event in the chain exists on this machine.

**Rate limiting is newest-wins.** The SDK accepts at most 30 events per second.
An event arriving inside that window is parked rather than dropped, and if
another arrives before the parked one is sent, it *replaces* it. The backlog is
one item by construction, and the name that survives is always the most recent —
the last thing that happened on the stream is the thing worth rendering. A
superseded event is logged to the OBS log so a name that never rendered is
visible rather than silently absent.

**Data structures:** [`RazerWyvrnEventInfo`](../types/RazerWyvrnEventInfo.md)

## Enum values

Every closed vocabulary this API returns is **camelCase**, or plain lowercase
where the value is a single word. The configurations on disk use the vendor's
own capitalisation (`Chest`, `VeryHigh`, `ChromaLink`); it is normalised on the
way out, by lowercasing the first character and leaving the rest.

| Field | Values |
| --- | --- |
| [`RazerWyvrnChromaComponent.device`](../types/RazerWyvrnChromaComponent.md) | `keyboard`, `keyboardExtended`, `keypad`, `mouse`, `mousepad`, `headset`, `chromaLink` |
| [`RazerWyvrnHapticComponent.mixing`](../types/RazerWyvrnHapticComponent.md) | `merge`, `override` |
| [`RazerWyvrnHapticComponent.priority`](../types/RazerWyvrnHapticComponent.md) | `veryLow`, `low`, `medium`, `high`, `veryHigh` |
| [`RazerWyvrnHapticTarget.target`](../types/RazerWyvrnHapticTarget.md) | `hand`, `head`, `chest`, `waist`, `leg`, `all`, `down`, `top` |
| [`RazerWyvrnHapticTarget.spatialization`](../types/RazerWyvrnHapticTarget.md) | `global`, `left`, `right` |
| [`RazerWyvrnEventInfo.kind`](../types/RazerWyvrnEventInfo.md) | `exact`, `fallbackPattern` |
| [`RazerWyvrnStatus.status`](../types/RazerWyvrnStatus.md) | see [RazerWyvrnStatus](../types/RazerWyvrnStatus.md) |

These are the values observed across every configuration installed on a machine
with Synapse. **Treat the lists as complete but not closed** — a future
configuration may introduce a value not listed here, and it will arrive
normalised the same way rather than being dropped.

Normalisation folds case splits in the source data onto one value: both `Waist`
and `waist` occur in the wild and both arrive as `waist`. It does **not** map
unknown spellings onto known ones — the shipped data contains a `Wasit` typo,
which arrives as `wasit` rather than being silently corrected into `waist`,
because a caller cannot otherwise tell a real value from a repaired one.

## Rendering previews

The asset URLs returned by `getAllRazerWyvrnEvents` point at the local file
server and are session-signed; fetching one yields the exact bytes on disk. Both
formats decode entirely in the browser.

A URL with a missing or altered signature is refused with
`{ "success": false, "message": "Invalid Request Signature" }`, and the file is
not served.

### The `.chroma` format

A frame-by-frame colour animation. All integers are little-endian.

```
uint32  version
uint8   deviceType       0 = 1D strip, 1 = 2D grid
uint8   device
uint32  frameCount
repeat frameCount times:
    float32  duration    seconds
    uint32   colors[ledCount]
```

Colours are `COLORREF` — `0x00BBGGRR`, so the **low** byte is red, not blue.

`ledCount` is **not in the file.** It comes from the `(deviceType, device)`
pair, which is the one thing a decoder cannot discover from the asset itself:

| Device | `deviceType` | `device` | Colors per frame | Grid |
| --- | --- | --- | --- | --- |
| ChromaLink | 0 | 0 | 5 | 5 LEDs |
| Headset | 0 | 1 | 5 | 5 LEDs |
| Mousepad | 0 | 2 | 15 | 15 LEDs |
| Keyboard | 1 | 0 | 132 | 6 × 22 |
| Keypad | 1 | 1 | 20 | 4 × 5 |
| Mouse | 1 | 2 | 63 | 9 × 7 |
| Keyboard (extended) | 1 | 3 | 192 | 8 × 24 |

`device` indexes within its type class, so the number alone is ambiguous — the
**pair** is the key.

**Both keyboard geometries occur under the same `_Keyboard` filename**, and only
the header separates them. Assuming the 192-LED grid parses the 132-LED files
without error and renders as noise, so verify the size rather than trusting the
table:

```
size === 10 + frameCount * (4 + ledCount * 4)
```

Refuse a mismatch.

### The `.haps` format

JSON, in one of two schemas: a plain one (`vibration.melodies[].notes[]`) and an
`m_`-prefixed one (`m_vibration.m_melodies[].m_notes[]`). Both occur in the
shipped data.

They are not simply renamed. In the `m_` schema, amplitude is normalised to
0..1 but **frequency is absolute Hz**, where the plain schema normalises both.
Treating one as the other yields frequencies three orders of magnitude wrong.

Transients (`transients[]` / `m_transients[]`) sit alongside melodies and carry
their own amplitude; a decoder that walks only the melodies reports zero
amplitude for a transient-only file.

## Events

[`hostRazerWyvrnStatusChanged`](../window.md#hostrazerwyvrnstatuschanged)
