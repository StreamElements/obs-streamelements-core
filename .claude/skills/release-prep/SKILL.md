---
name: release-prep
description: Prepare RELEASE_NOTES.md so the next master build tags and publishes a release. Use before merging user-visible changes to master.
disable-model-invocation: true
---

# Release prep

`$ARGUMENTS`

A master build only tags and publishes when `RELEASE_NOTES.md` is non-empty. The `version` job in `.github/workflows/build.yml` gates on `grep -c "" RELEASE_NOTES.md` — an empty file means the build compiles and uploads but never becomes a release.

## Add the notes

Append one bullet per user-visible change to `RELEASE_NOTES.md`. Match the style already in `RELEASE_NOTES.history.md`:

```
- Fix: crash under certain conditions when scenes are changed
- Feature: lazy video encoder creation to reduce GPU resource consumption
```

Rules of thumb:
- No version heading and no blank leading line — CI adds `### <version>` when it rotates the file.
- One line per change, written for a streamer, not for a developer. Skip pure CI, refactor, and build-system commits.
- Leave `RELEASE_NOTES.header.md` and `RELEASE_NOTES.footer.md` alone — they are static wrappers for the published notes page.
- If `RELEASE_NOTES.md` already has content, append; someone else's unreleased notes are in there.

## What CI does afterwards

On a successful master build, the `finalize` job prepends `### <version>` plus the current notes to `RELEASE_NOTES.history.md`, truncates `RELEASE_NOTES.md`, and pushes a commit titled:

```
[WORKFLOW-AUTOMATION] <version> finalize build
```

That prefix is load-bearing — it is how the next run detects its own commit and skips rebuilding, and it switches the concurrency group. **Never write a human commit message starting with `[WORKFLOW-AUTOMATION]`.**

Version numbers are minted by CI (`BASE_BUILD_NUMBER` + run number, formatted `yy.m.d.build`). Nothing about the version is stored in the repo, so don't try to set or predict one. `streamelements/Version.generated.hpp` is a committed placeholder that CI overwrites — leave it.

## Then

Commit the notes on your feature branch along with the change they describe, and open a PR. Never push to master directly.
