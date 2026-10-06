### 26.10.6.1039

- Fix: SE.Live could stop OBS Studio from starting when its own settings file could not be opened, for example on a read-only or unreachable settings folder; it now starts with default settings instead
- Fix: closing a SE.Live panel could crash OBS Studio if the panel was redrawn while it was closing
- Fix: the crash reporter could itself crash while preparing a report, for users whose Windows user name contains accented or non-Latin characters, or when files in the OBS Studio settings folder changed at that moment
- Fix: OBS Studio could hang on exit, with the window gone but the process still running, when closed shortly after start-up while an update prompt was on screen
- Fix: crash when exiting OBS Studio while a SE.Live dock was still starting up
- Fix: SE.Live crashed on every start-up if its saved layout had been corrupted; the layout is now reset instead
- Fix: OBS Studio could crash after a scene item group added through SE.Live was removed or ungrouped, the next time an item in that scene was selected, moved, renamed or changed
- New: added support for Razer WYVRN Chroma and Haptics integration

### 26.9.4.994

- Fix: lazy video encoder creation on use to reduce GPU resource consumption
- Fix: cache encoder basic properties to reduce GPU resource consumption
- Fix: crash under certain conditions when scenes are changed
- Fix: use OBS private canvas objects in custom video compositions instead of OBS views directly
- Fix: adjust unknown encoder video formats to preferred NV12 video format
- Fix: crashes triggered by corrupted memory
- Fix: crashes triggered by OBS Studio update
- Fix: crashes triggered by exiting OBS Studio
- Fix: crash triggered by a race between the OBS Update dialog on start-up and SE.Live
- Fix: crash when stopping a stream while broadcasting from more than one canvas or to more than one destination
- Fix: crashes caused by running out of memory were not reported
- Fix: crash reporting silently unavailable on installations with a long configuration path
- Fix: some crashes were reported without asking permission first, and without the details needed to act on them
- Fix: crashes caused by memory corruption or a fatal internal check were not reported at all
- Fix: OBS Studio could be left running and unresponsive after a crash when the report prompt was blocked behind another window
- Fix: SE.Live docks were registered incorrectly with the OBS window, corrupting the saved layout
- Fix: the crash report prompt no longer processes unrelated window messages while a crash is being collected
- Fix: crash when switching scene collections, caused by SE.Live docks not being registered correctly with the OBS window
- Fix: OBS Studio could crash a second time while the crash report prompt was on screen, losing the report being collected
- Fix: crashes originating in other plug-ins could be reported as SE.Live crashes
- Fix: small memory leak during the update check
- Fix: crash when a scene item changed while its video composition was being destroyed
- Fix: scene item data returned to the JavaScript API was missing its videoCompositionId
- Fix: a local file request with a bad signature was correctly refused, but leaked a file handle each time
- Infra: removed the unused 32-bit installer build
- Infra: crash reporting backend switched to sentry.io
- Breaking change: SE.Live now requires at least OBS 32.2.0

