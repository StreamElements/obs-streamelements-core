# HTTP Requests

`window.host`

## `httpRequestText(HTTPRequestInfo, ResultCallback<HTTPResponseInfo>)`

**Available since API version 1.16**

Make the specified HTTP request and return response information.

This method can be used to make cross-domain HTTP requests with custom HTTP headers.

**Note:** this method should be used with care:

- Subsequent API calls in the current window are *blocked* until the request completes or fails.

- All request and response data is sent and interpreted as UTF-8 encoded text. Binary responses may lead to empty results if UTF-8 decoding fails.

**Data structures:** [`HTTPRequestInfo`](../types/HTTPRequestInfo.md), [`HTTPResponseInfo`](../types/HTTPResponseInfo.md)
