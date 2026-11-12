# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

`obs-streamelements-core` is the SE.Live native OBS Studio plugin — C++/Objective-C++ with Qt 6, built as part of an obs-studio tree.

## Documentation

**`docs/architecture/` has to stay true: a change that alters what those pages
describe updates them in the same commit.** That means a renamed or added class,
a new or removed service or interface, a changed construction or teardown order,
a new lock or a widened lock scope, a new `check_*` invariant, a changed entry
point, a new build switch — the chapters name specific classes, counts and
invariant names, so a stale page lies about code somebody is reading right now.
Treat "I renamed something" as a documentation task, not only a code one.

Thirteen chapters, published beside the JS API reference at `/architecture/` by
`.github/workflows/pages.yml`, which only runs on `master`. Build them with
`mkdocs build -f mkdocs.architecture.yml`; it is `strict: true`, so a dead
cross-link or anchor fails the build rather than publishing. Prose wraps at 79
columns. The figures are hand-authored inline SVG drawn in `currentColor` so one
drawing serves the light and the dark palette — the few values that cannot be an
opacity of the page foreground live in `docs/architecture/assets/diagrams.css`.

`docs/api/` is a separate site: it documents the surface a page sees, so a new
or changed `window.host` method needs an edit in both trees.

`docs/analytics/` is the third site, built with `mkdocs build -f
mkdocs.analytics.yml` and published at `/analytics/`. It is the only
description of the analytics pipeline that exists — the 22 event names across
the plug-in, the installer and the uninstaller, and the payload each carries —
so **a new, renamed or removed event, or a changed field, updates it in the
same commit**, including the NSIS side. Its `assets/diagrams.css` is a
byte-for-byte copy of the architecture one, because a MkDocs site can only
reference files inside its own `docs_dir`;
`check_the_diagram_stylesheets_stay_identical` keeps the two in step. Same
79-column prose rule.

## Build

This plugin does **not** build standalone. It must sit at `obs-studio/plugins/obs-streamelements-core` (it already does here), with the sibling `obs-studio/plugins/obs-browser` present — `SEBrowserWidget.cpp` includes `../../obs-browser/panel/browser-panel.hpp` by relative path. Configure and build from the obs-studio root, not from this directory.

- Windows preset: `windows-x64-streamelements`, defined in `CMakeUserPresets.json` (CI copies it to the obs-studio root and deletes it from here).
- `.github/workflows/build.yml` is the only pipeline. It pins OBS to tag `32.2.2` (the `CHECKOUT` env of both the `windows` and `macos` jobs — keep them equal) and passes a `BUILD_OPTIONS` list of `-DENABLE_*=OFF` flags meant to narrow an artifact-only build. Don't reuse those flags locally, and **don't trust them either**: most are no-ops. `ENABLE_UI` exists in no OBS tree (checked at 32.2.1 and 33.0.1), so the frontend is built regardless, and 15 of the 18 flags are reported as "not used by the project" in the Windows job — the plugin ones because that job replaces `plugins/CMakeLists.txt` (below), which is what actually narrows the build. Pre-existing and harmless; fixing it means deciding what CI should build, which is a bigger change than it looks.
- **The OBS pin is a compatibility floor, not "what ships".** libobs rejects a module whose baked `LIBOBS_API_VER` is greater than its own — `obs-module.c`, logged as "compiled with newer libobs" and returning `MODULE_INCOMPATIBLE_VER` before any of our code runs — while a module built against an *older* libobs loads on a newer host fine. The check is one-directional, so **build against the oldest OBS we support.** Pinned at `33.0.1`, `26.11.10.1096` was silently inert on every OBS 32.x install: no crash, no dialog, nothing in our own log. The gate masks the patch byte, so a 32.2.2 build still loads on 32.2.0 and 32.2.1 — which is why `OBS_REQUIRE_VERSION` in `CI/obs-streamelements-installer/common.nsh` may stay at 32.2.0, and why `OBS_DOWNLOAD_VERSION` there must never be older than `CHECKOUT`.
- **Registering this plugin in an OBS tree is version-dependent.** At the pinned 32.2.2 the macro is `add_obs_plugin`, which is what both CI jobs write. OBS 33.0.0 renamed it to **`add_core_module`** and replaced the removed `obs_add_core_module()` runtime call with a list baked at configure time by `set_obs_core_modules()`, which generates `libobs/obs-core-modules.c` — and libobs links it. So on a 33.x tree anything that *replaces* `plugins/CMakeLists.txt` (as the Windows CI job does, to build only libobs and this plugin) must write both lines, or libobs fails on a missing source; appending to upstream's file instead (the macOS job) needs only the rename.
- `streamelements/Version.generated.hpp` must define `STREAMELEMENTS_PLUGIN_VERSION`. A placeholder is committed and CI overwrites it; don't bump it by hand.

