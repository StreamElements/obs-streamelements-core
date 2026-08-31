# HostInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| obsVersionString | string | OBS version |
| cefVersionString | string | CEF version |
| cefPlatformApiHash | string | CEF platform API hash |
| cefUniversalApiHash | string | CEF universal API hash |
| hostPluginVersionString | string | OBS Live version |
| hostApiVersionString | string | OBS Live API version |
| platform | string | Target platform (“windows”, “macos”, “linux”, “other”) |
| platformArch | string | Platform architecture (“64bit”, “32bit”) |
| hostMachineUniqueId | string | Unique machine identifier.<br>**Available since API version 1.5**<br>Consists of either windows installation UniqueID or a generated GUID in case installation UniqueID is not available. |
| hostSessionUniqueId | string | Unique session identifier.<br>**Available since API version 1.5** |
