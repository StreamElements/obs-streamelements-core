# Installer events

Nine event names, `feature: obs_core_plugin_installer`, Windows only. All
nine go through `SendTrackingEvent`
(`CI/obs-streamelements-installer/main.nsi:1299`), so each carries the whole
[envelope](envelope.md) plus the installer's own fields below.

Eight of the nine form an ordered funnel. The ninth, `..._abort`, can
replace any of them.

<figure class="se-fig">
<div class="se-fig-scroll">
<svg viewBox="0 0 880 540" role="img" aria-label="The installer's eight ordered events, from se_live_installer_start through show_user_agreement, show_select_target_folder, show_choose_components, show_install_progress, download_obs_studio, show_install_finished, to se_live_installer_completed. From any one of those stages the run can instead end in se_live_installer_abort, whose source prop is user, network or local_machine.">
<title>The installer funnel, and the abort that can replace any stage</title>
<rect class="se-box" x="40" y="36" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="58">se_live_installer_start</text>
<path class="se-edge" d="M 260,72 V 87"/>
<path class="se-head" d="M 260,96 L 254.5,87 L 265.5,87 Z"/>
<rect class="se-box" x="40" y="96" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="118">se_live_installer_show_user_agreement</text>
<path class="se-edge" d="M 260,132 V 147"/>
<path class="se-head" d="M 260,156 L 254.5,147 L 265.5,147 Z"/>
<rect class="se-box" x="40" y="156" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="178">se_live_installer_show_select_target_folder</text>
<path class="se-edge" d="M 260,192 V 207"/>
<path class="se-head" d="M 260,216 L 254.5,207 L 265.5,207 Z"/>
<rect class="se-box" x="40" y="216" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="238">se_live_installer_show_choose_components</text>
<path class="se-edge" d="M 260,252 V 267"/>
<path class="se-head" d="M 260,276 L 254.5,267 L 265.5,267 Z"/>
<rect class="se-box" x="40" y="276" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="298">se_live_installer_show_install_progress</text>
<path class="se-edge" d="M 260,312 V 327"/>
<path class="se-head" d="M 260,336 L 254.5,327 L 265.5,327 Z"/>
<rect class="se-frame" x="40" y="336" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="358">se_live_installer_download_obs_studio</text>
<text class="se-sub" x="464" y="358" text-anchor="end">conditional</text>
<path class="se-edge" d="M 260,372 V 387"/>
<path class="se-head" d="M 260,396 L 254.5,387 L 265.5,387 Z"/>
<rect class="se-box" x="40" y="396" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="418">se_live_installer_show_install_finished</text>
<path class="se-edge" d="M 260,432 V 447"/>
<path class="se-head" d="M 260,456 L 254.5,447 L 265.5,447 Z"/>
<rect class="se-box-em" x="40" y="456" width="440" height="36" rx="5"/>
<text class="se-id-sm" x="56" y="478">se_live_installer_completed</text>
<path class="se-edge-weak" d="M 480,54 H 552 M 480,114 H 552 M 480,174 H 552 M 480,234 H 552 M 480,294 H 552 M 480,354 H 552 M 480,414 H 552 M 480,474 H 552 M 552,54 V 474 M 552,264 H 630"/>
<path class="se-head-em" d="M 639,264 L 630,258.5 L 630,269.5 Z"/>
<rect class="se-box-em" x="640" y="234" width="225" height="66" rx="6"/>
<text class="se-id-sm" x="752" y="258" text-anchor="middle">se_live_installer_abort</text>
<text class="se-sub" x="752" y="277" text-anchor="middle">source: user | network |</text>
<text class="se-sub" x="752" y="292" text-anchor="middle">local_machine</text>
</svg>
</div>
<figcaption>The dashed stage is conditional: the OBS download only runs when
the component is selected. Closing the window at any point is an abort, not
a silent exit — <code>.onGUIEnd</code> reports one unless the install already
completed. The <code>/AUTOSTART</code> switch skips the three screens between
<code>start</code> and <code>show_install_progress</code>, so their events
never fire on that path.</figcaption>
</figure>

## The stages

| event | step | `curr_step_name` | fired from |
| --- | --- | --- | --- |
| `se_live_installer_start` | 10 | `Welcome` | `onShowWelcome`, `:1510` / `:1519` |
| `se_live_installer_show_user_agreement` | 20 | `User Agreement` | `onShowLicense`, `:1533` |
| `se_live_installer_show_select_target_folder` | 30 | `Select Target Folder` | `onShowFolder`, `:1545` |
| `se_live_installer_show_choose_components` | 40 | `Choose Components` | `onShowComponents`, `:1557` |
| `se_live_installer_show_install_progress` | 50 | `Install Progress` | `onShowInstall`, `:1569` |
| `se_live_installer_download_obs_studio` | 50 | `Install Progress` | `onBeginDownloadOBS`, `:1589` |
| `se_live_installer_show_install_finished` | 60 | `Install Finished` | `onShowFinish`, `:1581` |
| `se_live_installer_completed` | 60 | `Install Finished` | `FinishInstallation` from `.onInstSuccess`, `:1484` |

