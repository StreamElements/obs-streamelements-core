# GenericNetworkServiceInfo

**API 1.37+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Network service Id.<br>This is set by the API when starting the network service, and is required for subsequent service shutdown & management calls to uniquely identify a network service. |
| portNumber | number | Optional. Port number to listen on. |
| ipAddress | string | Optional. IP address to listen on. Default is “127.0.0.1”, to allow connections from other computers on the network, specify “0.0.0.0”. |
