# Services and injection

The plug-in is one root object, a container of services it creates, and a rule
about which direction references may point. Those three things are most of the
architecture.

## The root is not a singleton

`SEMain` derives from `SEObsAppMonitor`, is created by `SEMain::Create()` from
`obs-streamelements-core-plugin.cpp`, and is owned by one `static
std::shared_ptr` in that file. It was a singleton reached through
`GetInstance()` from 291 places; CORE-757 removed every one of them.

So:

- **There is no `GetInstance()` and no `IsInstanceAvailable()`.** A class that
  needs a service takes it as a constructor parameter.
- The question `IsInstanceAvailable()` used to answer — "does the root exist
  and is it initialized?" — is now the `weak_ptr` lock that every holder
  already performs, plus the `ready` flag on the container.
- `check_the_root_is_not_a_singleton` and `check_no_singletons_remain` keep it
  that way; `check_injected_services_are_not_also_fetched_globally` stops a
  class taking a service by injection *and* reaching for it globally.

## `SEApiServices`: a container and nothing else

`streamelements/SEApiServices.hpp` is a plain struct — public `shared_ptr`
members, one per object `Initialize()` creates, **no methods, no accessors, no
policy**. `check_api_services_is_a_container_and_nothing_else` enforces that
literally.

A member is null until `Initialize()` has got that far; after
`PublishApiServices()` it stays set for as long as the container lives, and
nothing empties it at teardown
(`check_api_services_container_is_not_emptied_at_teardown` — see
*The ring*, below, for why that invariant is phrased as a prohibition).

Two members are not services:

- `bool ready` — the dispatch gate. See
  [Module lifecycle](module-lifecycle.md).
- `QCef *cef` and `QCefCookieManager *cefCookieManager` — **borrowed, not
  owned.** The root creates the browser panel and deletes it in `Shutdown()`;
  they are in the container because the things that need them (browser
  widgets) are handed the bundle, not the panel.

**Field order is load-bearing.** Fields are destroyed in reverse declaration
order. `statePersistence` borrows raw pointers to four of the managers above
it, so it must stay below them; the managers that own dialogs and pop-up
windows stay below *it*, so their widgets go before the docks. Reordering the
struct is a lifetime change disguised as a tidy-up.

## The root keeps concrete handles; the container publishes interfaces

`SEMain` holds each service by its concrete type and drives its lifecycle —
`Shutdown()`, `Reset()`, `Update()`, `SetEnabled()`. `PublishApiServices()`
then assigns those into the container, where the member type is an **`ISE*`
interface**. There are 34 such interface headers, each beside its
implementation, declaring only what a container consumer calls.

Why the split: lifecycle methods only the root calls have no place on an
API-facing contract, and the inverse is also true — a method two container
consumers call belongs on the contract even if it is named `Reset()`. The
practical payoff is in [Testing](testing.md): because the container carries
interfaces, the translation unit holding every host API body reaches no obs
and no Qt header, and compiles in a test binary that links neither.

The shape of an interface, as set by the smallest one:

- the header sits beside its implementation, declares only what consumers
  call, and includes `cef-headers.hpp` and nothing else;
- the concrete class inherits it and marks those methods `override`, keeping
  its lifecycle surface off the contract;
- `SEApiServices` publishes `shared_ptr<ISEFoo>`, so the upcast happens where
  the root assigns it;
- no consumer body changes.

!!! warning "Mark every override"
    Giving a class its first `override` arms clang's
    `-Winconsistent-missing-override` for *all* of that class's overrides,
    including ones that have been unmarked for years. MSVC has no equivalent,
    so this breaks the macOS build and not the Windows one. It has happened.

## The ring, and the rule that prevents it

Four reference cycles used to run through this container, all of the shape
`container -> service -> container`, plus one that refcounting cannot see at
all:

