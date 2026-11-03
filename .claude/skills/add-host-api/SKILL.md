---
name: add-host-api
description: Add or modify a window.host.* method exposed to the SE.Live web front end. Use whenever a task involves the host API, the JS bridge, API_HANDLER_BEGIN, SEApiMessageHandler, or making a new native capability callable from page JavaScript.
---

# Adding a host API method

Host API methods are what page JavaScript calls as `window.host.<methodName>(...)`. They are registered in one place and dispatched over a WebSocket (port 27952) — not over CEF IPC.

Target method: `$ARGUMENTS`

## 1. Register the handler

All handlers live in `SEApiMessageHandler::RegisterIncomingApiCallHandlers()` in `streamelements/SEApiMessageHandler.cpp`. Add yours there, in the block with the other `API_HANDLER_BEGIN` entries, grouped near related methods rather than appended at the end. **All of them stay in that one file** — it is the plug-in's public contract, and keeping it whole is a deliberate decision, not an accident.

```cpp
API_HANDLER_BEGIN("myNewMethod");
{
        // result is CefRefPtr<CefValue>& and is pre-set to false by the
        // dispatcher, so `false` is what every refusal below answers.
        if (args->GetSize() < 1 || args->GetType(0) != VTYPE_STRING)
                return;

        std::string id = args->GetString(0).ToString();

        apiServices->someManager->DoTheThing(id, result);
}
API_HANDLER_END();
```

Names are camelCase string literals and must be unique — the registry is a `std::map<std::string, incoming_call_handler_t>`, so a duplicate silently replaces the earlier entry. Grep for the name before adding it.

Variables the macro puts in scope: `self`, **`apiServices`**, `message`, `args` (`CefRefPtr<CefListValue>`), `result` (`CefRefPtr<CefValue>&`), `target` (`std::shared_ptr<SEApiClientInfo>`), `cefClientId`, `complete_callback`. The macros are defined once, in `streamelements/SEApiMessageHandlerBase.hpp`.

## 2. Type-check every argument before reading it — this is not optional

Every typed getter on `CefValueImpl` is `if (m_type != VTYPE_X) throw;`, and a bare `throw` outside a catch calls `std::terminate`. So **an unchecked read of a page-supplied value is a process kill any page can trigger with one wrong argument.** Seven such handlers shipped before the gates below existed; `window.host.removeDockingWidgetsByIds("a")` was enough to take OBS down.

Three traps, all of which have bitten:

- **A size test is not a type test.** `args->GetSize() > 0` then `args->GetValue(0)->GetInt()` is a kill.
- **`HasKey` is not a type test.** `d->HasKey("url") ? d->GetString("url") : ""` kills on `{url: 7}`, because `CefDictionaryValueImpl::GetString(k)` lands on the same throwing getter. 51 of the 60 unguarded reads found in the last sweep were this shape — a `HasKey` test *looks* like diligence.
- **List elements count too.** `list->GetValue(i)->GetDictionary()` needs the element checked, not just the list.

Check the type, and on refusal just `return` — the pre-set `false` is the answer. For an optional field, keep the default instead of failing the whole call.

**If you hand a raw page value to a service** (`service->Foo(args->GetValue(0), result)`), the type check belongs **in the service**, which is where the other ~105 argument positions already do it. That protects non-handler callers too.

Two source invariants enforce this and will fail the suite:
`check_handler_bodies_type_check_before_they_read` and
`check_services_type_check_what_the_handlers_hand_them`.

## 3. Sync vs. async

- `API_HANDLER_END()` calls `complete_callback()` for you. Use it when the result is ready when the block returns.
- `API_HANDLER_END_ASYNC()` does not. Use it when you hand work off and **you** must call `complete_callback()` on every path afterwards, including error and early-return paths. A missed call leaves the JS promise pending for ever — `showNonModalDialog` is the only user of it in the file, and it shipped with exactly that bug on its no-arguments path.

**`API_HANDLER_BEGIN` takes a `std::recursive_mutex s_sync_api_call_mutex` that is per translation unit, deliberately.** The macro names it unqualified and each TU that expands the macro defines its own (`SEApiMessageHandler.cpp`, `SEDialogApiMessageHandler.cpp`). This is load-bearing: `showModalDialog` holds its TU's lock for the entire lifetime of a modal dialog, and the only thing that closes that dialog is `endModalDialog` in the *other* TU. Unify them and every modal dialog deadlocks on its own exit path — being recursive does not help, the two run on different threads. `check_the_api_call_lock_stays_per_translation_unit` pins this.

Don't do long or blocking work inside a synchronous handler: it stalls every other API call in that TU. `httpRequestText` handing `""` to libcurl wedged the whole host API for the life of the process. Hand off with `QtPostTask` and use `API_HANDLER_END_ASYNC()`.

