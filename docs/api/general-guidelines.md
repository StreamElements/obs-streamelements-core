# General guidelines

Before using any of the API methods, the caller must make sure the API is ready. This is achieved by testing *window.host.hostReady* === true, and listening for *window.addEventListener(‘hostReady’, function(e) {})* event in case the condition evaluates to *false*.

Example:

*function obsApiReady(callback) {*

*if (window.host && window.host.hostReady) {*

*callback();
} else {*

*window.addEventListener(‘hostReady’, function(e) {
        callback();*

*});
}*

*}*

API method calls are asynchronous. They accept a JavaScript callback as their last parameter.

The callback always receives a parameter indicated in the function signature.
