# Streaming bandwidth test

`window.host`

## `streamingBandwidthTestBegin(BandwidthTestSettings, ServerInfo[], ResultCallback<success>)`

Begin streaming bandwidth test with specified maximum bitrate and test duration.

## `streamingBandwidthTestEnd(stopIfRunning, ResultCallback<BandwidthTestStatus>)`

Complete streaming bandwidth test. If *stopIfRunning* == true, bandwidth test which is currently in progress will stop, discarding remaining servers to be tested, and will present only the data which has been accumulated so far.

## `streamingBandwidthTestGetStatus(ResultCallback<BandwidthTestStatus>)`

Present the bandwidth test results which have been accumulated so far.
