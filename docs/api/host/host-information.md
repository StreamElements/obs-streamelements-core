# Host information

`window.host`

## `getHostProperties(ResultCallback<HostInfo>)`

Available since API version 1.4

Get host platform properties, including platform, architecture, component versions & API version.

**Data structures:** [`HostInfo`](../types/HostInfo.md)

## `getHostCapabilities(ResultCallback<HostCapabilities>)`

Available since API version 1.8

Get host platform capabilities.

**Note:** documented since API 1.8 but only implemented in API 6.8. Nothing
could depend on the original shape, so the documented `sceneCollections`
member is retained as specified and the structured `razerWyvrn` member was
added beside it.

This is deliberately the only place [Razer WYVRN](razer-wyvrn.md)
availability is reported — a separate status call would be a second source of
truth that could disagree with this one.

**Windows only.** Registered only in builds that include Razer WYVRN. On macOS
the integration is compiled out, so no handler by this name is registered and
the host reports API version 6.7.

**Data structures:** [`HostCapabilities`](../types/HostCapabilities.md)
