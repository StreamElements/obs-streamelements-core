# Invoke API methods in Batch Mode

`window.host`

## `batchInvokeSeries(InvokeInfo[], ResultCallback<result[]>)`

Invoke API methods specified by array of *InvokeInfo* structures in series and return all results as an array.

In case *InvokeInfo.invoke* refers to an API method which does not exist, the call result for that API method will be *null*.

Saving of OBS front-end state will be suspended before API methods execution begins, and will be resumed once execution of all methods in the array completes.

**Available since API version 1.35**

**Data structures:** [`InvokeInfo`](../types/InvokeInfo.md)