`se_live_installer_download_obs_studio` is the only one with no screen of
its own: it fires from inside the OBS Studio section, which runs during step
50, and only when that component is selected.

### Each stage fires at most once, and only going forward

Every screen handler is guarded by
`IntCmp $g_instCurrentScreenIndex <n> skip proceed skip` — fire only when
the stored index is **less** than this screen's. So navigating back and
forward again does not re-fire anything, and the funnel counts distinct runs
that reached a stage rather than screen impressions.

The same guard has a consequence for the other two fields: the stored index
is the **furthest screen reached**, not the screen currently shown. After a
Back, an abort reports `curr_step_index` 40 while the user is looking at
screen 30.

### `/AUTOSTART` skips three of them

With `/AUTOSTART` on the command line, `onShowWelcome` sends
`se_live_installer_start`, sets `auto_start` to `true`, and jumps four pages
forward. The user agreement, target folder and components events never fire,
so an autostart run's funnel is `start` → `show_install_progress` →
`show_install_finished` → `completed`. Filter on the `auto_start` field
before reading a drop-off between steps 10 and 50.

## Fields

On top of the [four every producer sends](envelope.md#fields):

| field | value |
| --- | --- |
| `setup_version` | `SETUP_VERSION` at build time, beside `plugin_version` from `PRODUCT_VERSION` |
| `prev_event_name` | the previous event this run sent — empty on the first |
| `curr_step_index` | `10`…`60`, or empty before any screen |
| `curr_step_name` | the screen name from the table above |
| `auto_start` | `true` with `/AUTOSTART`, otherwise **empty**, not `false` |
| `installation_started` | `true` once step 50 has been reached, else `false` |
| `obs_download_requested` | `true` / `false`, or empty before step 50 |
| `obs_download_requested_by` | `forced_by_setup` / `user`, or empty |

`prev_event_name` is what makes the funnel readable without joining: each
event names its predecessor within the same `sessionId`, so a drop-off is
visible from a single row.

`installation_started` and the two `obs_download_*` fields are computed from
the section flags at send time, and all three are empty before step 50
because the components screen is where the answer exists.

### `obs_download_requested_by` says whether the machine already had OBS

The distinction is the section's read-only flag:

| value | means |
| --- | --- |
| `forced_by_setup` | the component was selected **and** read-only, i.e. `VerifyObsStudioInstalled` found no OBS or one older than `OBS_REQUIRE_VERSION` |
| `user` | the component was selectable and the user chose it anyway |
| `false` in `obs_download_requested` | an acceptable OBS was already installed |

So this pair, and not any plug-in event, is the only signal in the data for
"how many SE.Live installs also had to install OBS".

### On completion, two more

`se_live_installer_completed` appends:

| field | value |
| --- | --- |
| `desktop_shortcut_requested` | `true` / `false` |
| `start_obs_requested` | `true` / `false` |

## `se_live_installer_abort`

One name, three call sites, told apart by the `source` prop — and carrying
the reason text in `message`:

| `source` | function | `message` |
| --- | --- | --- |
| `user` | `AbortByUserRequest`, `:1403` | `User cancelled installation` |
| `user` | | `User cancelled downloading <what> from <url>` |
| `network` | `AbortByNetworkError`, `:1419` | `Failed downloading <what> from <url>` |
| `local_machine` | `AbortByLocalError`, `:1435` | `User refused to close running OBS components` |
| `local_machine` | | `Failed installing OBS Studio` |
| `local_machine` | | `Another running instance of the installer was detected` |

Three things about this event are easy to get wrong:

**Closing the window is an abort.** `.onGUIEnd` calls `onInstallationEnd`,
which aborts with `User cancelled installation` unless the run already
completed. So abandonment is instrumented, and the abort count is not a
count of errors.

**`source` is not a reliable "was it our fault" split.**
`User refused to close running OBS components` is classified
`local_machine`, although it is a user decision — the classification is the
abort *function* that was called, not the agency behind it. Use `message`.

**One abort fires before any screen exists.** The second-instance guard runs
in `.onInit`, after `scienceInit` but before the first page, so its abort
has empty `curr_step_index` and `curr_step_name`, and an empty
`prev_event_name`. It is the only event a run can send without having sent
`se_live_installer_start` first.

## Related

- [The envelope](envelope.md) — the props and fields all nine share.
- [Uninstaller events](uninstaller.md) — the same machinery, five events.
- [Reading the data](reading-the-data.md#funnels) — reading these as a funnel.