<figure class="se-fig">
<div class="se-fig-scroll">
<svg viewBox="0 0 880 260" role="img" aria-label="A five-node ownership chain that would close into a cycle. SEApiServices, the service container, owns SEBrowserWidgetManager, which deletes the QDockWidgets in its destructor. Each dock destroys its SEBrowserWidget, which holds an SEApiMessageHandler, which refers back to SEApiServices. That last edge is a weak_ptr, locked per call: non-owning, so the chain never closes and the container's reference count can reach zero.">
<title>The ownership chain through the container, and the one edge that does not own</title>
<text class="se-note" x="279" y="40" text-anchor="middle">owns</text>
<text class="se-note" x="569" y="40" text-anchor="middle">deletes in ~dtor</text>
<rect class="se-box" x="24" y="48" width="220" height="58" rx="6"/>
<text class="se-id" x="134" y="72" text-anchor="middle">SEApiServices</text>
<text class="se-sub" x="134" y="90" text-anchor="middle">the service container</text>
<rect class="se-box" x="314" y="48" width="220" height="58" rx="6"/>
<text class="se-id" x="424" y="72" text-anchor="middle">SEBrowserWidgetManager</text>
<text class="se-sub" x="424" y="90" text-anchor="middle">owns the docks</text>
<rect class="se-box" x="604" y="48" width="220" height="58" rx="6"/>
<text class="se-id" x="714" y="72" text-anchor="middle">QDockWidget</text>
<text class="se-sub" x="714" y="90" text-anchor="middle">owns its browser widget</text>
<path class="se-edge" d="M 246,77 H 305 M 536,77 H 595 M 714,106 V 139"/>
<path class="se-head" d="M 314,77 L 305,72 L 305,82 Z M 604,77 L 595,72 L 595,82 Z M 714,148 L 708.5,139 L 719.5,139 Z"/>
<text class="se-note" x="726" y="130">destroys</text>
<rect class="se-box" x="604" y="148" width="220" height="58" rx="6"/>
<text class="se-id" x="714" y="172" text-anchor="middle">SEBrowserWidget</text>
<text class="se-sub" x="714" y="190" text-anchor="middle">one CEF panel</text>
<rect class="se-box" x="314" y="148" width="220" height="58" rx="6"/>
<text class="se-id" x="424" y="172" text-anchor="middle">SEApiMessageHandler</text>
<text class="se-sub" x="424" y="190" text-anchor="middle">the page's window.host</text>
<path class="se-edge" d="M 598,177 H 543"/>
<path class="se-head" d="M 534,177 L 543,172 L 543,182 Z"/>
<text class="se-note" x="569" y="140" text-anchor="middle">holds</text>
<path class="se-edge-weak" d="M 424,206 V 234 H 139 V 115"/>
<path class="se-head-em" d="M 139,106 L 133.5,115 L 144.5,115 Z"/>
<text class="se-note-em" x="156" y="228">weak_ptr&lt;SEApiServices&gt; &mdash; non-owning, locked per call</text>
</svg>
</div>
<figcaption>Make that last edge a <code>shared_ptr</code> and the chain closes: the container cannot release the widget manager until the handler releases the container, which cannot happen until the manager's destructor has deleted the docks. <code>check_nothing_but_the_manager_owns_the_services_container</code> is what keeps it weak.</figcaption>
</figure>

That loop hung OBS: a complete shutdown in the log, process never exited,
`QCoreApplication::exec` still on the main thread's stack. It was broken for a
while by one line in the root's destructor that emptied the container — a
teardown step deciding other objects' lifetimes, which is exactly what the
refactor existed to remove.

**The rule now:** anything whose lifetime is controlled by something *in* the
container — a browser widget, a dialog, an API handler, a background worker —
holds the container through a `weak_ptr` and locks it per use. Nine classes do
this today:

`SEApiMessageHandlerBase`, `SEAuxiliaryControlsBuilder`, `SEBrowserDialog`,
`SEBrowserDialogManager`, `SENativeObsUIControlsManager`,
`SEPopupWindowManager`, `SESessionManager`, `SEWidgetManager`,
`SEWorkerManager`.

`check_nothing_but_the_manager_owns_the_services_container` allows exactly one
strong `shared_ptr<SEApiServices>` member in the tree, in `SEMain.hpp`. It is
the invariant that makes the rings unconstructible, and it replaced a
hand-maintained list of consumers.

Two holders are the deliberate exceptions to "take what you need by
injection", because their dependency set genuinely is most of the surface:
**`SEApiMessageHandler`** and **`SESessionManager`**. Both hold the container
weakly and lock it per call. The alternative for the session manager — a
twelve-parameter constructor rewritten every time signing out grows a step —
is recorded in its own header as the reason.

