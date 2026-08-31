# HotkeyBindingInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | number | Hotkey binding Id |
| name | string | Internal hotkey binding name (example: “OBSBasic.StartStreaming”) |
| description | string | Localized hotkey binding description (example: “Start Streaming”) |
| registererTypeId | number | Internal hotkey binding registerer type (frontend, source, encoder, output, service) Id. |
| registererTypeName | string | Hotkey binding registerer type name (“frontend”, “source”, “encoder”, “output”, “service”) |
| registererName | string | Hotkey binding registerer name (if registererType is not ‘frontend’. For example: source (scene) name). |
| partnerHotkeyBindingId | number | Used for hotkey pairs. Specifies the hotkey binding id of the other partner in a hotkey pair. |
| triggers | KeyCombinationInfo[] | Array of hotkey triggers (actual bound key combinations). |
| eventDetail | object | Data to be passed to *hostHotkeyPressed* and *hotHotkeyReleased* events *detail* property. |
