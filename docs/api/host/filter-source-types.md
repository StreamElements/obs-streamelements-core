# Filter source types

`window.host`

## `getAvailableFilterSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`

Available since API version 6.0

Get a list of available audio or video filter source types.

Filter source types can be added as items to the *filters* collection of a *SceneItemInfo*.

## `getAvailableAudioFilterSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`

Available since API version 6.0

Get a list of available audio filter source types.

Filter source types can be added as items to the *filters* collection of a *SceneItemInfo*.

## `getAvailableVideoFilterSourceTypes(ResultCallback<InputSourceTypeInfo[]>)`

Available since API version 6.0

Get a list of available video filter source types.

Filter source types can be added as items to the *filters* collection of a *SceneItemInfo*.

## `getFilterSourceProperties(InputSourceTypeInfo, ResultCallback<InputSourceTypeInfo | null>)`

**Available since API version 6.0**

Get **filter** source type’s properties.

This works by creating a filter source of *InputSourceTypeInfo.class* with *InputSourceTypeInfo.settings* applied to it, and returning an *InputSourceTypeInfo* with *InputSourceTypeInfo.properties* field adjusted to the source settings.
