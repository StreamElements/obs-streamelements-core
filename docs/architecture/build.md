# Build

## The plug-in does not build standalone

It must sit at `obs-studio/plugins/obs-streamelements-core`, with the sibling
`obs-studio/plugins/obs-browser` present — `SEBrowserWidget.cpp` includes
`../../obs-browser/panel/browser-panel.hpp` by relative path. **Configure and
build from the obs-studio root**, not from this directory.

On Windows the preset is `windows-x64-streamelements`, defined in
`CMakeUserPresets.json` here (CI copies it to the obs-studio root and deletes
it from here).

## Registering the plug-in in an OBS tree is version-dependent

This is the single most common build failure, and it is in the *host* tree, not
in this repository.

At the pinned tag — 32.2.2 — the macro is **`add_obs_plugin`**, and that is
what both CI jobs write. The spelling tracks the pin, so it changes if the pin
does.

**OBS 33.0.0 renamed `add_obs_plugin` to `add_core_module`**, and replaced the
removed `obs_add_core_module()` runtime call with a list baked at configure
time by `set_obs_core_modules()`, which generates `libobs/obs-core-modules.c`
— and libobs links it.

So on a 33.x tree anything that **replaces** `plugins/CMakeLists.txt` — as
the Windows CI job does, to build only libobs and this plug-in — must write
**both** lines, or libobs fails on a missing source file, with an error that
has nothing to do with this plug-in. Appending to upstream's file instead (what
the macOS job does) needs only the rename. `set_obs_core_modules()` also
appears twice in upstream's file; the other is the early return under
`if(NOT ENABLE_CORE_MODULES)`, so an insertion has to target the last one.

## The pin is a compatibility floor, not "the version that ships"

libobs rejects a module whose baked `LIBOBS_API_VER` is greater than its own.
`OBS_DECLARE_MODULE()` in `obs-streamelements-core-plugin.cpp` exports
`obs_module_ver()` returning `LIBOBS_API_VER` as it stood in the headers the
DLL compiled against, and `obs_open_module` compares it:

```c
/* Reject plugins compiled with a newer libobs. Patch version (lower 16-bit) is ignored. */
uint32_t ver = mod.ver ? mod.ver() & 0xFFFF0000 : 0;
if (ver > LIBOBS_API_VER) {
        blog(LOG_WARNING, "Module '%s' compiled with newer libobs %d.%d", ...);
        return MODULE_INCOMPATIBLE_VER;
}
```

That returns before `load_state = OBS_MODULE_ENABLED`, so **none** of the
plug-in runs: no crash, no dialog, no line in our own log, and the module is
simply absent from the host log's `Loaded Modules` list. The only trace is that
one `LOG_WARNING` from libobs, and scenes referencing our source types then log
`Source ID '...' not found`.

The comparison is one-directional — a module built against an older libobs
loads on a newer host without complaint — so the pin has to be the **oldest**
OBS we support, never the newest. `26.11.10.1096` was built against 33.0.1 and
was therefore inert on every OBS 32.x install.

Two consequences worth keeping together:

- The patch byte is masked, so a 32.2.2 build still loads on 32.2.0 and
  32.2.1. `OBS_REQUIRE_VERSION` in `CI/obs-streamelements-installer/common.nsh`
  may sit below the pin inside one `major.minor`, and must be raised with it
  across one.
- `OBS_DOWNLOAD_VERSION` in that same file is the OBS the installer fetches
  when none is present, so it must never be older than `CHECKOUT` — otherwise
  a fresh install provisions a host that cannot load the payload the installer
  is about to write.

The only obs-provided helpers this repository's `CMakeLists.txt` calls are
`set_target_properties_obs` and `find_qt`; both exist at 32.2.1 and 33.0.1.

## Switches

| variable | meaning |
| --- | --- |
| `STREAMELEMENTS_CRASH_HANDLER` | `bugsplat` (default), `sentry` or `none`. Drives which handler compiles, which SDK links, and the `SE_CRASH_HANDLER_*` define the factory reads — keep the three together |
| `STREAMELEMENTS_ENABLE_WYVRN` | the Razer WYVRN integration, `ON` by default; defines `SE_ENABLE_WYVRN` |
| `SE_ENABLE_TESTS` | build the standalone ctest suite, `OFF` by default |
| `SE_ENABLE_CENTRAL_WIDGET_DECORATIONS` | `0`; compiles out the preview decorations and four host API methods. Defined on `ISENativeObsUIControlsManager.hpp` so all three guarded regions see it |

sentry-native is vendored as `deps/sentry-native.zip` and extracted by CMake
into the **build** tree; there is no unzip step to run. If
`STREAMELEMENTS_CRASH_HANDLER=sentry` and `STREAMELEMENTS_SENTRY_DSN` is empty,
CMake warns that crash reporting will be inert.

**`streamelements/Version.generated.hpp` must define
`STREAMELEMENTS_PLUGIN_VERSION`.** A placeholder is committed and CI overwrites
it — do not bump it by hand.

## Adding files

**New source files must be added by hand** to the explicit
`obs-streamelements-core_SOURCES` (113 entries) and
`obs-streamelements-core_HEADERS` (195 entries) lists in `CMakeLists.txt`.
There is no globbing. New qrc assets go in **both**
`streamelements/streamelements.qrc` and the `qt6_add_resources(...)` `FILES`
list.

## The runtime library, and a block of dead code

This project is `/MD` (`MultiThreadedDLL`), the same as libobs. Vendored
libraries must match or the link fails with `LNK4098`, which `/WX` promotes to
a hard error.

