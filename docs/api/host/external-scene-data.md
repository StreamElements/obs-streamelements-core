# External Scene Data

`window.host`

This section describes API methods available to retrieve scene data from external sources, such as SLOBS.

## `getExternalSceneDataProviders(ResultCallback<ExternalSceneDataProviderInfo>)`

**Available since API version 1.15**

Get available external scene data providers. Different data providers understand and read data from different sources.

**Data structures:** [`ExternalSceneDataProviderInfo`](../types/ExternalSceneDataProviderInfo.md)

## `getExternalSceneDataSceneCollections(ExternalSceneDataProviderInfo, ResultCallback<ExternalSceneDataSceneCollectionInfo>)`

**Available since API version 1.15**

Get scene collections available through a specific external scene data provider. Use this method to retrieve a list of scene collections for specific scene data provider.

**Data structures:** [`ExternalSceneDataProviderInfo`](../types/ExternalSceneDataProviderInfo.md), [`ExternalSceneDataSceneCollectionInfo`](../types/ExternalSceneDataSceneCollectionInfo.md)

## `getExternalSceneDataSceneCollectionContent(ExternalSceneDataSceneCollectionInfo, ResultCallback<ExternalSceneDataSceneCollectionContent>)`

**Available since API version 1.15**

Get specific external scene data provider’s specific scene collection content. The resulting object contains the provider Id, collection Id, collection name, metadata files and their content, and any referenced local files with a special URL which can be used to access those local files using standard XMLHttpRequest2 API.

**Data structures:** [`ExternalSceneDataSceneCollectionInfo`](../types/ExternalSceneDataSceneCollectionInfo.md), [`ExternalSceneDataSceneCollectionContent`](../types/ExternalSceneDataSceneCollectionContent.md)
