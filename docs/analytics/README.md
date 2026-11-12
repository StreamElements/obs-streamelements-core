# Analytics

Every analytics event SE.Live reports, and the exact payload each one
carries. Three separate programs report: the plug-in itself, the Windows
installer and the Windows uninstaller. They all post to one endpoint, in one
envelope, and nothing else in the product reports anything.

This is the companion to the
[architecture chapters](https://streamelements.github.io/obs-streamelements-core/architecture/),
which describe how the plug-in is built, and to the
[JavaScript OBS API reference](https://streamelements.github.io/obs-streamelements-core/api/),
which describes the surface a page sees. These pages document what leaves the
machine.

## The ten-second version

| producer | `feature` | events | source |
| --- | --- | --- | --- |
| the plug-in | `obs_selive_core_plugin` | [8](plugin.md) | `streamelements/SEAnalyticsEventsManager.cpp` |
| the installer | `obs_core_plugin_installer` | [9](installer.md) | `CI/obs-streamelements-installer/main.nsi` |
| the uninstaller | `obs_core_plugin_uninstaller` | [5](uninstaller.md) | `CI/obs-streamelements-uninstaller/main.nsi` |

Twenty-two event names in all. Every payload goes to
`https://api.streamelements.com/science/insert/obslive` as
`application/json`, and `feature` is the only field that reliably says
which program sent it — see
[Reading the data](reading-the-data.md#feature-is-the-producer-source-is-not).

<figure class="se-fig">
<div class="se-fig-scroll">
<svg viewBox="0 0 880 410" role="img" aria-label="Three producers report analytics: the plug-in with feature obs_selive_core_plugin and 8 events, the installer with feature obs_core_plugin_installer and 9 events, and the uninstaller with feature obs_core_plugin_uninstaller and 5 events. All three build the same envelope: props carrying feature, name, source, placement, sessionId, hostMachineId and a _meta object, plus a fields array carrying plugin_version, obs_version, seconds_since_start and seconds_since_previous_event. All three POST it as application/json to api.streamelements.com/science/insert/obslive.">
<title>Three producers, one envelope, one endpoint</title>
<rect class="se-box" x="15" y="14" width="250" height="74" rx="6"/>
<text class="se-ttl" x="140" y="38" text-anchor="middle">the plug-in</text>
<text class="se-id-sm" x="140" y="58" text-anchor="middle">obs_selive_core_plugin</text>
<text class="se-sub" x="140" y="76" text-anchor="middle">8 events · Windows and macOS</text>
<rect class="se-box" x="315" y="14" width="250" height="74" rx="6"/>
<text class="se-ttl" x="440" y="38" text-anchor="middle">the installer</text>
<text class="se-id-sm" x="440" y="58" text-anchor="middle">obs_core_plugin_installer</text>
<text class="se-sub" x="440" y="76" text-anchor="middle">9 events · Windows only</text>
<rect class="se-box" x="615" y="14" width="250" height="74" rx="6"/>
<text class="se-ttl" x="740" y="38" text-anchor="middle">the uninstaller</text>
<text class="se-id-sm" x="740" y="58" text-anchor="middle">obs_core_plugin_uninstaller</text>
<text class="se-sub" x="740" y="76" text-anchor="middle">5 events · Windows only</text>
<path class="se-edge" d="M 140,88 V 120 M 440,88 V 120 M 740,88 V 120"/>
<path class="se-head" d="M 140,129 L 134.5,120 L 145.5,120 Z M 440,129 L 434.5,120 L 445.5,120 Z M 740,129 L 734.5,120 L 745.5,120 Z"/>
<rect class="se-frame" x="15" y="132" width="850" height="150" rx="6"/>
<text class="se-cap" x="29" y="152">EVERY PAYLOAD</text>
<rect class="se-box" x="40" y="162" width="390" height="104" rx="6"/>
<text class="se-ttl" x="56" y="183">props</text>
<text class="se-id-sm" x="56" y="207">feature</text>
<text class="se-id-sm" x="56" y="225">name</text>
<text class="se-id-sm" x="56" y="243">source</text>
<text class="se-id-sm" x="56" y="261">placement</text>
<text class="se-id-sm" x="250" y="207">sessionId</text>
<text class="se-id-sm" x="250" y="225">hostMachineId</text>
<text class="se-id-sm" x="250" y="243">_meta { }</text>
<rect class="se-box" x="450" y="162" width="390" height="104" rx="6"/>
<text class="se-ttl" x="466" y="183">fields</text>
<text class="se-sub" x="516" y="183">— array of [name, value] pairs</text>
<text class="se-id-sm" x="466" y="207">plugin_version</text>
<text class="se-id-sm" x="466" y="225">obs_version</text>
<text class="se-id-sm" x="466" y="243">seconds_since_start</text>
<text class="se-id-sm" x="466" y="261">seconds_since_previous_event</text>
<path class="se-edge-em" d="M 440,282 V 322"/>
<path class="se-head-em" d="M 440,331 L 434.5,322 L 445.5,322 Z"/>
<text class="se-note-em" x="452" y="306">application/json</text>
<rect class="se-box-em" x="215" y="334" width="450" height="60" rx="6"/>
<text class="se-id" x="440" y="360" text-anchor="middle">POST /science/insert/obslive</text>
<text class="se-sub" x="440" y="380" text-anchor="middle">api.streamelements.com</text>
</svg>
</div>
<figcaption>The three producers are separate programs that share a payload
shape, not a shared implementation: the plug-in builds it in C++, the
installer and uninstaller each build it as NSIS string concatenation. Where
they differ — and they do, in the <code>_meta</code> keys, the extra fields
and even the JSON wrapping — the differences are listed in
<a href="envelope.md">The envelope</a>.</figcaption>
</figure>

## The pages

- **[The envelope](envelope.md)** — the props every event carries, the
  `fields` array and why it is an array of pairs, `_meta`, and the four
  places where the three producers disagree about the shape.
- **[Identity and sessions](identity.md)** — where `hostMachineId` comes
  from, why all three producers resolve the same value on Windows, what a
  `sessionId` covers, and what can therefore be joined to what.
- **[Plug-in events](plugin.md)** — the 8 events, their payloads, and which
  three of them only exist in some builds.
- **[Installer events](installer.md)** — the 9 events as the funnel they
  form, the fields unique to the installer, and the three abort paths.
- **[Uninstaller events](uninstaller.md)** — the 5 events, and the two
  fields it reports empty.
- **[Reading the data](reading-the-data.md)** — the traps between this
  catalogue and a correct query: naming that breaks its own convention,
  build-conditional events, what no event answers.
- **[Adding an event](adding-an-event.md)** — the recipe in each of the
  three producers, and what to decide before writing one.

## Where the authority is

The source is the truth: `SEAnalyticsEventsManager::AddRawEvent` for the
plug-in and the `SendTrackingEvent` function in each NSIS script. These
pages summarise and connect them, and a disagreement means a page needs
fixing.

Two of the pipeline's properties are also pinned by the test suite rather
than only written down —
`check_wyvrn_init_reports_its_outcome_to_analytics` and
`check_the_analytics_managers_shared_state_is_atomic`, both in
`tests/test_source_invariants.cpp`. See
[Enforced invariants](https://streamelements.github.io/obs-streamelements-core/architecture/invariants/).

## Where the data is

The events are read in the SE.Live analytics dashboard, which is internal
and Superset-hosted:

[observatory.streamelements.dev/superset/dashboard/p/EwdyM758jQG](https://observatory.streamelements.dev/superset/dashboard/p/EwdyM758jQG/)

Every chart there is built on the payloads catalogued here, which is why
[Reading the data](reading-the-data.md) is worth reading before trusting a
number: several of the traps on that page — the WYVRN denominator, the
partial crash count, the missing uninstall version — are invisible in a
chart and decisive for what it means.

## What this is not

It is not a description of what happens to an event after the POST. The
endpoint, its storage and the dashboard's own queries and derived metrics
live outside this repository, so the catalogue stops at the wire.

It is also not the crash-reporting path. Crash reports go to Sentry or
BugSplat with their own payloads, documented in
[Diagnostics](https://streamelements.github.io/obs-streamelements-core/architecture/diagnostics/).
The one analytics event that overlaps is `OBS Studio Crashed`, which is a
count rather than a report — see
[Plug-in events](plugin.md#obs-studio-crashed).
