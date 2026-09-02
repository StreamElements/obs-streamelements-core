# WYVRN effect preview — proof of concept

Parses and renders Razer `.chroma` lighting and Interhaptics `.haps` vibration
files in the browser.

**Self-contained by design.** No SE.Live host API, no bridge, and **no
third-party client-side code** — no framework, no jQuery, not even a webfont. The
page makes zero external requests. Everything is DOM, Canvas 2D and WebAudio.

Supports the requirement that motivated it: SE.Live can show a user what a WYVRN
event looks and feels like before they wire it to a stream event.

## Run it

```sh
node build.mjs                      # TypeScript -> dist/*.js
node test.mjs                       # parsers, against real assets on this machine
```

To produce the standalone demo page with effects baked in:

```sh
node build.mjs --manifest demo-manifest.json
# -> dist/demo.html, openable straight from disk
```

A manifest rather than a file list, because the body view needs each haps source
paired with the body regions it targets, and that pairing lives in the WYVRN
config, not in the `.haps` file:

```json
{
  "chroma": ["…/@Idle_Rainbow_Keyboard.chroma", "…"],
  "haps": [
    { "path": "…/Interact_Environment.haps",
      "targeting": [{ "target": "Chest", "spatialization": "Global", "gain": 1 }] }
  ]
}
```

Effects on a machine with Razer Synapse live under
`C:\Program Files (x86)\Interhaptics\hapticFolders\<App>\`.

**The build needs no dependencies.** It uses Node's own
`module.stripTypeScriptTypes()` rather than a toolchain, so there is nothing to
install. The trade is that the source must be *erasable* TypeScript — type
annotations, interfaces, `type` imports; no enums, no parameter properties, no
namespaces, since stripping cannot emit runtime code for those.

## Classes

Every class exposes `async dispose(): Promise<void>`, following the StreamElements
client SDK idiom. Disposal is idempotent, releases children in reverse
acquisition order, awaits each one, and survives a teardown that throws.

| Class | Owns |
| --- | --- |
| `Disposable` / `DisposableBag` | the disposal contract itself |
| `ChromaEffect` | a parsed `.chroma`: frames, device, duration |
| `HapsEffect` | a parsed `.haps`: melodies, transients, envelope sampling |
| `SequenceClock` | playback timing, shared by both haptics previews |
| `ChromaRenderer` | the device SVG, rAF loop, the DOM it created |
| `HapsRenderer` | canvas timeline, rAF loop, an optional `AudioContext` |
| `HapticBodyRenderer` | the two body views, rAF loop |

### Haptics playback

Both haptics previews present the same interface, so a caller driving them
together does not have to remember which is which:

```ts
await renderer.play();      // start, or restart from the top
renderer.pause();
renderer.oneShot = true;    // play once and stop, instead of repeating
```

`play()` is a **re-trigger**, not a no-op when already running — firing an event
that is already playing should restart it, which is how `SetEventName` behaves.
It is `async` on both, even though only the timeline has anything to await (it
may be opening an `AudioContext`).

Timing lives in `SequenceClock` rather than in each renderer, so the timeline and
the body view cannot drift apart:

```
|<-- duration -->|<-- pauseMs -->|<-- duration -->| ...
 playing           resting         playing
```

The rest is what makes a short effect readable — haptic effects are often a
fraction of a second, and looped back-to-back they read as a continuous buzz with
no discernible shape. During it the body goes dark and the timeline dims with its
playhead parked at the end, so both panels agree at a glance about whether
anything is playing.

`AudioContext.close()` is genuinely asynchronous, which is why `dispose()` returns
a promise rather than being a plain `close()`.

`HapticBodyRenderer` takes **multiple sources**, each a `HapsEffect` plus the body
regions it targets, because one WYVRN event routinely drives several at once. It
does *not* take ownership of the effects: the same one is usually on screen in the
timeline beside it, and disposing it here would pull it out from under that.

## The `.chroma` format

Undocumented by Razer, but the public WYVRN gallery decodes it client-side, and
its reader is plain `DataView` calls. Little-endian throughout:

```
uint32   version
uint8    deviceType      0 = linear strip, 1 = grid
uint8    device          index within that type class
uint32   frameCount
repeat frameCount:
    float32  duration    seconds
    uint32   color[ledCount]
