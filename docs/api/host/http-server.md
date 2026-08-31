# HTTP server

`window.host`

## `addBrowserScopedHttpServer(GenericNetworkServiceInfo, ResultCallback<GenericNetworkServiceInfo>)`

Creates an HTTP server listener in the scope of the current browser (current page). When the browser is closed, the HTTP server is destroyed along with it.

The method accepts a *portNumber* and *ipAddress* to listen on in the *GenericNetworkServiceInfo* structure.

In case *portNumber* is missing or \<= 0, a random port number will be assigned.

*ipAddress* can have either “127.0.0.1” or “0.0.0.0” as value. In the former case, only localhost connections will be accepted, in the latter – any network connection on any of the machine’s IP addresses.

The resulting structure contains an *id* field which uniquely identifiers the HTTP server for subsequent API calls.

HTTP requests will be translated into hostMessageReceived events. Event *scope* will be set to “browser”, *source* will be set to “http”, *sourceAddress* to “urn:http:server:browser” and *payload* to an *HTTPRequestInfo* structure describing the contents of the HTTP request.

**HTTP Response:**

To send an HTTP response, invoke the *sendHttpRequestResponse* API call with *HTTPRequestInfo.id* as first argument, and *HTTPResponseInfo* as second.

**Security consideration:**

Remove the listening HTTP service before navigating away to another domain.

**Available since API version 1.37**

## `getAllBrowserScopedHttpServers(ResultCallback<GenericNetworkServiceInfo[]>)`

Retrieve a list of currently running HTTP servers scoped to the current browser.

**Available since API version 1.37**

## `removeBrowserScopedHttpServersByIds(<string|string[]>, ResultCallback<success>)`

Remove one or more of HTTP servers scoped to the current browser by *id*.

**Available since API version 1.37**

## `sendHttpRequestResponse(string<id>, HTTPResponseInfo, ResultCallback<success>)`

Send a response to an HTTP request received by a *hostMessageReceived* event handler (originating in embedded HTTP listener server).

*id* = value of HTTPRequestInfo.id field of the incoming HTTP request

*HTTPResponseInfo* = HTTP response content

**Timeout:**

The HTTP listener service waits for up to 15 seconds for a response to be sent. Afterwards it responds with a default HTTP 404 Request Not Handled status (a variation of HTTP 404 Not Found).

**Available since API version 1.37**
