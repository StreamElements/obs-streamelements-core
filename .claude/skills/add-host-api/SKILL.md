---
name: add-host-api
description: Add or modify a window.host.* method exposed to the SE.Live web front end. Use whenever a task involves the host API, the JS bridge, API_HANDLER_BEGIN, StreamElementsApiMessageHandler, or making a new native capability callable from page JavaScript.
---

# Adding a host API method

Host API methods are what page JavaScript calls as `window.host.<methodName>(...)`. They are registered in one place and dispatched over a WebSocket (port 27952) — not over CEF IPC.

Target method: `$ARGUMENTS`

## 1. Register the handler

All handlers live in `StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlers()` in `streamelements/StreamElementsApiMessageHandler.cpp`. Add yours there, in the block with the other `API_HANDLER_BEGIN` entries, grouped near related methods rather than appended at the end.

```cpp
API_HANDLER_BEGIN("myNewMethod");
{
        // result is CefRefPtr<CefValue>& — always set it on every path
        result->SetNull();

        if (args->GetSize() < 1 || args->GetType(0) != VTYPE_STRING)
                return;

        std::string id = args->GetString(0).ToString();
        // ...
        result->SetBool(true);
}
API_HANDLER_END();
```

Names are camelCase string literals and must be unique — the registry is a `std::map<std::string, incoming_call_handler_t>`, so a duplicate silently replaces the earlier entry. Grep for the name before adding it.

Variables the macro puts in scope: `self`, `message`, `args` (`CefRefPtr<CefListValue>`), `result` (`CefRefPtr<CefValue>&`), `target` (the calling client), `cefClientId`, `complete_callback`.

## 2. Sync vs. async

- `API_HANDLER_END()` calls `complete_callback()` for you. Use it when the result is ready when the block returns.
- `API_HANDLER_END_ASYNC()` does not. Use it when you hand work off (a Qt task, a network call, an OBS event) and **you** must call `complete_callback()` on every path afterwards, including error and early-return paths. A missed call leaves the JS promise pending forever.

`API_HANDLER_BEGIN` takes a global `std::recursive_mutex s_sync_api_call_mutex` for the whole body, so all API calls serialize against each other. Don't do long or blocking work inside a synchronous handler — it stalls every other API call. Hand off with `QtPostTask` and use `API_HANDLER_END_ASYNC()` instead.

Note the recursive mutex: a handler that synchronously invokes another handler on the same thread will not deadlock, but re-entrancy is real — don't rely on state being untouched across a nested call.

## 3. Touching OBS or Qt state

- UI/Qt work must go through `QtPostTask` / `QtExecSync` / `QtDelayTask` from `StreamElementsUtils.hpp`. They are macros that capture `__FILE__`/`__LINE__` for the crash handler — don't replace them with `QMetaObject::invokeMethod`.
- Reaching a subsystem goes through the singleton: `StreamElementsGlobalStateManager::GetInstance()->GetFooManager()`. Check `StreamElementsGlobalStateManager::IsInstanceAvailable()` first when the handler can run during shutdown.

## 4. Serialization helpers

Convert between OBS/native objects and `CefValue` with the existing `Serialize*` / `Deserialize*` helpers in `streamelements/StreamElementsUtils.hpp`. Add a matching pair there rather than hand-rolling dictionary building inside the handler, and keep the two symmetrical.

Remember that `CefRefPtr` is `std::shared_ptr` here (`deps/cef-stub/cef_value.hpp`) and this whole value tree is a local JSON-ish data model — there is no CEF refcounting or thread affinity to respect.

## 5. Bump the API version

In `streamelements/Version.hpp`:

- New method, new optional argument, or a bugfix to existing behavior → increment `HOST_API_VERSION_MINOR`.
- Changed or removed signature, changed return shape → increment `HOST_API_VERSION_MAJOR` and reset the minor to 0.

The web front end reads this to decide what it can call, so skipping the bump ships a method the page will never invoke.

## 6. Finish

- If you added a new file (rare for this — most methods go in the existing handler), add it to the explicit `_SOURCES` / `_HEADERS` lists in `CMakeLists.txt`.
- Run `clang-format -i -style=file -fallback-style=none streamelements/StreamElementsApiMessageHandler.cpp` (and any other file you touched). Never run `./formatcode.sh`.
- There are no tests. State plainly that the change is unverified beyond compiling, and mention the JS-side call the front end would need to make.
