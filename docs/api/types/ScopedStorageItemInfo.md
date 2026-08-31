# ScopedStorageItemInfo

**API 5.0+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| scope | string | Scope to which the storage item belongs. Usually translates to “user id”. |
| container | string | Container under “scope” to which the storage item belongs. Usually translates to “service/facility name/id” |
| item | string | Storage item name. Only required for operations on specific storage items. |
| content | any | Storage item content. Only required when writing content to a storage item. JSON storage item functions treat this field as a JavaScript object. |
| contentLength | number | Read-only. Storage item content length. Returned in *getAllScopedStorageTextItems* API call response. |
