# Razer WYVRN

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
`initializing` and `setRazerWyvrnEventName` returns `false`. Call
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

**Data structures:** [`RazerWyvrnEventInfo`](../types/RazerWyvrnEventInfo.md)

## `setRazerWyvrnEventName(RazerWyvrnEventInfo, ResultCallback<success>)`

**Available since API version 6.8**

Fire an event.

Takes the whole [`RazerWyvrnEventInfo`](../types/RazerWyvrnEventInfo.md) object
rather than a bare string, so an item obtained from `getAllRazerWyvrnEvents` can
be handed back unmodified; only `id` is read. A bare string is also accepted.

An empty or absent `id` **stops playback**, which is the SDK's own convention.

Returns `false` when the integration is not ready — including during the first
few seconds of a session, while initialization is still running.

**Rate limiting is newest-wins.** The SDK accepts at most 30 events per second.
An event arriving inside that window is parked rather than dropped, and if
another arrives before the parked one is sent, it *replaces* it. The backlog is
one item by construction, and the name that survives is always the most recent —
the last thing that happened on the stream is the thing worth rendering. A
superseded event is logged to the OBS log so a name that never rendered is
visible rather than silently absent.

**Data structures:** [`RazerWyvrnEventInfo`](../types/RazerWyvrnEventInfo.md)

## Rendering previews

The asset URLs returned by `getAllRazerWyvrnEvents` point at the local file
server and are session-signed; fetching one yields the exact bytes on disk. Both
formats decode entirely in the browser.

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
