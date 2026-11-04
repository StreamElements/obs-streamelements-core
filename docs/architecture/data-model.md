# The data model

Everything that crosses the boundary between page JavaScript and native code
is a `CefValue` tree. The name is misleading and the misunderstanding is
expensive, so this page is short and blunt.

## `CefRefPtr` here is `std::shared_ptr`

`deps/cef-stub/` reimplements the CEF value types as a plain, JSON-ish data
model used as the wire format:

```cpp
#define CefRefPtr std::shared_ptr
```

So:

- **CEF's refcounting rules do not apply.** There is no `AddRef`/`Release`
  dance, no `CefRefPtr<T>` ownership subtleties.
- **CEF's thread-affinity rules do not apply.** These objects are not bound to
  a renderer or browser thread; they are ordinary heap objects behind a
  `shared_ptr`.
- **Real CEF APIs are unavailable.** Anything from the actual CEF headers
  simply is not there.

The files are `cef_value.{hpp,cpp}`, `cef_value_list.cpp`,
`cef_value_dictionary.cpp`, `cef_value_json.cpp`, `cef_value_binary.cpp`,
`cef_process_message.{hpp,cpp}` and `cef_string.{hpp,cpp}`. `cef-headers.hpp`
at the repository root is the include anyone else uses.

!!! note "`cef-headers.hpp` pulls in more than it looks like"
    It includes the local HTTP server — cpp-httplib, with openssl/zlib/brotli
    behind macros that happen to be undefined — for an include-ordering
    reason ("Must include before `<windows.h>`"). Every file that wants the
    value model gets that too. Recorded rather than fixed.

## Typed getters throw, and a bare `throw` terminates

```cpp
CefString CefValueImpl::GetString() {
    if (m_type != VTYPE_STRING) throw;    // ← no exception object
    …
}
```

Every typed getter is shaped like that, for all seven types, and a bare `throw`
outside a `catch` calls `std::terminate`. `CefListValue` indexing is an
unchecked `std::vector` subscript on top of that.

This is the single most important property of the data model, because it turns
a type mismatch into a process kill rather than an error. The defence is not in
the data model — it is the two gates described in
[the host API bridge](host-api-bridge.md), which require a type check before
every read of a page-supplied value, in the handler **and** in any service the
handler hands it to. `HasKey` is not a type check; `!value.get()` is not a type
check.

`CefParseJSON` returns null on bad input rather than throwing, and
`check_cefparsejson_results_are_guarded_everywhere` requires every result to be
guarded.

## Serialization conventions

Conversion between native/OBS objects and `CefValue` goes through paired
`Serialize*` / `Deserialize*` helpers, kept symmetrical, in the `SEUtils`
domains — `SEObsDataUtils` for obs data/settings/properties,
`SEObsSceneUtils` for scene traversal, `SEObsEncoderUtils` for encoders and
transitions, `SEObsSourceCatalogUtils` for "what can OBS create and what
already exists". A new pair belongs in the matching domain rather than inline
in a handler body.

Two conventions on the wire, both visible to pages and therefore effectively
frozen:

- **A refusal is `false`.** The dispatcher pre-sets `result` to `false`, so
  every body that returns without setting it refuses in the same way. A body
  that wants `null` must say `result->SetNull()` — and the surface is not
  entirely consistent about this, which is documented rather than changed.
- **Collections keyed by id come back as dictionaries, not lists.**
  `getAllDockingWidgets` and `getAllBackgroundWorkers` answer objects keyed by
  id. Probing them as arrays is a classic harness mistake.

One deleted helper is worth remembering as a lesson rather than a fact:
`IsCefValueEqual()` serialised its first argument twice and compared the result
with itself, so it answered true for every pair. Nothing called it, and
`CefValue::IsEqual()` in the stub already does the job recursively.

## Related

- [The host API bridge](host-api-bridge.md) — where these values come from and
  what must be checked before reading them.
- The
  [data structures section](https://streamelements.github.io/obs-streamelements-core/api/types/)
  of the API reference — the page-facing shapes these values carry.
