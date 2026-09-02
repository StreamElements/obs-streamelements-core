# RazerWyvrnStatus

Availability of the [Razer WYVRN](../host/razer-wyvrn.md) integration.

**Available since API version 6.8**

Returned as the `razerWyvrn` member of
[`HostCapabilities`](HostCapabilities.md), and carried verbatim as the payload
of [`hostRazerWyvrnStatusChanged`](../window.md#hostrazerwyvrnstatuschanged).
The two are produced by the same code and cannot disagree.

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| available | bool | `true` only when events can be fired right now. |
| initialized | bool | `true` once the SDK has initialized successfully. |
| status | string | See below. |
| eventCount | number | Number of events `getAllRazerWyvrnEvents` would return. |

`status` is a closed vocabulary, so a caller can branch on it without parsing
prose:

| **Value** | **Meaning** |
| --- | --- |
| `ok` | Initialized and ready. |
| `initializing` | Initialization is in flight. Takes about 3.4 seconds from OBS start. |
| `notCompiledIn` | This build has the integration disabled. |
| `notSupportedOnPlatform` | Not a Windows build. |
| `dllNotFound` | `RzChromatic64.dll` is absent — the ordinary outcome on a machine with no Razer software. |
| `dllInvalidSignature` | The DLL is present but is not signed by Razer, and was refused. |
| `initFailed` | The DLL loaded but the SDK declined to initialize. |
| `shuttingDown` | OBS is closing; no new events are accepted. |
