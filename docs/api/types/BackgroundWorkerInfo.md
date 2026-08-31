# BackgroundWorkerInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Background worker id |
| content | string | Worker content (HTML)<br>**Removed since API 2.0+** |
| url | string | Worker content assumed URL.<br>**Note**: the URL is not being accessed.<br>It is used to determine the *security context* of the worker.<br>See Background workers section for more information. |
| executeJavaScriptOnLoad | string | JavaScript code to execute on page load<br>**API 1.27+** |