### Why not weak back-pointers everywhere

Because it costs about 200–300 lines of lock-and-bail boilerplate at ~100
dereference sites, every one of those branches is unreachable except during
teardown and therefore untested by construction, a constructor stops
documenting what a class touches, a missing dependency becomes a runtime no-op
instead of a compile error, and `lock()` is an atomic read-modify-write on one
control block in paths that run per scene item and per output signal. It also
buys no safety that injection does not already provide: injecting the one or
two services a class needs is ring-free, because **no service owns its
consumers**. The widget-owning services are the exception, which is precisely
where the weak edge is worth its cost.

## The teardown audit

`WatchServicesForTeardownAudit()` takes a `weak_ptr` to each of the **39**
published services through `SE_WATCH(field)` / `SE_WATCH_KEPT(field)`, and at
the end of `Shutdown()` — where the container is released — reports which are
gone. `SE_WATCH_KEPT` marks the three the crash handler deliberately keeps, so
they are reported as expected rather than as leaks.

`check_every_published_service_is_watched_at_teardown` matches the watch list
against the container's fields, so adding a service and forgetting to watch it
fails the suite. `check_the_services_are_released_at_the_end_of_shutdown`
pins *where* the audit runs: at the point the container is released, which can
tell a service released on time from one released while the root was being
destroyed.

## Adding a service: the recipe

1. **Write the class.** One class per file, `SE<Thing>.{hpp,cpp}`, forward
   declarations in the header and real includes in the `.cpp` with
   `SEUtils.hpp` first (which is also what keeps the `windows.h` /
   `winsock2.h` include order intact). Model it on
   `streamelements/SEStatePersistence.{hpp,cpp}`.
2. **Take what it needs** as constructor parameters — specific services, not
   the container — unless it is destroyed by something in the container, in
   which case it takes `std::weak_ptr<SEApiServices>` and locks per use,
   logging at `LOG_WARNING` on a failed lock.
3. **Give it an `ISE<Thing>.hpp`** beside the implementation if a container
   consumer will call it. Declare only what they call; keep lifecycle methods
   off it; mark the implementations `override` — all of them.
4. **Add it to `CMakeLists.txt`** by hand, in both the
   `obs-streamelements-core_SOURCES` and `obs-streamelements-core_HEADERS`
   lists. There is no globbing.
5. **Add the field to `SEApiServices`**, minding the destruction-order rules
   above, and a concrete member plus a getter on `SEMain` if the root drives
   its lifecycle.
6. **Construct it in `Initialize()`** after whatever it depends on, assign it
   in `PublishApiServices()`, and add an `SE_WATCH` line.
7. **Run the tests.** Several invariants will tell you if you have broken one
   of the rules above; `check_every_published_service_is_watched_at_teardown`
   in particular.

For a new `window.host` method, see [the host API bridge](host-api-bridge.md)
and use the `/add-host-api` skill.

## The `SEUtils` domains

Free functions and small helpers live under `streamelements/SEUtils/` in 20
domain headers behind a permanent umbrella, `SEUtils.hpp`, so that the ~50
files which include the umbrella do not care how the domains are split:
diagnostics, async tasks, lifecycle gating, strings, crypto, environment
config, paths, system info, HTTP, logging, obs data, obs scenes, obs
encoders, the obs source catalog, overlay URLs, docks, re-entrancy, app
environment, cancelable tasks, and the lazy encoder provider.

The split exists for testability: roughly half of what used to be one
4,784-line translation unit is pure, and was unreachable from a test purely
because of where it sat. Two lessons from doing it are worth keeping — the
blocker on a "pure" cluster is usually a single declaration in the wrong
header rather than the cluster itself, and moving a body out of a
single-translation-unit file loses its `#ifdef WIN32` if you are not careful,
which the Windows build cannot tell you about.

## Related

- [Module lifecycle](module-lifecycle.md) — construction order and the gate.
- [The host API bridge](host-api-bridge.md) — the container's largest consumer.
- [Enforced invariants](invariants.md) — every rule above, with its check.
