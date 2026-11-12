# Uninstaller events

Five event names, `feature: obs_core_plugin_uninstaller`, Windows only. The
machinery is the installer's, trimmed: the same `science.nsh`, the same
`SendTrackingEvent` shape
(`CI/obs-streamelements-uninstaller/main.nsi:468`), three screens instead of
six.

| event | step | `curr_step_name` | fired from |
| --- | --- | --- | --- |
| `se_live_uninstaller_start` | 10 | `Welcome` | `onShowWelcome`, `:584` |
| `se_live_uninstaller_show_progress` | 20 | `Uninstall Progress` | `onShowInstall`, `:603` |
| `se_live_uninstaller_show_uninstall_finished` | 30 | `Uninstall Finished` | `onShowFinish`, `:615` |
| `se_live_uninstaller_completed` | 30 | `Uninstall Finished` | `FinishInstallation` from `.onInstSuccess`, `:569` |
| `se_live_uninstaller_abort` | any | whatever was reached | `AbortByUserRequest` `:528`, `AbortByLocalError` `:544` |

The same once-only, forward-only `IntCmp` guard applies as in the
[installer](installer.md#each-stage-fires-at-most-once-and-only-going-forward),
so these are distinct runs reaching a stage, not screen impressions.

## Fields

Only the four every producer sends, plus three:

| field | value |
| --- | --- |
| `prev_event_name` | the previous event this run sent |
| `curr_step_index` | `10`, `20` or `30`, or empty before any screen |
| `curr_step_name` | the screen name above |

There is **no** `auto_start`, no `installation_started` and no
`obs_download_*` here — those are installer-only. `setup_version` exists but
is empty, as below.

## It reports no version

`main.nsi:481` builds the field list with both version fields as literal
empty strings:

```
StrCpy $g_scienceFields '["plugin_version", ""], ["setup_version", ""], …'
```

Not a lookup that failed — the values were never wired up. The consequence
is worth stating plainly: **an uninstall cannot be attributed to a SE.Live
version.** "Which release do people uninstall?" is unanswerable from this
data, and so is any version-cohort retention figure. The installer fills
both fields from `PRODUCT_VERSION` and `SETUP_VERSION` at build time, so
the asymmetry is in the uninstaller alone.

## `se_live_uninstaller_abort`

Two call sites, two `source` values — there is no `AbortByNetworkError`
here, because the uninstaller downloads nothing:

| `source` | `message` |
| --- | --- |
| `user` | `User cancelled uninstaller` — including simply closing the window, via `.onGUIEnd` → `onInstallationEnd` |
| `local_machine` | `User refused to close running OBS components` (`:130`) |
| `local_machine` | `Another running instance of the installer was detected` (`common.nsh:30`) |

That last message says "installer" inside the uninstaller. It is the
installer's string, carried over with the guard, and it is what the data
contains — so match on it as written rather than on the word "uninstaller".

## The farewell popup

`onShowFinish` calls `ShowFarewellPopup` (`:635`) straight after sending
`se_live_uninstaller_show_uninstall_finished`, which opens the uninstall
survey in the user's default browser:

```
https://obs.streamelements.com/uninstall-popup-quiz?identity=<hostMachineId>&session=<sessionId>
```

The base URL is read from the `UninstallerPopupQuizUrl` product environment
value and falls back to `POPUP_QUIZ_DEFAULT_URL` from `common.nsh`; the two
parameters are appended either way, with `&` if the URL already has a query
string. It is opened with `ExecShell "open"`, so nothing reports whether it
was shown, loaded or answered.

This is the one place either identifier leaves the analytics payload, and it
is what lets a survey answer be joined back to the uninstall it came from.
See [Identity](identity.md#where-the-identity-leaves-the-payload).

## Which uninstaller this is

The uninstaller is built from this repository and bundled inside the
installer. Until `26.11.10.1096` the installer bundled a stale binary
fetched from the wrong CDN folder — one built on 2026-07-15, four months
before the repository's own copy of the source — so **events from installs
made before that release come from code that is not in this tree.** The
event names did not change, but anything inferred from their payloads
against today's source should be checked against the release that produced
them.

## Related

- [Installer events](installer.md) — the same machinery with nine events.
- [Identity and sessions](identity.md) — what the popup's two parameters are.