**New source files must be added by hand** to the explicit `obs-streamelements-core_SOURCES` / `obs-streamelements-core_HEADERS` lists in `CMakeLists.txt` — there is no globbing. New qrc assets go in both `streamelements/streamelements.qrc` and the `qt6_add_resources(...)` `FILES` list.

**Crash-reporting backend is a build-time choice**: `-DSTREAMELEMENTS_CRASH_HANDLER=bugsplat|sentry|none` (default `bugsplat`). One variable drives which handler compiles, which SDK links, and the `SE_CRASH_HANDLER_*` define the factory in `SECrashHandler.cpp` reads — keep those three together. Only one backend is ever linked, so two can't contend for the process exception filter. sentry-native is vendored as `deps/sentry-native.zip` and extracted by CMake into the *build* tree; there is no unzip step to run. The Sentry DSN is baked into `STREAMELEMENTS_SENTRY_DSN` and is **not** a secret — a DSN only permits submitting events. `SENTRY_AUTH_TOKEN`/`SENTRY_ORG`/`SENTRY_PROJECT` are CI-only, for symbol upload.

**The `string(REPLACE "/MD" "/MT" ...)` block in `CMakeLists.txt` is dead code.** obs-studio sets the runtime through the `MSVC_RUNTIME_LIBRARY` target property, so the flags never contain a literal `/MD` and nothing is replaced. This project is `/MD` (`MultiThreadedDLL`), same as libobs — vendored libraries must match, or the link fails with `LNK4098`, which `/WX` promotes to a hard error.

## Tests

`tests/` holds a small ctest suite, off by default (`-DSE_ENABLE_TESTS=ON`, or configure `tests/` directly). It is **not** part of the plugin build: the tests are standalone C++17 executables that link neither libobs nor Qt, so they build and run without an obs-studio tree — on any platform, in seconds.

```sh
cmake -S tests -B tests/build && cmake --build tests/build
ctest --test-dir tests/build --output-on-failure
```

Two kinds live there, and `se_add_test(<name> <sources>)` in `tests/CMakeLists.txt` registers both:

- **Behavioural** — exercise real logic directly, or a minimal mirror of it where the real thing is entangled with libobs.
- **Source-invariant** — open the production sources and assert a fixed bug pattern is absent, so a regression is caught without needing the OBS build to succeed.

Anything self-contained enough to compile without libobs/Qt belongs here rather than in a throwaway harness — parsers, redaction, string handling, pure helpers. Deliberately keeping a unit under test free of OBS dependencies is a good reason to structure it that way.

CI runs this suite on Windows and macOS (`.github/workflows/tests.yml`), so a red ctest blocks a PR. It is still worth running locally before pushing, and don't call a change verified on the basis of tests that were never run against it.

`CI/check-format.sh` exists but **no workflow invokes it** — formatting is enforced by the pre-commit hook below, not by the pipeline.

## Formatting

`.clang-format` at the root is authoritative: 8-wide tabs (`UseTab: ForContinuationAndIndentation`), 80-column limit, `AccessModifierOffset: -8`, brace on the next line for **function definitions only** (everything else K&R), `PointerAlignment: Right`, `SortIncludes: false`.

Format only the files you edited: `clang-format -i -style=file -fallback-style=none <file>`. **Never run `./formatcode.sh`** — it has no exclusions and would reformat all of `deps/` and `streamelements/deps/`.

**Install the pre-commit hook once per clone:** `git config core.hooksPath .githooks`. It is
not installed automatically, and a clone without it formats nothing.

`.githooks/pre-commit` formats **only the line ranges this commit touches**, then re-stages
them. That restriction is the point: the tree was formatted by older clang-format releases,
so a whole-file run with a newer one rewrites code nobody edited — 19.1.5 reformats 225
lines of `tests/test_source_invariants.cpp` unprompted. Line ranges keep the blast radius
equal to the diff, which is why no clang-format version is pinned.

A file with unstaged changes is never re-staged — that would commit a version nobody
reviewed. The hook reports it and fails instead. `CLANG_FORMAT=<path>` overrides discovery;
`SE_SKIP_FORMAT=1` or `--no-verify` skips it.

## Don't edit

