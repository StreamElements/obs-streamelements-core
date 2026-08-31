# Scoped Storage

`window.host`

Scoped storage allows storing data files under a “scope” and a “container”, which should usually translate as “scope” = “logged in user id” and “container” = “service or facility which is storing and accessing the data”.

Text files are later parsed by backup/restore functions, file paths are extracted, and relevant files are included in the backup.

This allows leveraging backup/restore facilities to transfer user-specific information between machines.

## `writeScopedStorageJsonItem(ScopedStorageFileInfo, ResultCallback< ScopedStorageFileInfo | null>)`

Write content to scoped storage item. It is recommended to keep the item content in JSON format.

**Available since API version 5.0**

## `readScopedStorageJsonItem(ScopedStorageItemInfo, ResultCallback< ScopedStorageItemInfo | null>)`

Read content of scoped storage text item.

**Available since API version 5.0**

**Data structures:** [`ScopedStorageItemInfo`](../types/ScopedStorageItemInfo.md)

## `removeScopedStorageJsonItem(ScopedStorageItemInfo, ResultCallback< ScopedStorageItemInfo | null>)`

Remove a scoped storage item.

**Available since API version 5.0**

**Data structures:** [`ScopedStorageItemInfo`](../types/ScopedStorageItemInfo.md)

## `getAllScopedStorageJsonItems(ScopedStorageItemInfo, ResultCallback< ScopedStorageItemInfo [] | null>)`

Get list of scoped storage items from specific scope and container.

**Available since API version 5.0**

**Data structures:** [`ScopedStorageItemInfo`](../types/ScopedStorageItemInfo.md)
