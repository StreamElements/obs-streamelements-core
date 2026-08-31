# HTTPRequestInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| method | string | HTTP request method (‘GET’ or ‘POST’)<br>**API 6.0** adds support for ‘PATCH’, ‘PUT’, ‘DELETE’, ‘OPTIONS’ |
| url | string | HTTP/HTTPS request URL.<br>May contain only the path & query string fragments when listening to HTTP requests with an HTTP service. |
| headers | object | Dictionary of HTTP request headers |
| body | string | Request body for ‘POST’ requests |
| query | object | Optional. *Key -> value* map of query string parameters.<br>Applies when listening to HTTP requests with an HTTP service.<br>**API 1.37+** |
| id | string | Optional. HTTP request unique identifier.<br>Applies when listening to HTTP requests with an HTTP service.<br>**API 1.37+** |
