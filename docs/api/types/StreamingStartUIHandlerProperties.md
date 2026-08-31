# StreamingStartUIHandlerProperties

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| autoStart | boolean | If true, streaming will start automatically when requested via UI, otherwise, will only trigger the hostStreamingStartRequested event and wait for JavaScript to react. |
| requestAcknowledgeTimeoutSeconds | number | Number of seconds to wait before JavaScript responds with a call to<br>adviseStreamingStartUIRequestAccepted (ResultCallback\<success>) or adviseStreamingStartUIRequestRejected (ResultCallback\<success>)<br>or requestStreamingStart(ResultCallback\<success>) |