The `string(REPLACE "/MD" "/MT" ...)` block in `CMakeLists.txt` is **dead
code**: obs-studio sets the runtime through the `MSVC_RUNTIME_LIBRARY` target
property, so the flags never contain a literal `/MD` and nothing is replaced.

## Do not edit

- CMake/MSBuild output at the repository root: `qrc_streamelements.cpp`,
  `obs-streamelements-core.sln`, `*.vcxproj*`, `cmake_install.cmake`,
  `CMakeFiles/`, `RelWithDebInfo/`, `*.dir/`, `*_autogen/`. Generated,
  gitignored, and not build definitions — only `CMakeLists.txt` matters. The
  equivalent `.vcxproj`/`.sln` files under `deps/` and `CI/*/nsis/` **are**
  vendored source and are tracked.
- `deps/`, `streamelements/deps/` — vendored third-party.
- `data/locale/*.ini` other than `en-US.ini` — translated externally.

## Formatting

`.clang-format` at the root is authoritative: 8-wide tabs
(`UseTab: ForContinuationAndIndentation`), 80-column limit,
`AccessModifierOffset: -8`, brace on the next line for **function definitions
only** (everything else K&R), `PointerAlignment: Right`,
`SortIncludes: false`, `ReflowComments: false`.

Format only the files you edited:

```sh
clang-format -i -style=file -fallback-style=none <file>
```

**Never run `./formatcode.sh`** — it has no exclusions and would reformat all
of `deps/` and `streamelements/deps/`.

**Install the pre-commit hook once per clone:**
`git config core.hooksPath .githooks`. It is not installed automatically, and
a clone without it formats nothing.

`.githooks/pre-commit` formats **only the line ranges the commit touches**, then
re-stages them. That restriction is the point: the tree was formatted by older
clang-format releases, so a whole-file run with a newer one rewrites code nobody
edited — 19.1.5 reformats 225 lines of `tests/test_source_invariants.cpp`
unprompted. Line ranges keep the blast radius equal to the diff, which is why
no clang-format version is pinned. A file with unstaged changes is never
re-staged; the hook reports it and fails instead. `CLANG_FORMAT=<path>`
overrides discovery; `SE_SKIP_FORMAT=1` or `--no-verify` skips it.

`CI/check-format.sh` exists but **no workflow invokes it** — formatting is
enforced by the hook, not by the pipeline.

## CI

| workflow | what it does |
| --- | --- |
| `build.yml` | the only build pipeline: Windows (`windows-2025-vs2026`) and macOS OBS builds, release notes gate, versioning, and on master, tagging and publishing |
| `tests.yml` | the ctest suite on `windows-latest` and `macos-latest`; a red ctest blocks a PR |
| `pages.yml` | this documentation site — see below |
| `linear-issue-key.yml` | requires a Linear issue key, and surfaces `#partial` |
| `release.yml`, `upload-obs-symbols.yml`, the installer/uninstaller builds, `claude*.yml` | release plumbing and review automation |

`build.yml` pins OBS to a tag in the `CHECKOUT` env of **both** the `windows`
and `macos` jobs — keep them equal. It currently pins `32.2.2`; that choice
is a floor, for the reason given above.

It also passes a `BUILD_OPTIONS` list of `-DENABLE_*=OFF` flags meant to narrow
an artifact-only build. **Do not reuse those locally, and do not trust them
either**: `ENABLE_UI` exists in no OBS tree (checked at 32.2.1 and 33.0.1), so
the frontend is built regardless, and 15 of the 18 flags are reported as "not
used by the project" in the Windows job — the plug-in ones because that job
replaces `plugins/CMakeLists.txt`, which is what actually narrows the build.
Pre-existing and harmless; fixing it means deciding what CI should build, which
is a bigger change than it looks.

### Documentation builds

Two MkDocs sites, built by `pages.yml` from one checkout:

| config | source | published at |
| --- | --- | --- |
| `mkdocs.yml` | `docs/api/` | `/api/` — the JavaScript OBS API reference |
| `mkdocs.architecture.yml` | `docs/architecture/` | `/architecture/` — these pages |

Everything else at the top level of `docs/` is copied verbatim to the site
root *after* both builds and with `cp -n`, so a docs change can never take the
status page down. Both configs are `strict: true` with link and anchor
validation on, so a renamed heading or a moved file **fails the build** instead
of publishing a dead link. Cross-links between the two sites must therefore be
absolute `https://` URLs: each build only knows its own `docs_dir`, and a
relative path into the other site is an unresolvable link.

The toolchain is pinned (`mkdocs-material==9.7.7`) because an unpinned docs
toolchain is a supply-chain hole that runs on every push to master. To preview
locally:

```sh
pip install 'mkdocs-material==9.7.7'
mkdocs build -f mkdocs.yml                  # /api
mkdocs build -f mkdocs.architecture.yml     # /architecture
mkdocs serve -f mkdocs.architecture.yml     # live preview
```

## Git conventions

- **Never commit to `master`.** Branch and open a PR, even for a small fix.
- **Put the Linear issue key in the branch name**
  (`jacob/core-266-track-selive-releases`). master is squash-merged, so a
  branch without a key leaves the work untraceable.
- **Put `#partial` in a PR that does not finish its issue**, or Linear closes
  the issue when the first of several PRs merges.
- Conventional Commits: `fix:`, `feat:`, with scopes (`fix(ci):`).
- **Never start a commit message with `[WORKFLOW-AUTOMATION]`** — CI uses that
  prefix to recognise its own commits and skip the build.
- `RELEASE_NOTES.md` must be non-empty for a master build to tag a release.

## Related

- [Testing](testing.md) — what the suite is and how to run it.
- [Diagnostics](diagnostics.md) — the crash-backend switch in context.