- CMake/MSBuild output at the repo root: `qrc_streamelements.cpp`, `obs-streamelements-core.sln`, `*.vcxproj*`, `cmake_install.cmake`, `CMakeFiles/`, `RelWithDebInfo/`, `*.dir/`, `*_autogen/`. Generated, gitignored, and not build definitions — only `CMakeLists.txt` matters. Note the equivalent `.vcxproj`/`.sln` files under `deps/` and `CI/*/nsis/` are vendored source and *are* tracked.
- `deps/`, `streamelements/deps/` — vendored third-party.
- `data/locale/*.ini` other than `en-US.ini` — translated externally.

## Architecture gotchas

- **`CefRefPtr` / `CefValue` / `CefListValue` here are not CEF.** `deps/cef-stub/` reimplements them (`#define CefRefPtr std::shared_ptr`) as a JSON-ish data model used as the wire format. CEF refcounting and thread-affinity rules do not apply, and real CEF APIs are unavailable.
- **The browser bridge is a WebSocket, not CEF IPC** — `SEWebsocketApiServer` listens on port 27952; page JS receives `window.host.*` from an injected bootstrap script.
- **Host API methods** are registered in `SEApiMessageHandler::RegisterIncomingApiCallHandlers()` via the `API_HANDLER_BEGIN` / `API_HANDLER_END` macros, which are defined once in `SEApiMessageHandlerBase.hpp`. The lock they take is **per translation unit on purpose** — `showModalDialog` holds it across a blocking `exec()`, so the dialog handlers that close that dialog must lock something else. Use `/add-host-api`.
- **A new subsystem becomes a manager on `SEMain`**, which is the root, not a singleton: add `std::shared_ptr<Foo> m_foo`, construct it in `Initialize()` after whatever it depends on, and publish it in `PublishApiServices()` so consumers can be handed it. **There is no `GetInstance()` and no `IsInstanceAvailable()`** (CORE-757) — a class that needs the manager takes it as a constructor parameter. The exceptions are the two whose dependency set genuinely is most of the surface, `SEApiMessageHandler` and `SESessionManager`: they hold `std::weak_ptr<SEApiServices>` and lock it per call. Anything the container's own services destroy — a browser widget, a dialog, an API handler — must hold the container weakly or shutdown stalls on a ring refcounting cannot unwind; `check_nothing_but_the_manager_owns_the_services_container` enforces it.
- **To reach the Qt/UI thread, use the `QtPostTask` / `QtExecSync` / `QtDelayTask` macros** from `SEUtils/SEAsyncTaskUtils.hpp` (or the `SEUtils.hpp` umbrella). They are macros, not functions: they capture `__FILE__`/`__LINE__` into an async-call context the crash handler reads. Don't substitute raw `QMetaObject::invokeMethod`.
- **Naming**: one class per file, file name == class name, `SE<Thing>.hpp` / `.cpp`; members `m_camelCase`, file statics `s_`, globals `g_`.
- **User-visible strings** go through `obs_module_text()`, with keys added to `data/locale/en-US.ini`.

## Git

- Never commit to `master`. Branch and open a PR (`gh pr create`), even for small fixes.
- **Put the Linear issue key in the branch name** (`jacob/core-266-track-selive-releases`). Linear links the PR to the issue from that name, and the `qa -> beta` promotion attaches those issues to the Linear release. master is squash-merged, so a branch without a key leaves the work untraceable — the commit subject keeps only `(#93)`, and that PR links to nothing. `linear-issue-key.yml` enforces this; label a PR `no-linear-issue` if there genuinely isn't one. A key in the PR title or a `Fixes CORE-266` line in the body works too. Anything needing Linear API *write* access — suggesting a key, reopening an issue — runs through the Linear MCP, not CI: a workspace-scoped key in Actions was rejected on security grounds.
- **Put `#partial` in the title or body of a PR that does not finish its issue.** Linear closes an issue when any linked PR merges, which is wrong when the work is split across several PRs — CORE-554 was closed by the first of three. **CI cannot undo this**: it needs a Linear API key, and a workspace-scoped key in Actions was rejected on security grounds. `#partial` is a marker for humans and agents, not automation — `linear-issue-key.yml` only surfaces it on the check. After merging such a PR, reopen the issue via the Linear MCP (`save_issue` with `state: "In Progress"`).
- Conventional Commits: `fix:`, `feat:`, with scopes where they apply (`fix(ci):`, `fix(workflow):`).
- **Never start a commit message with `[WORKFLOW-AUTOMATION]`** — CI uses that prefix to recognize its own commits and skip the build.
- `RELEASE_NOTES.md` must be non-empty for a master build to tag a release. Use `/release-prep`.
