# Identity and sessions

Two props identify where an event came from: `hostMachineId`, which is meant
to be stable for the life of a machine, and `sessionId`, which is minted per
run. What can be joined to what follows entirely from how those two are
produced, and the answer is not the same on Windows and macOS.

## `sessionId`

A fresh GUID, minted once per producer process and never changed:

| producer | minted in | covers |
| --- | --- | --- |
| the plug-in | `SEAnalyticsEventsManager`'s constructor, early in `SEMain::Initialize()` | one OBS run |
| the installer | `scienceInit`, from `.onInit` | one installer run |
| the uninstaller | `scienceInit`, from `.onInit` | one uninstaller run |

So a plug-in `sessionId` is an OBS session, and a restart of OBS is a new
one. It is not a user, an account or an install.

The plug-in's value is also handed to pages: `ISEHostInfoProvider` reports
it as `hostSessionUniqueId` alongside `hostMachineUniqueId` from the same
manager (`SEHostInfoProvider.cpp:34`), which is how a page can tie its own
telemetry to an analytics session.

## `hostMachineId` on Windows

One value, cached in the registry, minted by whichever SE program needed it
first:

```
HKEY_LOCAL_MACHINE\SOFTWARE\WOW6432Node\StreamElements
    MachineUniqueIdentifier    REG_SZ
```

The 64-bit plug-in spells `WOW6432Node` explicitly
(`SEUtils/SEEnvConfigUtils.cpp:38`). The installer and uninstaller are
32-bit processes asking for `SOFTWARE\StreamElements`, which WOW64 redirects
to the same key. **So all three producers read and write one value, and
`hostMachineId` is directly comparable across them.**

Every id is `<4-letter tag>/<payload>`, and the tag says which code minted
it:

| tag | minted by | payload |
| --- | --- | --- |
| `WUID/` | the NSIS helper | a machine UUID |
| `SWID/` | the plug-in | the WMI `Win32_ComputerSystemProduct` UUID, a GUID and a crypto-random string |
| `SEID/` | either, when no machine UUID could be read | a GUID and a crypto-random string |

The plug-in's own generation path is at
`SEUtils/SESystemInfoUtils.cpp:633`: read the registry, and only if that
yields nothing usable, query WMI for the product UUID (`SWID/`), falling
back to a generated id (`SEID/`). On a normal Windows install the installer
has already run and written the value, so the plug-in only ever reads it —
which is why a `WUID/` id is the common case in the data even though the
plug-in cannot produce one.

!!! note "The NSIS side is not readable as source here"

    `NSISConfig.dll` is fetched from
    `cdn.streamelements.com/obs/dist/nsis-config` at installer build time,
    so the table above is read off the strings embedded in the shipped DLL
    — `SOFTWARE\StreamElements`, `MachineUniqueIdentifier`, `WUID/`,
    `SEID/` and the same `select * from Win32_ComputerSystemProduct` query
    — plus the plug-in source that reads the result. The algorithm matches;
    the code is elsewhere.

### Values the plug-in refuses

A stored id is read back and then validated, because it is written straight
back out again and so becomes that machine's permanent identity. Four exact
strings are rejected as known-bad — a known duplicate, all-zeroes,
all-`F`s, and one value set by a cracked Office installer — and so is
anything not even shaped like an id: shorter than 16 characters, or without
a `/` at index 4.

That second rule exists because of an observed machine whose stored id was
literally `"S"`, the first character of an `SWID`/`SEID` value. Every crash
report from it was anonymous and none could be correlated with any other. A
rejected value is logged at `LOG_WARNING` and a fresh id is generated on the
spot.

### The id can change between sessions if the write fails

The write is to `HKEY_LOCAL_MACHINE`, so it needs elevation. When the direct
write fails, `WriteEnvironmentConfigStrings` falls back to
`ShellExecuteW(..., "runas", "obs-streamelements-set-machine-config.exe", ...)`
— a UAC prompt. If that is declined, or the helper is missing, nothing is
persisted and the **next** process to need an id mints another one.

Within one OBS session this does not show up in the data: the analytics
manager reads the id once, in its constructor, and every event uses that
cached copy. Across sessions it does — the same machine reports a different
`hostMachineId` each run. So `hostMachineId` is a reliable machine key only
where the value was successfully persisted, which is the normal case
precisely because the installer runs elevated and writes it first.

## `hostMachineId` on macOS

A different scheme, in `SEUtils.mm:80`:

```
ALSU/<IOPlatformSerialNumber>-<uppercase SHA-256 of the login name>
```

Two consequences worth keeping straight:

- **It is per user, not per machine.** The username hash is part of the id,
  so two macOS accounts on one Mac are two `hostMachineId` values. The
  Windows id has no user component at all.
- **There is no registry and no persistence.** It is derived from the
  hardware serial on every call, so it is stable without needing to be
  written anywhere — and the failure modes are the literal sentinel strings
  `ERR_NO_PLATFORM_EXPERT`, `ERR_NO_CFS_SERIAL` and `ERR_NO_NS_SERIAL`,
  which reach the wire as the `hostMachineId` value itself. Those three
  strings in the data are a machine whose serial could not be read, not an
  id.

There is no macOS installer or uninstaller in this repository, so on macOS
the plug-in is the only producer and the cross-producer join below does not
arise.

## What joins to what

| question | key | works? |
| --- | --- | --- |
| which events belong to one OBS run | `sessionId` | yes |
| which events belong to one installer run | `sessionId` | yes |
| did this machine install and then run SE.Live | `hostMachineId` | yes, on Windows |
| did this machine install and then uninstall | `hostMachineId` | yes, on Windows |
| which install produced this OBS session | — | **no** |
| which user | — | **no** |

The fifth row is the one that catches people. The installer and the plug-in
share a machine id but nothing narrower: their `sessionId`s are independent,
and no event carries the other producer's session. An install and a
subsequent first run can be ordered by timestamp on the same machine, but
they cannot be linked as one flow.

The sixth row is by design: nothing in any payload identifies a
StreamElements account, a channel or a person. The plug-in knows the signed-in
state (`ISEConfig::STARTUP_FLAGS_SIGNED_IN`) and does not report it.

## Where the identity leaves the payload

Once, in the uninstaller. `ShowFarewellPopup`
(`CI/obs-streamelements-uninstaller/main.nsi:635`) opens the uninstall
survey in the user's browser with both ids appended as query parameters:

```
https://obs.streamelements.com/uninstall-popup-quiz?identity=<hostMachineId>&session=<sessionId>
```

The URL is overridable through the `UninstallerPopupQuizUrl` product
environment value, and the parameters are appended either way. This is the
one place either id travels anywhere other than the analytics endpoint, and
it is what lets a survey answer be joined to the uninstall events that
preceded it.

## What else is personal

Two payloads carry user-authored or user-specific text, both from the
plug-in, and both are worth knowing before treating the event stream as
anonymous:

- **`se_live_report_issue`** puts the user's own description of their
  problem in the `message` prop — free text, typed by the user, whatever
  they chose to write in it.
- **`se_live_dock_show_click` / `se_live_dock_hide_click`** carry the dock's
  live window title in `dock_widget_title`, which for a user-added dock is a
  name the user or a page chose.

Nothing else in the catalogue contains a path, a hostname, an account, a URL
or a machine name.

## Related

- [The envelope](envelope.md) — where these two props sit.
- [Plug-in events](plugin.md) — the two events named above.
- [Uninstaller events](uninstaller.md) — the farewell popup in context.
