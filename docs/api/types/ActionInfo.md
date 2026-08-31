# ActionInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| type | string | “separator” – separator<br>“command” – command action item |
| icon | IconInfo | Optional. Action icon.<br>Ignored when type = “separator”. |
| title | string | Optional. Action title.<br>Ignored when type = “separator”. |
| tooltip | string | Optional. Action tooltip.<br>Ignored when type = “separator”. |
| color | string | Optional. Action text color.<br>Ignored when type = “separator”.<br>**API 1.32+** |
| invoke | string | API method to invoke when action is requested.<br>Valid only when type = “command”.<br>The value of this field corresponds to one of API method names described in this document.<br>In addition, for Scene Items, this field supports the following **macros**:<br>“**:defaultAction**” – Scene / Scene Item default action<br>“**:defaultContextMenu**” – Scene / Scene Item default context menu<br>“**:none**” – no action (**API 1.32+**)<br>Examples:<br>“showModalDialog”, “showCentralWidget”, “addDockingWidget”, “openPopupWindow”, etc. |
| invokeArgs | array | Arguments to the API method described by *invoke* as described by the API call documentation in this document.<br>Valid only when type = “command”. |

**Note:** either *title* or *url* must be specified if *type* != “separator”
