# Testing

Three layers, each answering a question the others cannot: unit and contract
tests that run in seconds with no OBS, source invariants that pin architectural
rules, and a live end-to-end harness that drives a real OBS.

## The ctest suite

`tests/` holds standalone C++17 executables that link **neither libobs nor
Qt**, so they build and run on any platform in seconds, without an obs-studio
tree:

```sh
cmake -S tests -B tests/build && cmake --build tests/build --config Debug
ctest --test-dir tests/build -C Debug --output-on-failure
```

It is **not** part of the plug-in build (`-DSE_ENABLE_TESTS=ON`, or configure
`tests/` directly). `se_add_test(<name> <sources>)` in `tests/CMakeLists.txt`
registers each target; there are **18**, one of which (`test_http_utils`) is
conditional on libcurl being found. `tests.yml` runs the suite on Windows and
macOS, so a red ctest blocks a PR.

Two kinds live there:

- **Behavioural** — real logic compiled beside the test. Production `.cpp`
  files, not mirrors: `test_string_utils`, `test_session_signature`,
  `test_overlay_url_utils`, `test_razer_wyvrn_config`,
  `test_lazy_encoder_refcount`, the crash-gate family, and the three that
  compile the host API translation units (below).
- **Source-invariant** — `test_source_invariants` opens the production sources
  and asserts a fixed bug pattern is absent, so a regression is caught without
  the OBS build succeeding. See [Enforced invariants](invariants.md).

Anything self-contained enough to compile without libobs/Qt belongs here rather
than in a throwaway harness. Deliberately keeping a unit free of OBS
dependencies is a good reason to structure it that way.

### The host API is under test, all of it

This is the part worth knowing about, because it is unusual:
`test_api_handler_bodies` **compiles and links `SEApiMessageHandler.cpp`** —
the 2,800-line translation unit holding every host API body — in a binary with
no libobs and no Qt. That is possible because the service container publishes
interfaces, so the file's include closure reaches **0 obs and 0 Qt headers**.

What it took is in `tests/fake_api_services.cpp` and
`tests/stub_concrete_services.cpp`: fakes deriving from the `ISE*` interfaces
the bodies reach, plus **link seams** for the fifteen services the container
still carries concretely — 51 methods and eleven free functions, supplied as
real definitions because a concrete class cannot be stood in for by
inheritance. `api_handler_harness.cpp` does the same for `blog`,
`obs_initialized` and `__QtPostTask_Impl`.

The two mechanisms differ in what the compiler can enforce. An interface fake
is **checked**: add a pure virtual and the fake stays abstract and the build
fails. A concrete stub is not, so a method added to one of the fifteen only
fails when a handler actually calls it. That asymmetry is an argument for
interfacing the rest eventually, not a reason to do it now.

The three executables:

| target | covers |
| --- | --- |
| `test_api_handler_infrastructure` | the base: handshake messages, unknown-method dispatch, duplicate registration, re-entrancy, async completion, the weak-services bail, `Clone()`, API-context pairing |
| `test_api_handler_bodies` | the 208 blocks. The target compiles `test_api_handler_bodies.cpp` (narrative cases and the no-argument sweep) **and** `test_api_handler_contract.cpp` (the tables), against the real `SEApiMessageHandler.cpp` |
| `test_api_handler_optional` | the same TU with `SE_ENABLE_WYVRN` and `SE_ENABLE_CENTRAL_WIDGET_DECORATIONS=1`, so the eight otherwise-uncompiled bodies are covered |

Each row supplies a well-formed argument and asserts three things, of which a
live probe can see at most the first: the reply, the exact ordered sequence of
service calls, and **the arguments those calls received**. Six shape tables
carry the families where the bodies repeat themselves, because the repetition
is itself a finding — 69 of the 200 compiled bodies are a straight
pass-through, 40 are `service->Method(result)` serializers, 10 are
bool-returners run both ways.

The **completeness check** is what makes that a contract rather than a sample:
every registered name must appear in a table or on a short `coveredElsewhere`
list pinned by a narrative case, and every row must name a method that still
exists. **A new host API method fails the suite until somebody says what it
does.**

Three handlers cannot be covered and are listed as such: `crashProgram`
dereferences null, `crashProgramFastFail` calls `__fastfail`, and
`deadlockProgram` hangs by design — each is the whole point of its handler.

### What the suite pins that a live probe cannot

- **Flag values, not just delivery.** `broadcastEvent` answers `true` whether
  or not its destination flags are right. The test asserts the exact flags
  (`DEST_ALL_LOCAL & ~DEST_BROWSER_SOURCE` = 65531) and the fixed event name.
- **Branches a real profile never takes.** `getCurrentContainerProperties`
  answers four keys instead of five when there is no dock — a CDP run never
  sees it, because the SE.Live panel is always a dock.
- **The destructive methods.** `deleteAllCookies` and the sign-in flag pair
  log the account out, and `showPopupMenuAtMousePointerPosition` blocks on
  `QMenu::exec()`. All three are tested here and skipped live.
- **Drain loops.** `while (service->Method())` cannot be exercised by a fake
  that answers once — one programmed value makes it run zero times or for
  ever — so the journal can answer a *sequence*.

## Mutation testing, because a green test proves nothing on its own

Every invariant and most narrative cases are proven by **reinstating the bug
and watching the test fail.** This is not ceremony: it has caught two classes
of problem repeatedly.

