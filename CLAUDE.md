# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`obs-streamelements-core` is the SE.Live native OBS Studio plugin — C++/Objective-C++ with Qt 6, built as part of an obs-studio tree.

## Build

This plugin does **not** build standalone. It must sit at `obs-studio/plugins/obs-streamelements-core` (it already does here), with the sibling `obs-studio/plugins/obs-browser` present — `StreamElementsBrowserWidget.cpp` includes `../../obs-browser/panel/browser-panel.hpp` by relative path. Configure and build from the obs-studio root, not from this directory.

- Windows preset: `windows-x64-streamelements`, defined in `CMakeUserPresets.json` (CI copies it to the obs-studio root and deletes it from here).
- `.github/workflows/build.yml` is the only pipeline. It pins OBS to tag `31.1.2` and configures with `-DENABLE_UI=OFF` plus most other OBS features off — those flags are for artifact-only builds, don't reuse them locally.
- `streamelements/Version.generated.hpp` must define `STREAMELEMENTS_PLUGIN_VERSION`. A placeholder is committed and CI overwrites it; don't bump it by hand.

**New source files must be added by hand** to the explicit `obs-streamelements-core_SOURCES` / `obs-streamelements-core_HEADERS` lists in `CMakeLists.txt` — there is no globbing. New qrc assets go in both `streamelements/streamelements.qrc` and the `qt6_add_resources(...)` `FILES` list.

## Tests

There are none — no ctest, gtest, or catch2 anywhere in project code. The only gates are that it compiles and that `CI/check-format.sh` passes. Don't describe a change as verified on the basis of tests.

## Formatting

`.clang-format` at the root is authoritative: 8-wide tabs (`UseTab: ForContinuationAndIndentation`), 80-column limit, `AccessModifierOffset: -8`, brace on the next line for **function definitions only** (everything else K&R), `PointerAlignment: Right`, `SortIncludes: false`.

Format only the files you edited: `clang-format -i -style=file -fallback-style=none <file>`. **Never run `./formatcode.sh`** — it has no exclusions and would reformat all of `deps/` and `streamelements/deps/`.

## Don't edit

- CMake/MSBuild output at the repo root: `qrc_streamelements.cpp`, `obs-streamelements-core.sln`, `*.vcxproj*`, `cmake_install.cmake`, `CMakeFiles/`, `RelWithDebInfo/`, `*.dir/`, `*_autogen/`. Generated, gitignored, and not build definitions — only `CMakeLists.txt` matters. Note the equivalent `.vcxproj`/`.sln` files under `deps/` and `CI/*/nsis/` are vendored source and *are* tracked.
- `deps/`, `streamelements/deps/` — vendored third-party.
- `data/locale/*.ini` other than `en-US.ini` — translated externally.

## Architecture gotchas

- **`CefRefPtr` / `CefValue` / `CefListValue` here are not CEF.** `deps/cef-stub/` reimplements them (`#define CefRefPtr std::shared_ptr`) as a JSON-ish data model used as the wire format. CEF refcounting and thread-affinity rules do not apply, and real CEF APIs are unavailable.
- **The browser bridge is a WebSocket, not CEF IPC** — `StreamElementsWebsocketApiServer` listens on port 27952; page JS receives `window.host.*` from an injected bootstrap script.
- **Host API methods** are registered in `StreamElementsApiMessageHandler::RegisterIncomingApiCallHandlers()` via the `API_HANDLER_BEGIN` / `API_HANDLER_END` macros. Use `/add-host-api`.
- **A new subsystem becomes a manager on `StreamElementsGlobalStateManager`** (the singleton god object): add `std::shared_ptr<Foo> m_foo` plus a `GetFoo()` accessor, and wire it into `Initialize()` and the destructor. Guard access with `StreamElementsGlobalStateManager::IsInstanceAvailable()` — the instance is gone during shutdown.
- **To reach the Qt/UI thread, use the `QtPostTask` / `QtExecSync` / `QtDelayTask` macros** from `StreamElementsUtils.hpp`. They are macros, not functions: they capture `__FILE__`/`__LINE__` into an async-call context the crash handler reads. Don't substitute raw `QMetaObject::invokeMethod`.
- **Naming**: one class per file, file name == class name, `StreamElements<Thing>.hpp` / `.cpp`; members `m_camelCase`, file statics `s_`, globals `g_`.
- **User-visible strings** go through `obs_module_text()`, with keys added to `data/locale/en-US.ini`.

## Git

- Never commit to `master`. Branch and open a PR (`gh pr create`), even for small fixes.
- **Put the Linear issue key in the branch name** (`jacob/core-266-track-selive-releases`). Linear links the PR to the issue from that name, and the `qa -> beta` promotion attaches those issues to the Linear release. master is squash-merged, so a branch without a key leaves the work untraceable — the commit subject keeps only `(#93)`, and that PR links to nothing. `linear-issue-key.yml` enforces this and will suggest an issue when it fails; label a PR `no-linear-issue` if there genuinely isn't one. A key in the PR title or a `Fixes CORE-266` line in the body works too.
- Conventional Commits: `fix:`, `feat:`, with scopes where they apply (`fix(ci):`, `fix(workflow):`).
- **Never start a commit message with `[WORKFLOW-AUTOMATION]`** — CI uses that prefix to recognize its own commits and skip the build.
- `RELEASE_NOTES.md` must be non-empty for a master build to tag a release. Use `/release-prep`.
