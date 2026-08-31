# SharedVideoCompositionInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Unique shared video composition ID: corresponds to canvas UUID assigned by OBS |
| name | string | Shared video composition name: corresponds to canvas name visible in OBS frontend |
| videoCompositionId | string \| null | Read-only. Corresponds to connected video composition ID or null if none is connected |
| canChange | boolean | Read-only. True if shared video composition can be changed, False otherwise.<br>When streaming/recording is active in OBS, we don’t allow changes to the shared video composition |
| canRemove | boolean | Read-only. True if shared video composition can be removed, False otherwise.<br>When streaming/recording is active in OBS, we don’t allow removing the shared video composition |
