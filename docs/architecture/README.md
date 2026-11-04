# Architecture

How `obs-streamelements-core` — the SE.Live native plug-in for OBS Studio —
is put together, and why it is put together that way.

This is the companion to the
[JavaScript OBS API reference](https://streamelements.github.io/obs-streamelements-core/api/),
which documents the surface a page sees. These pages document what is behind
it: who creates what, in which order, on which thread, under which lock, and
which of those arrangements are load-bearing rather than incidental.

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

``` text
                        ┌─────────────────────────────────────────┐
     page JavaScript    │  window.host.<method>(args, callback)   │
                        └───────────────────┬─────────────────────┘
                                            │  WebSocket, port 27952
                        ┌───────────────────▼─────────────────────┐
                        │  SEWebsocketApiServer                   │
                        │  SEApiMessageHandler(Base)  ─ 208 blocks│
                        └───────────────────┬─────────────────────┘
                                            │  apiServices-> …
                        ┌───────────────────▼─────────────────────┐
                        │  SEApiServices — the service container  │
                        │  39 services, mostly as ISE* contracts  │
                        └───────────────────┬─────────────────────┘
                                            │
       ┌───────────────┬────────────────────┼───────────────┬─────────────────┐
       ▼               ▼                    ▼               ▼                 ▼
  compositions      outputs              widgets         host-level        diagnostics
  scenes, items   encoders, RTMP      docks, dialogs    streaming,        trace, crash
  transitions     replay buffer       menus, workers    updates, info     reporting
       │               │                    │               │                 │
       └───────────────┴────────────────────┴───────────────┴─────────────────┘
                                            │
                        ┌───────────────────▼─────────────────────┐
                        │  libobs + obs-frontend-api + Qt 6       │
                        └─────────────────────────────────────────┘
```

Three facts about this diagram are worth more than the diagram:

- **The root is not a singleton.** `SEGlobalStateManager` is created by
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
- **[Enforced invariants](invariants.md)** — the 74 architectural rules the
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
