# Architecture

How `obs-streamelements-core` — the SE.Live native plug-in for OBS Studio —
is put together, and why it is put together that way.

This is the companion to the
[JavaScript OBS API reference](https://streamelements.github.io/obs-streamelements-core/api/),
which documents the surface a page sees, and to the
[Analytics catalogue](https://streamelements.github.io/obs-streamelements-core/analytics/),
which documents what leaves the machine. These pages document what is behind
both: who creates what, in which order, on which thread, under which lock,
and which of those arrangements are load-bearing rather than incidental.

## The ten-minute version

The plug-in is a single shared library that OBS loads like any other
(`obs-streamelements-core.dll` / `.so` / `.dylib`), built inside an
obs-studio tree. It does three things:

1. **It hosts web pages inside OBS** — docks, a central widget, pop-up
   windows, modal and non-modal dialogs, and off-screen background workers.
   Each is a CEF browser widget borrowed from the sibling `obs-browser`
   plug-in.
2. **It exposes OBS to those pages** as `window.host`, built from 208
   registered handler blocks plus one hand-written registration, reached over
   a **local WebSocket** — not over CEF IPC. A few blocks sit behind feature
   switches, so the live count is build-specific: `Object.keys(window.host)`
   is 210 in a WYVRN-enabled Windows build, methods plus four properties.
3. **It adds capabilities OBS does not have** — most visibly multiple
   simultaneous video *compositions* ("canvases") with their own scenes,
   transitions, encoders and streaming outputs, alongside OBS's own single
   native one.

Everything else follows from those three.

<figure class="se-fig">
<div class="se-fig-scroll">
<svg viewBox="0 0 880 520" role="img" aria-label="The plug-in in four layers. Page JavaScript calls window.host over a local WebSocket on port 27952. SEWebsocketApiServer and SEApiMessageHandler dispatch 208 handler blocks, which reach services through the SEApiServices container. The container publishes five domains: compositions, outputs, widgets, host-level services and diagnostics. All of them are built on libobs, obs-frontend-api and Qt 6.">
<title>How a host API call reaches libobs</title>
<rect class="se-box" x="240" y="14" width="400" height="58" rx="6"/>
<text class="se-id" x="440" y="40" text-anchor="middle">window.host.&lt;method&gt;(args, callback)</text>
<text class="se-sub" x="440" y="58" text-anchor="middle">page JavaScript</text>
<path class="se-edge-em" d="M 440,72 V 104"/>
<path class="se-head-em" d="M 440,112 L 434.5,103 L 445.5,103 Z"/>
<text class="se-note-em" x="452" y="86">WebSocket, port 27952</text>
<text class="se-note" x="452" y="101">not CEF IPC</text>
<rect class="se-box" x="240" y="112" width="400" height="78" rx="6"/>
<text class="se-id" x="440" y="136" text-anchor="middle">SEWebsocketApiServer</text>
<text class="se-id" x="440" y="156" text-anchor="middle">SEApiMessageHandler(Base)</text>
<text class="se-sub" x="440" y="176" text-anchor="middle">208 registered handler blocks</text>
<path class="se-edge" d="M 440,190 V 220"/>
<path class="se-head" d="M 440,228 L 434.5,219 L 445.5,219 Z"/>
<text class="se-id-sm" x="452" y="212">apiServices-&gt;</text>
<rect class="se-box" x="240" y="228" width="400" height="72" rx="6"/>
<text class="se-id" x="440" y="252" text-anchor="middle">SEApiServices</text>
<text class="se-sub" x="440" y="270" text-anchor="middle">the service container</text>
<text class="se-sub" x="440" y="287" text-anchor="middle">39 services, mostly as ISE* contracts</text>
<path class="se-edge" d="M 440,300 V 322 M 102,322 H 778 M 102,322 V 337 M 271,322 V 337 M 440,322 V 337 M 609,322 V 337 M 778,322 V 337"/>
<path class="se-head" d="M 102,346 L 96.5,337 L 107.5,337 Z M 271,346 L 265.5,337 L 276.5,337 Z M 440,346 L 434.5,337 L 445.5,337 Z M 609,346 L 603.5,337 L 614.5,337 Z M 778,346 L 772.5,337 L 783.5,337 Z"/>
<text class="se-note" x="450" y="316">publishes</text>
<rect class="se-box" x="24" y="346" width="156" height="78" rx="6"/>
<text class="se-ttl" x="102" y="370" text-anchor="middle">compositions</text>
<text class="se-sub" x="102" y="388" text-anchor="middle">scenes, scene items</text>
<text class="se-sub" x="102" y="405" text-anchor="middle">transitions</text>
<rect class="se-box" x="193" y="346" width="156" height="78" rx="6"/>
<text class="se-ttl" x="271" y="370" text-anchor="middle">outputs</text>
<text class="se-sub" x="271" y="388" text-anchor="middle">encoders, RTMP</text>
<text class="se-sub" x="271" y="405" text-anchor="middle">replay buffer</text>
<rect class="se-box" x="362" y="346" width="156" height="78" rx="6"/>
<text class="se-ttl" x="440" y="370" text-anchor="middle">widgets</text>
<text class="se-sub" x="440" y="388" text-anchor="middle">docks, dialogs</text>
<text class="se-sub" x="440" y="405" text-anchor="middle">menus, workers</text>
<rect class="se-box" x="531" y="346" width="156" height="78" rx="6"/>
<text class="se-ttl" x="609" y="370" text-anchor="middle">host level</text>
<text class="se-sub" x="609" y="388" text-anchor="middle">streaming, updates</text>
<text class="se-sub" x="609" y="405" text-anchor="middle">host info</text>
<rect class="se-box" x="700" y="346" width="156" height="78" rx="6"/>
<text class="se-ttl" x="778" y="370" text-anchor="middle">diagnostics</text>
<text class="se-sub" x="778" y="388" text-anchor="middle">trace, crash</text>
<text class="se-sub" x="778" y="405" text-anchor="middle">reporting</text>
<path class="se-edge" d="M 102,424 V 444 M 271,424 V 444 M 440,424 V 444 M 609,424 V 444 M 778,424 V 444 M 102,444 H 778 M 440,444 V 464"/>
<path class="se-head" d="M 440,472 L 434.5,463 L 445.5,463 Z"/>
<text class="se-note" x="450" y="460">built on</text>
<rect class="se-box" x="240" y="472" width="400" height="46" rx="6"/>
<text class="se-id" x="440" y="500" text-anchor="middle">libobs + obs-frontend-api + Qt 6</text>
</svg>
</div>
<figcaption>Every <code>window.host</code> call takes this path, top to bottom. The two labelled hops are where the plug-in's shape is decided: a local WebSocket rather than CEF IPC, and a container handed to the handler rather than a global reached from it.</figcaption>
</figure>

Three facts about this diagram are worth more than the diagram:

- **The root is not a singleton.** `SEMain` is created by
  `obs-streamelements-core-plugin.cpp` and owned there; a class that needs a
  service is *handed* it. There is no `GetInstance()` and no
  `IsInstanceAvailable()` anywhere in the plug-in, and two source invariants
  keep it that way.
- **The arrows point one way.** Services never own their consumers, and
  anything whose lifetime a service controls — a browser widget, a dialog, an
  API handler — holds the container through a `weak_ptr`. That rule is the
  only reason shutdown terminates; see [Services and injection](services.md).
- **The container is reached through interfaces.** `SEApiServices` carries
  `ISE*` contracts, not concrete classes, so the 2,800-line translation unit
  holding every host API body compiles in a test binary that links neither
  libobs nor Qt. See [Testing](testing.md).

## Where to start, by what you are doing

| | |
| --- | --- |
| Adding a `window.host` method | [The host API bridge](host-api-bridge.md), then the `/add-host-api` skill |
| Adding a subsystem | [Services and injection](services.md) |
| Chasing a shutdown hang or a leak | [Module lifecycle](module-lifecycle.md), [Diagnostics](diagnostics.md) |
| Touching scenes or transitions | [Compositions and scenes](compositions.md), [Threading and locks](threading.md) |
| Touching streaming, recording or encoders | [Outputs and encoders](outputs.md) |
| Touching docks, dialogs or menus | [UI and widgets](ui.md) |
| Working out why the build or CI did that | [Build](build.md) |
| Working out whether a change is safe | [Testing](testing.md), [Enforced invariants](invariants.md) |

## The chapters

- **[Module lifecycle](module-lifecycle.md)** — the four module entry points,
  what happens at `FINISHED_LOADING`, the `ready` gate, and the two ways OBS
  can ask the plug-in to shut down (one of which is a trap).
- **[Services and injection](services.md)** — the root, the container, the
  interfaces, the weak back-edge rule, the teardown audit, and the recipe for
  adding a service.
- **[The host API bridge](host-api-bridge.md)** — the WebSocket transport, the
  handshake that builds `window.host`, registration and dispatch, the two
  deliberate lock domains, async completion, and argument type-checking.
- **[The data model](data-model.md)** — `CefValue` here is not CEF, what that
  changes, and the serialization conventions.
- **[Compositions and scenes](compositions.md)** — native versus custom
  compositions, scenes and items, transitions, and the lock discipline around
  them.
- **[Outputs and encoders](outputs.md)** — the output family, lazy encoder
  providers and their reference counting, output settings.
- **[UI and widgets](ui.md)** — widget and dock management, browser widgets,
  dialogs, pop-ups, menus, OBS's own UI controls, and state persistence.
- **[Threading and locks](threading.md)** — which threads exist, the Qt
  posting macros, and the re-entrancy hazards that have actually bitten.
- **[Diagnostics](diagnostics.md)** — `SETrace` reference accounting, the
  crash-reporting backends, logging and analytics.
- **[Build](build.md)** — how the plug-in attaches to an obs-studio tree, the
  CMake switches, and why the OBS version matters.
- **[Testing](testing.md)** — the two kinds of test, mutation testing, and the
  live end-to-end harness.
- **[Enforced invariants](invariants.md)** — the 80 architectural rules the
  test suite enforces, grouped by what they protect.

## How to read the code

The source carries long comments at the places where the reasoning matters,
and they are the authority — these pages summarise and connect them, they do
not replace them. When the two disagree, the code is right and a page needs
fixing.

Two conventions worth knowing before reading anything:

- **Naming.** One class per file, file name == class name,
  `SE<Thing>.{hpp,cpp}`; interfaces are `ISE<Thing>.hpp` beside their
  implementation. Members are `m_camelCase`, file statics `s_`, globals `g_`.
- **`CefRefPtr` is `std::shared_ptr`.** `deps/cef-stub/` reimplements the CEF
  value types as a plain JSON-ish data model. None of CEF's refcounting or
  thread-affinity rules apply to them. See [The data model](data-model.md).
