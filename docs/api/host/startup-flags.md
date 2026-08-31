# Startup flags

`window.host`

## `setStartupFlags(number<flags>, ResultCallback<success>)`

Sets whether on-boarding has completed or not and other start-up flags. Multiple flags can be set together by binary **or-**ing (|) them together.

| **Flag** | **Value** | **Description** |
| --- | --- | --- |
| ONBOARDING_MODE | 1 | Show on-boarding central widget on OBS start-up.<br>This flag is initially set and **must be cleared** when on-boarding completes. |

## `getStartupFlags(ResultCallback<number<flags>>)`
