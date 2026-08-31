# MessageBusMessageInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| scope | string | Message scope (“broadcast”) |
| source | string | Message source (“web”) |
| sourceAddress | string | Message source address. In case of “web” source, this will be the URL of the web page which generated the message. |
| message | object | The actual message payload |
