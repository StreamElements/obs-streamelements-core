# BandwidthTestSettings

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| maxBitsPerSecond | number | Maximum streaming bandwidth (bits per second). The actual bandwidth will be calculated as a percentage of the actual bits transferred during the testing period. |
| serverTestDurationSeconds | number | Test duration per each server. Note: additional 2.5 seconds will be added in the beginning of the test to allow the stream to stabilize. |