## 4. Reaching a subsystem

**Through `apiServices`, which the macro puts in scope. There is no `GetInstance()` and no `IsInstanceAvailable()` — CORE-757 removed them.**

```cpp
apiServices->profilesManager->SerializeAllProfiles(result);
```

The dispatcher locks the services container once per dispatch and bails before the handler runs if it is gone, so a body never checks liveness. The container publishes 33 `ISE*` interfaces; the concrete class is reached only by the root.

**If the method you need doesn't exist on the service yet**, add it to the `ISE*` interface, not just the concrete class — a pure virtual makes the *vtable* demand it, so `tests/fake_api_services.{hpp,cpp}` (the interface fakes) or `tests/stub_concrete_services.cpp` (the services still carried concretely) must gain it too, or the test build fails to link. Note the asymmetry: the compiler enforces completeness for an interface fake, while a missing stub for a concrete service only shows up as a link error once something calls it.

**The handler file must stay free of libobs and Qt.** `check_handler_include_closure_only_goes_down` is pinned at **0 obs / 0 Qt headers**, which is what lets the whole translation unit compile in `tests/`. So no `obs_*`, no `Q*` types, no `blog` in a handler body — route it through a service, or through an `SEUtils` domain header that declares no obs/Qt type. To reach the Qt/UI thread use the `QtPostTask` / `QtExecSync` / `QtDelayTask` macros from `SEAsyncTaskUtils.hpp`; they capture `__FILE__`/`__LINE__` for the crash handler, so don't substitute raw `QMetaObject::invokeMethod`.

## 5. Serialization helpers

Convert between native objects and `CefValue` with the existing `Serialize*` / `Deserialize*` helpers. `SEUtils` is split by domain under `streamelements/SEUtils/` behind an umbrella `SEUtils.hpp`; pick the specific domain header, and remember that the obs-flavoured ones are off-limits here (see above). Add a matching pair rather than hand-rolling dictionary building inside the handler, and keep the two symmetrical.

`CefRefPtr` is `std::shared_ptr` here (`deps/cef-stub/`) and the value tree is a local JSON-ish data model — there is no CEF refcounting or thread affinity to respect, and no real CEF API available.

## 6. Tests — a new method fails the suite until you describe it

The old version of this skill said there are no tests. There are, and they are not optional:

- **`tests/test_api_handler_contract.cpp` has a completeness check**: every registered name must appear in one of its tables or on a short pinned list. A new method **fails ctest** until you add a row saying what it does — the reply, the ordered service calls, and the arguments those calls receive.
- **The surface count is asserted**: `tests/test_api_handler_bodies.cpp` expects **202** registered names on Windows and **201** elsewhere (208 `API_HANDLER_BEGIN` blocks, less those compiled out behind `SE_ENABLE_WYVRN`, `SE_ENABLE_CENTRAL_WIDGET_DECORATIONS` and `WIN32`, plus the hand-written `batchInvokeSeries`), and `tests/test_api_handler_optional.cpp` expects **209** with the optional features on. Adding a method means bumping both, and keeping the platform split right — hard-coding one number passed on Windows and failed on macOS the first time it ran there.
- A sweep calls every registered method with no arguments and requires each to answer exactly once. That is what catches a missing `complete_callback()` and the unchecked reads from section 2.

```sh
cmake -S tests -B tests/build && cmake --build tests/build --config Debug
ctest --test-dir tests/build -C Debug --output-on-failure
```

Always build `tests/` before running ctest: a stale binary has reported both a false pass and a false failure in this repo.

## 7. Bump the API version

In `streamelements/Version.hpp`:

- New method, new optional argument, or a bugfix to existing behaviour → increment `HOST_API_VERSION_MINOR`.
- Changed or removed signature, changed return shape → increment `HOST_API_VERSION_MAJOR` and reset the minor to 0.

The web front end reads this to decide what it can call, so skipping the bump ships a method the page will never invoke.

## 8. Finish

- New files (rare for this) go in the explicit `_SOURCES` / `_HEADERS` lists in `CMakeLists.txt` — there is no globbing.
- User-visible strings go through `obs_module_text()`, with keys added to `data/locale/en-US.ini`.
- Format only what you touched: `clang-format -i -style=file -fallback-style=none streamelements/SEApiMessageHandler.cpp`. **Never run `./formatcode.sh`.**
- Verify live if you can: launch with `--remote-debugging-port=9222`, attach to the SE.Live panel over CDP with `suppress_origin=True`, and **poll `Object.keys(window.host).length` until it reaches 210** before probing — the shim creates `window.host` with one key and fills the methods in asynchronously, so an early probe reports every method missing. Race each call against a timer: a probe that simply awaits a callback cannot tell "answered false" from "never answered".