```

Colours are Win32 `COLORREF` — `0x00BBGGRR`, low byte red, top byte always zero.

**`ledCount` is not in the file.** It comes from the device, and the pair
`(deviceType, device)` is the key — `device` alone is ambiguous because it indexes
within its type class. Derived from real assets and cross-checked against Razer's
published constants; each reproduces its file size exactly via
`size === 10 + frameCount * (4 + ledCount * 4)`:

| Device | deviceType | device | LEDs | grid |
| --- | --- | --- | --- | --- |
| Chroma Link | 0 | 0 | 5 | strip |
| Headset | 0 | 1 | 5 | strip |
| Mousepad | 0 | 2 | 15 | strip |
| Keyboard | 1 | 0 | 132 | 6 × 22 |
| Keypad | 1 | 1 | 20 | 4 × 5 |
| Mouse | 1 | 2 | 63 | 9 × 7 |
| Keyboard (extended) | 1 | 3 | 192 | 8 × 24 |

Both keyboard grids occur in the wild under the same `_Keyboard` filename suffix,
and only the header tells them apart. `ChromaEffect` therefore cross-checks the
file size and refuses a mismatch: assuming one geometry for both parses happily
and renders as noise. That check is not hypothetical — it is what caught the
missing 6 × 22 entry during development, on 8 files out of the first 400.

## The `.haps` format

JSON, in **two different schemas**, neither documented. Files under
`hapticFolders/<Game>/` use plain keys (`vibration.melodies[].notes[]`, envelopes
keyed by `position`/`value`), while the 202 files under
`hapticFolders/GenericEvent/` — the same set the Effects Library serves — use an
`m_`-prefixed shape (`m_vibration`, `m_startingPoint`, `m_length`, curves keyed by
`m_time`/`m_value`). `HapsEffect` normalises the second into the first, so
everything downstream sees one model.

One trap in that conversion: within the `m_` schema the two curves are **not in
the same units**. Amplitude keyframes are normalised 0..1, but frequency
keyframes are absolute Hz — observed up to ~242 across the GenericEvent set.
Treating frequency as normalised silently yields 50 kHz readings.

Either schema carries two kinds of content, and a file may have either or both:

- **Transients** — discrete taps at a position in seconds, with normalised
  amplitude and pitch. They carry no duration, so `amplitudeAt()` gives each an
  80 ms decay window; without one they are unsamplable, and a transient-only file
  reports zero amplitude for its whole length. That shape is not rare —
  `Interact_Environment` ships with two transients and no notes at all.
- **Melodies** — continuous vibration. Each note carries amplitude and pitch as
  keyframe envelopes, positioned relative to the note's own start.

Keyframe positions can be slightly *negative* in real files, and coincident
keyframes occur, so envelope sampling clamps at both ends and treats a zero-width
span as a step rather than dividing by zero.

`pitch` is normalised 0..1; map it through `vibration.frequency_range` for Hz.

## Verification

`node test.mjs` runs the parsers against every asset it can find — no browser, no
jsdom, because the parsers are deliberately DOM-free.

```
.chroma parsed : 900
.haps parsed   : 900  (515 using the m_ schema)
devices seen   : ChromaLink x150, Headset x150, Keyboard x8,
                 KeyboardExtended x142, Keypad x150, Mouse x150, Mousepad x150
1837 passed, 0 failed
```

It also covers disposal semantics (reverse order, idempotence, concurrent calls,
a throwing teardown, acquisition after disposal), the malformed-input paths, and
`SequenceClock` — cycle boundaries, wrap, the parked playhead, one-shot
completion, a zero-length effect, and re-triggering.

The walk is recursive, which it was not at first: `GenericEvent` nests two levels
deep, so a single-level walk found **zero** `.haps` files there and the entire
`m_` schema went untested while the suite reported green.

## Known gaps

- **Keyboard LED mapping is approximate.** The gallery concatenates every `led`
  element with every key-ish element and fills sequentially, which is what its own
  comment calls a "simplified keyboard mapping". This reproduces that rather than
  improving on it, so the animation matches the gallery exactly — but an
  individual key is not guaranteed to be the physically correct one.
- **The mouse has 63 grid positions and 16 LED shapes**, so only the first 16
  colours are used. Again, this is the gallery's behaviour.
- **The audible haptics preview is an approximation**, not a simulation. Sub-100 Hz
  vibration is felt rather than heard, so it is lifted into an audible band.
- **The body silhouettes are drawn here.** Unlike the device artwork, WYVRN has no
  equivalent asset to borrow — the gallery shows body targeting as small icons,
  not a figure. Regions are schematic areas, not anatomy.
- **`FallbackCommands` handling is unexercised.** The regex-pattern form is
  supported, but zero configs on this machine use it.
