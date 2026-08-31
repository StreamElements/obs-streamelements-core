# SystemCPUUsageTimes

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| idleSeconds | number | Total seconds CPU was idle (doing nothing) |
| kernelSeconds | number | Total seconds CPU spent executing OS kernel code |
| userSeconds | number | Total seconds CPU spent executing user code |
| totalSeconds | number | idleSeconds + kernelSeconds + userSeconds |
| busySeconds | number | totalSeconds - idleSeconds |
