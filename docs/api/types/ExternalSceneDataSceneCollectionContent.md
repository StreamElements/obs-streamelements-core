# ExternalSceneDataSceneCollectionContent

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| providerId | string | Scene data provider Id |
| collectionId | string | Scene collection Id |
| name | string | Scene collection name |
| metadataFiles | array |  |
| metadataFiles[].path | string | Metadata file path |
| metadataFiles[].content | string | Metadata file content |
| referencedFiles | array |  |
| referencedFiles[].path | string | Referenced file path |
| referencedFiles[].url | string | Referenced file URL<br>This URL can be used to read the file contents with XMLHttpRequest2<br>The URL is valid only for the current session. The next time the application is restarted, the URL will be invalid. |