- **Guards that cannot fire.** The counted-reference check first required the
  auto-release type in the same statement, so the real defect — which declares
  the variable earlier — escaped it. Its named-site needles then collapsed
  whitespace instead of stripping it, so they broke the moment clang-format
  joined a call onto one line. Review found neither; mutation found both.
- **Vacuous success.** A scan whose pattern stops matching reports clean. Every
  such check carries a **non-vacuity floor** — a minimum number of sites it
  must still find, per file where a global floor would let one file's sites
  hide behind another's.

Mechanically: rewrite the production source in place, run ctest, restore from
an in-memory copy in a `finally`, then re-check `git diff --stat`. The restore
is not optional — a mutation harness that half-restores leaves a committed bug.

## The live end-to-end harness

The suite cannot test what needs a real OBS: that a scene change does not
deadlock the UI thread, that an output actually connects, that shutdown is
clean and the reference-count balance is zero. For that, OBS is launched with
`--remote-debugging-port=9222` and driven over CDP, calling `window.host`
directly from the SE.Live page.

The harness lives outside this repository (it names machine-specific rigs,
profiles and media); on the development machine it is at
`C:\se-test\harness\`. It is machine-specific by nature, which is why `tests/`
holds the opposite kind of test.

**What a live run proves and the suite cannot:** one of the worst defects found
this way needed a real libcurl and a real proxy configuration —
`httpRequestText` with no usable URL never returned, and because the body holds
the per-TU API call lock for its whole length, one such call from any page left
the entire host API unanswerable for the life of the process. The suite was
green throughout.

### Rules the harness has to follow, each learned by losing a run

1. **Poll for the method set, not for `window.host`.** The shim creates the
   object synchronously with one key and fills ~209 methods in over the socket
   afterwards — about 1.3 s in which every call is `is not a function`. OBS
   32.2.2 was slow enough to hide this; 33.0.1 is not.
2. **The SE.Live canvases do not exist when `window.host` appears.** A baseline
   taken 7.5 s in reported 0 compositions and 1 output; seconds later there
   were 3 and 6. Poll `getAllVideoCompositions`.
3. **`getCurrentScene` and `getAllScenes` answer for the main canvas** unless
   given `{videoCompositionId: …}`. Verifying a canvas scene change without the
   selector compares the wrong canvas and reports a false failure. The write
   side needs no selector.
4. **The main canvas can only *select* a configured transition** — see
   [Compositions and scenes](compositions.md). Asking it for a stinger on a
   profile that has none configured is correctly refused, and reads as a
   regression if you do not know that.
5. **Race every call against a timer.** A probe that awaits a host callback
   cannot tell "answered false" from "never answered": it hangs exactly as the
   defect makes it hang, so a missing completion looks like a slow call and the
   *next* call looks like the broken one.
6. **Prove the UI thread from the page.** The CDP socket staying open proves
   nothing about the Qt thread. Follow every scene change with a real
   `getHostProperties()`.
7. **Close cleanly, always.** `CloseMainWindow()` silently does nothing while a
   modal window is up, because the modal disables the main window and Windows
   ignores `WM_CLOSE` on a disabled window. Enumerate top-level windows, close
   the ones that are visible **and enabled** — leaving the disabled ones alone
   — and loop, because closing one modal reveals the next (SE.Live modal →
   OBS's update dialog → OBS 33's "Plugin Errors"). `Stop-Process` throws away
   `obs_module_unload`, which is where the measurement lives.
8. **OBS 33 can block a stream start on its own "Warning" dialog**, visible and
   enabled, waiting for a human — `requestStreamingStart` returns `true` and
   `getStreamingStatus` stays false for ever. `WM_CLOSE` *cancels* the go-live;
   accepting it needs `VK_RETURN`.
9. **Back up the profile first**, and test against the real one: a portable
   profile starts empty and misses the whole multi-endpoint configuration, so
   bugs that need real content do not reproduce in it.

### What to read afterwards

A clean run ends with the lines in [Diagnostics](diagnostics.md):
`N of N services released cleanly`, `shutdown complete`,
`reference count balance = 0`, balanced `AddConsumer`/`RemoveConsumer`, no
`future_error`, no crash event, no dump.

## Don't call a change verified on tests that were never run against it

Two specific traps, both of which have produced a believed-but-false result:

- **A stale test binary.** `ctest` happily re-runs the previous executables.
  Once, a broken configure left CXX unenabled, every target failed to build,
  and ctest reported the *previous* binaries as a pass. The inverse also
  happens: a source-invariant binary built before a change reports a failure
  against sources that are now fine. **Always `cmake --build tests/build`
  before `ctest`.**
- **A stale plug-in binary.** Check that the DLL is newer than every production
  source and that the repository is clean, or better, hash it against the build
  output. A mutation harness that rewrote sources after the last build leaves
  mtimes that look fine and content that is not.

And a platform note: **four defects in one refactor series were macOS-only**,
every one a clang diagnostic MSVC does not have — a missing `#ifdef WIN32`,
`-Wnewline-eof`, `-Winconsistent-missing-override`, `-Wrange-loop-construct`.
A green ctest on macOS is **not** a macOS build: the suite does not compile
every header. Build both platforms per series, not per phase.

## Related

- [Enforced invariants](invariants.md) — the catalogue.
- [Diagnostics](diagnostics.md) — what the log lines mean.
- [Build](build.md) — the CI workflows that run all this.
