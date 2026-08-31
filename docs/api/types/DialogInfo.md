# DialogInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| id | string | Optional. **Non-Modal** dialog identifier. Can be set while creating a non-modal dialog. If a non-modal dialog with the same *id* exists, a new, unique *id* will be generated instead.<br>**Available since API version 3.3** |
| url | string | Dialog content URL |
| executeJavaScriptOnLoad | string | *Optional*. JavaScript code to execute on *each* page load |
| width | number | *Optional.* Dialog width |
| height | number | *Optional.* Dialog height |
| title | string | *Optional.* Dialog title |
| incognitoMode | bool | *Optional.* If set and is set to *true*, the modal dialog will open in incognito mode (no cache & separate, volatile cookie store)<br>**Available since API version 1.7** |
| isResizable | bool | *Optional*. If set and is set to *true*, the dialog will be user-resizable. Otherwise, it will be fixed-size.<br>**Available since API version 6.0** |
