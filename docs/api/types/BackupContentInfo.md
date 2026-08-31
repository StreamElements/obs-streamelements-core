# BackupContentInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| url | string | Backup package URL. May be local (secure, digitally signed) or remote URL. |
| sceneCollections | SceneCollectionInfo[] | List of scene collections in the backup package. When restoring, indicates which scene collections to restore.<br>When querying for referenced files with *queryUserEnvironmentBackupReferencedFiles*, each *SceneCollectionInfo* gets another property named *referencedFiles* of type *BackupReferencedFileInfo[]*. |
| profiles | ProfileInfo[] | List of profiles in the backup package. When restoring, indicates which profiles to restore. |
