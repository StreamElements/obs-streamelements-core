# obs-streamelements-core

Core SE.Live OBS plugin.

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

<a href="https://cdn.streamelements.com/obs/qa/status.html" target="_blank">Extended Status Page</a>

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

