# Backup/Restore

`window.host`

## `queryUserEnvironmentBackupReferencedFiles (BackupRequestInfo, ResultCallback<BackupContentInfo>)`

**Available since API version 2.2**

Query user environment (scene collections, media files) for referenced files.

This method can be used to determine whether referenced files requested by a backup operation are too large to be included in the backup package.

## `createUserEnvironmentBackupPackage(BackupRequestInfo, ResultCallback<BackupContentInfo>)`

**Available since API version 1.22**

Create user environment (profiles, scene collections, media files) backup package.

## `queryUserEnvironmentBackupPackageContent(BackupContentInfo, ResultCallback<BackupContentInfo>)`

**Available since API version 1.22**

Retrieve scene collection names & profile names from backup package.

**Note:** If *BackupContentInfo.url* points to remote package, it will be downloaded and a new *BackupContentInfo.url* pointing to the local downloaded file will be generated.

## `restoreUserEnvironmentBackupPackageContent(BackupContentInfo, ResultCallback<success>)`

**Available since API version 1.22**

Restore backup package from *BackupContentInfo* previously obtained via a call to *queryUserEnvrironmentBackupPackageContent*.

**Note:** If *BackupContentInfo.url* points to remote package, it will be downloaded first.

**Important:** the restore operation will cause OBS to restart!

**Important:** attempt to restore while OBS is streaming or recording **will fail**.
