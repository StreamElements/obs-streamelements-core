# obs-streamelements-core

Core SE.Live OBS plugin.

# JavaScript OBS API

The host API this plugin exposes to page JavaScript — 217 `window.host` calls,
104 `window` events and methods, and 70 data structures — is documented in
**[`docs/api/`](docs/api/)**, currently at API version 6.6.

That tree is the source of truth. It replaces `JavaScript OBS API Version
6.6.docx`, which is not in version control; see
[`tools/docx-to-markdown/`](tools/docx-to-markdown/) for how it was converted
and verified.

# Building locally

This plugin does not build on its own. It is built as part of an obs-studio
tree, and it has a hard dependency on the `obs-browser` plugin being present
as a sibling — `StreamElementsBrowserWidget.cpp` includes
`../../obs-browser/panel/browser-panel.hpp` by relative path.

## Layout

```
obs-studio/
├── CMakeUserPresets.json          <- copy of this repo's CMakeUserPresets.json (Windows)
└── plugins/
    ├── CMakeLists.txt             <- must contain: add_obs_plugin(obs-streamelements-core)
    ├── obs-browser/               <- required
    └── obs-streamelements-core/   <- this repo
```

Clone obs-studio with `--recursive` so `obs-browser` comes along. CI pins
obs-studio to tag `31.1.2`; newer trees also work.

## Windows

CMake reads `CMakeUserPresets.json` only from the top-level source directory,
so the preset has to be copied up to the obs-studio root before configuring:

```cmd
copy plugins\obs-streamelements-core\CMakeUserPresets.json CMakeUserPresets.json
cmake --preset=windows-x64-streamelements
msbuild build_x64\obs-studio.sln -p:Configuration=RelWithDebInfo
```

The `windows-x64-streamelements` preset just inherits obs-studio's own
`windows-x64` preset, so configuring with `--preset=windows-x64` works too.

## macOS

`deps/BugSplat.xcframework.zip` must be unpacked first or CMake stops with a
fatal error:

```bash
(cd plugins/obs-streamelements-core/deps && unzip -o BugSplat.xcframework.zip)
cmake --preset=macos -DCMAKE_OSX_ARCHITECTURES=arm64 -B build_macos_arm64
cd build_macos_arm64 && xcodebuild -configuration RelWithDebInfo \
    -scheme obs-streamelements-core \
    -destination "generic/platform=macOS,name=Any Mac"
```

## Notes

- `streamelements/Version.generated.hpp` must define
  `STREAMELEMENTS_PLUGIN_VERSION`. A placeholder is committed so a fresh
  checkout compiles; CI overwrites it with the real build number. Don't edit
  it by hand.
- Don't copy the `-DENABLE_*=OFF` flags out of `.github/workflows/build.yml`.
  CI turns off nearly every OBS feature — including `-DENABLE_UI=OFF` —
  because it only needs the plugin binary as an artifact. For local work you
  want the OBS UI built so there is something to run the plugin in.
- There are no tests. Compiling is the only automated check, alongside
  `CI/check-format.sh`.
- Format only what you changed:
  `clang-format -i -style=file -fallback-style=none <file>`. Do not run
  `formatcode.sh` — it has no exclusions and rewrites all of `deps/` and
  `streamelements/deps/`.

# Release channels status

|Platform 	|Environment 	|Version 	|
|---	|---	|---	    |
| Windows | `signed` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/signed/obs-streamelements.version.svg" /> |
| Windows | `qa` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/qa/obs-streamelements.version.svg" /> |
| Windows | `beta` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/beta/obs-streamelements.version.svg" /> |
| Windows | `latest` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/latest/obs-streamelements.version.svg" /> |
| Windows | `stable` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/stable/obs-streamelements.version.svg" /> |
| MacOS | `signed` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/signed/obs-streamelements.version.svg" /> |
| MacOS | `qa` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/qa/obs-streamelements.version.svg" /> |
| MacOS | `beta` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/beta/obs-streamelements.version.svg" /> |
| MacOS | `latest` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/latest/obs-streamelements.version.svg" /> |
| MacOS | `stable` | <img src="https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/stable/obs-streamelements.version.svg" /> |

<a href="https://streamelements.github.io/obs-streamelements-core/status.html" target="_blank">Extended Status Page</a> — every channel on both platforms, with build dates, promotion drift and release notes, read live from the CDN manifests. Source: <a href="docs/status.html">docs/status.html</a>.

# Deployment

1. Make sure RELEASE_NOTES.md reflects the release content of the plug-in since previous release to the public (`latest` release group).

2. Once this repository is built, run the <a href="https://github.com/StreamElements/obs-streamelements-core/actions/workflows/release.yml" target="_blank">Release Signed Binaries</a> action in github.

# Proper deployment order

| From Environment | To Environment | Predicate |
|--- |--- |---
| `signed` | `qa` | none |
| `qa` | `beta` | passes internal and closed beta testing |
| `beta` | `latest` | 2 weeks without serious issues on `beta` |
| `latest` | `stable` | 2 weeks without stability issues on `latest` |

