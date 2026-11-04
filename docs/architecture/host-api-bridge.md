# The host API bridge

Everything a page can ask OBS to do arrives through this path. It is the
plug-in's public contract, and the
[JavaScript OBS API reference](https://streamelements.github.io/obs-streamelements-core/api/)
documents the page-facing half of it. This page is the native half.

## It is a WebSocket, not CEF IPC

`SEWebsocketApiServer` listens on **port 27952**. Page JavaScript receives
`window.host` from a bootstrap script injected into our own `QCefWidget`
panels; calls and replies travel over that socket as JSON.

Two consequences that catch people out:

- **`CefRefPtr`, `CefValue` and `CefListValue` here are not CEF.** They are
  reimplemented in `deps/cef-stub/` as a JSON-ish data model.
  See [The data model](data-model.md).
- **Browser *sources* are not part of this.** The shim is injected only into
  the panels the plug-in creates; `obs-browser` is not ours to change.
  `DEST_BROWSER_SOURCE` on `ISEMessageBus` is left over from when they were.

## The handshake that builds `window.host`

`SEApiMessageHandlerBase::OnProcessMessageReceived` handles two inbound message
names and sends three outbound:

| direction | message |
| --- | --- |
| in | `CefRenderProcessHandler::OnContextCreated` |
| in | `SEApiMessageHandler::OnIncomingApiCall` |
| out | `CefRenderProcessHandler::BindJavaScriptFunctions` |
| out | `CefRenderProcessHandler::BindJavaScriptProperties` |
| out | the `hostReady` event |

On `OnContextCreated`, and only then, the handler registers its API call
handlers, sends the function and property bindings, and dispatches host-ready.
Three things about that are worth knowing:

1. **It is gated.** `IsPluginInitialized()` is `IsObsInitialized()` **and** a
   successful lock of the container **and** `services->ready`. A context
   created before the gate opens is answered `true` — handled — with nothing
   registered, so the page gets **no `window.host` at all for the life of that
   context**, and nothing retries. That is why the gate is opened before the
   first browser widget exists; see [Module lifecycle](module-lifecycle.md).
2. **Registration is lazy and once per page context**, not once per process.
3. **The object exists before it is usable.** The shim creates `window.host`
   synchronously with its own `endpoint` key and the methods arrive over the
   socket afterwards — a window of roughly 1.3 s in which every call is
   `TypeError: window.host.<m> is not a function`. Any harness that waits for
   the object rather than the method set will test nothing; see
   [Testing](testing.md).

An incoming call carries a header, the method name as `window.host.XXX` (the
handler takes the substring after the last `.`), and JSON-encoded arguments,
each parsed with `CefParseJSON` and replaced by a null value if parsing fails.

## Registration: `API_HANDLER_BEGIN` / `API_HANDLER_END`

The macros are defined **once**, in `SEApiMessageHandlerBase.hpp`, and
expanded in every translation unit that registers methods.
`SEApiMessageHandler.cpp` holds **208** blocks plus one hand-written
`RegisterIncomingApiCallHandler` (`batchInvokeSeries`), all inside
`RegisterIncomingApiCallHandlers()`.

```cpp
API_HANDLER_BEGIN("myNewMethod");
{
        if (args->GetSize() < 1 || args->GetType(0) != VTYPE_STRING)
                return;                 // result is already false

        apiServices->obsSceneManager->DoSomething(args->GetString(0));
        result->SetBool(true);
}
API_HANDLER_END();
```

Variables the macro puts in scope: `self`, **`apiServices`**, `message`,
`args`, `result`, `target`, `cefClientId`, `complete_callback`.

- Names are camelCase string literals and **must be unique**: the registry is a
  `std::map`, so a duplicate silently replaces the earlier entry and the
  method count is the only symptom. A test asserts both the exact count and
  uniqueness.
- `API_HANDLER_END()` calls `complete_callback()` for you.
  `API_HANDLER_END_ASYNC()` does not — the body must call it on **every** path,
  error paths included, or the page's promise never settles. There is exactly
  one async body in the file today (`showNonModalDialog`), and getting this
  wrong is invisible in review and indistinguishable from a slow call at
  runtime.
- **The dispatcher pre-sets `result` to `false`.** So `false` is every refusal
  across the whole surface, and a body that wants `null` has to say so.
- `self->m_services` is not how a body reaches a service. The dispatcher locks
  the container **once per dispatch**, bails with a null result if it cannot,
  and passes the locked `shared_ptr` in as `apiServices`
  (`check_api_handler_locks_the_services_once_per_dispatch`). A handler body
  can therefore be invoked in a test with a hand-built bundle and no root
  anywhere.

## Two lock domains, on purpose

`API_HANDLER_BEGIN` takes `s_sync_api_call_mutex` — a
`static std::recursive_mutex` that **each expanding translation unit defines
for itself**. This looks like an accident and is not:

`showModalDialog`'s body calls `DeserializeModalDialog`, which calls
`dialog->exec()` synchronously, so it holds its TU's lock *for the entire
lifetime of the modal dialog*. The only way that dialog ever closes is its own
`endModalDialog` / `endDialog` handler, which lives in
`SEDialogApiMessageHandler.cpp` and defines its own lock. Put both in one
domain and the exit path blocks on the
lock its own opener holds, for ever — the mutex being recursive does not help,
because the two run on different threads.

So a TU that expands these macros must define `s_sync_api_call_mutex` itself;
forgetting is a compile error, which is the intended outcome.
`check_the_api_call_lock_stays_per_translation_unit` is what stops the two
definitions being "simplified" into one.

A side effect nobody intended: the 208 lambdas are *compiled* in
`SEApiMessageHandler.cpp`, so even when they are registered into a dialog
handler's map they lock that TU's mutex while the dialog's own three lock the
dialog's. **The lock domain a handler gets depends on which TU its macro was
expanded in.**

The cost of the design, stated plainly: a page can hold its TU's lock for as
long as any single synchronous handler takes, and every other call in that
domain waits. One unbounded case has been removed — `httpRequestText` with no
usable URL used to hand `""` to libcurl and never return, which left the whole
host API unanswerable for the life of the process — but a slow handler is
still a slow domain. Long or blocking work belongs behind `QtPostTask` and
`API_HANDLER_END_ASYNC()`.

## Arguments are hostile input

Every typed getter on `CefValueImpl` is `if (m_type != VTYPE_X) throw;`, and a
bare `throw` outside a `catch` calls `std::terminate`. `CefListValue::GetValue`
is an unchecked `std::vector` subscript. So **one wrongly typed argument from
any page used to be able to take the process down**, and six handlers could be
killed that way (`window.host.removeDockingWidgetsByIds("a")` was enough).

Two gates now hold the whole surface:

- **`check_handler_bodies_type_check_before_they_read`** — every body must
  check a value's type before reading it, in all four spellings a read takes
  here (`args->GetX(i)`, `args->GetValue(i)->GetX()`, a dictionary member, a
  list element). The fourth rule exists because a mutation escaped the first
  three.
- **`check_services_type_check_what_the_handlers_hand_them`** — a body that
  passes a raw page value *to a service* is not safe either; the service reads
  it through the same throwing getter. This resolves each handler-to-service
  argument position from the call sites (90 of them) and requires the
  receiving parameter, its members and its list elements to be checked before
  use. 14 service methods were unguarded when it was written, 60 reads in all.

Two traps that gate found and are worth repeating: **`HasKey` is not a type
check** — `GetString(k)` on a present-but-wrongly-typed member lands on the
same throwing getter, which is where 51 of those 60 reads were — and
`if (!input.get())` is a null check, not a type check, because a page's `"x"`
arrives as a perfectly non-null `VTYPE_STRING`.

The convention for *what* to answer: a required argument of the wrong type
refuses the call (`false`); an optional one keeps its default; a wrongly typed
element of a list is skipped rather than failing the batch.

## Infrastructure versus bodies

`SEApiMessageHandlerBase` holds the infrastructure — the registry, the
dispatcher, the recursive lock, async completion, the API-context stack,
`Clone()`, `OnProcessMessageReceived` — and reaches **no obs and no Qt header
at all**. `RegisterIncomingApiCallHandlers()` is pure virtual, so the
infrastructure cannot be instantiated on its own.

`SEApiMessageHandler` derives from it and carries the 208 blocks.
`SEDialogApiMessageHandler` derives again for the three methods a page inside
an SE.Live dialog uses to close itself — `endModalDialog`,
`endNonModalDialog`, `endDialog` — whose shared body lives in
`SEDialogApiCall` behind an interface that names no Qt type, so it can be
asserted against a recording dialog. They were three drifting copies of one
body registered in a `QDialog` translation unit that no ratchet read. Both the
base and the bodies are compiled by the test suite — see
[Testing](testing.md).

## Events out to pages

Host events travel the other way through **`ISEHostEventDispatcher`**, which
every service that announces a `host*` event holds, rather than reaching for
the server. `ISEMessageBus` carries page-to-page and external traffic with
destination flags (`DEST_ALL_LOCAL`, `DEST_ALL_EXTERNAL`, …) that live **on the
interface** as `static constexpr`, so a caller naming a destination is naming
it on the bus's contract.

Worth knowing when testing: `broadcastEvent` answers `true` whether or not the
flags are right, so a return code proves nothing about delivery. Local
listeners receive `DEST_ALL_LOCAL & ~DEST_BROWSER_SOURCE` under the fixed name
`hostEventReceived`; external listeners get `DEST_ALL_EXTERNAL`, only the
`data`, and the caller's own event name.

## Related

- [The data model](data-model.md) — what `CefValue` is here.
- [Services and injection](services.md) — what `apiServices->` reaches.
- [Threading and locks](threading.md) — which thread a body runs on.
- The `/add-host-api` skill — the step-by-step for adding a method, including
  the API version bump in `streamelements/Version.hpp`.
