# BackupRequestInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| sceneCollections | SceneCollectionInfo[] | Optional. List of scene collections to back up. If empty or not present, all scene collections will be backed up. |
| profiles | ProfileInfo[] | Optional. List of profiles to back up. If empty or not present, all profiles will be backed up. |
| includeReferencedFiles | bool | Optional. If true – includes referenced (mostly media) files in the backup, otherwise omits them. Default: true. |
