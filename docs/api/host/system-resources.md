# System resources

`window.host`

## `getSystemCPUUsageTimes(ResultCallback<SystemCPUUsageTimes>)`

Available since API version 1.1

Get time in seconds the system CPU was idling, executing OS kernel code and executing user code. The first call to this function always returns 0 on all metrics. The subsequent calls return values accumulated since the first call.

To calculate total CPU usage, you must calculate deltas between the current call’s result and the previous call result.

For example:

```js
var prevCallResult = { idleSeconds: 10, userSeconds: 1, kernelSeconds: 1,     totalSeconds: 12, busySeconds: 2 };

var currentCallResult = { idleSeconds: 20, userSeconds: 2, kernelSeconds: 2,     totalSeconds: 24, busySeconds: 4 };

var busyDelta = currentCallResult.busySeconds – prevCallResult.busySeconds;
var totalDelta = currentCallResult.totalSeconds – prevCallResult.totalSeconds;

var cpuUsagePercentage = busyDelta * 100 / totalDelta;
```

This technique allows retrieving CPU usage times of different parts and states of the program.

## `getSystemMemoryUsage(ResultCallback<SystemMemoryUsageInfo>)`

Available since API version 1.1

Get current system memory usage information.

## `getSystemHardwareProperties(ResultCallback<SystemHardwareInfo>)`

Available since API version 1.1

Get current system hardware information.
