# SceneCollectionInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Scene collection Id.<br>This is set by the API and is ignored when specified by the caller.<br>The value is not globally unique and may change between OBS sessions. |
| name | string | Scene collection name, as seen in the “Scene Collections” OBS menu. |
| referencedFiles | BackupReferencedFileInfo[] | Only appears in results from *queryUserEnvironmentBackupReferencedFiles* API call.<br>**API 2.2+** |
