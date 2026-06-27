# Linux Prebuilt Release Notes (`obs-streamelements-core`)

This note exists to keep Linux release packaging explicit and narrow in scope.

## What gets published

This repository should publish its own prebuilt archive separately from
`obs-browser`.

Recommended artifact shape:
- `obs-streamelements-core-<version>-<distro-tag>-<obs-version>.tar.gz`

The archive should contain:
- `obs-streamelements-core/bin/64bit/obs-streamelements-core.so`
- `obs-streamelements-core/data/obs-plugins/obs-streamelements-core/...`

## Why this is a separate release

Even though this repo depends on the companion `obs-browser` fork, the release
message should stay repo-specific:
- this repo ships the StreamElements plugin itself
- `obs-browser` ships the browser runtime/plugin layer it depends on
- users should install matching releases from both repos

## Compatibility guardrails

Do not describe Linux prebuilt artifacts as universal.

Each archive is only meaningful when these stay aligned:
- distro/runtime family
- OBS version family
- architecture
- Qt/CEF stack expectations
- matching custom `obs-browser` release

Current practical stance:
- prefer one validated target at a time, for example Fedora 43 with OBS 32.1.x
- expand only after real validation on another target

## Suggested release message

```text
Linux prebuilt release for obs-streamelements-core

Target:
- distro/runtime: <distro-tag>
- OBS: <obs-version>
- architecture: x86_64

This artifact contains only obs-streamelements-core.
You must also install the matching prebuilt obs-browser release from the
companion repository.

Important:
- This is not a universal Linux build.
- Use it only on the validated distro/runtime and OBS family listed above.
- Do not mix it with a different active obs-browser variant.

Install:
1. Close OBS.
2. Extract the archive.
3. Copy `obs-streamelements-core` into `~/.config/obs-studio/plugins/`.
4. Install the matching obs-browser release.
5. Start OBS and validate.
```

## Packaging command

Example:

```bash
./_scripts/package-linux-release.sh \
  --distro-tag fedora43 \
  --obs-version obs32.1.2
```
