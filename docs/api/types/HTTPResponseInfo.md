# HTTPResponseInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| success | boolean | true/false<br>Does not apply when responding to HTTP requests, only when requesting data from HTTP servers. |
| body | string | Response body |
| statusText | string | Request status text |
| statusCode | number | HTTP status code |
| headers | object | Optional. Key -> Value map of HTTP response headers.<br>Applies when responding to requests made via HTTP listener service.<br>**API 1.37+** |
