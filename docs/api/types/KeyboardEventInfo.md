# KeyboardEventInfo

**API 1.36+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| type | string | Event type:<br>*rawkeydown*<br>*keydown*<br>*keyup*<br>*keypress*<br>**Note**: in most cases you’ll want to send *keydown* followed by *keypress* followed by *keyup* with the pressed key indicated by *key* or *charCode*. |
| key | string (1 char) | **Optional**. The character which was pressed.<br>**Note**: if you specify this property, you must not specify *charCode* since it will override the value of this property. |
| charCode | number | **Optional**. Character code of the character which was pressed.<br>**Note**: this property overrides the value of *key*. |
| code | number | **Optional**. Platform-specific virtual key code.<br>It is unlikely you’ll want to use this property, however, it is supported for the sake of completeness. |
| altKey | bool | **Optional**. Alt key is pressed |
| ctrlKey | bool | **Optional**. Ctrl key is pressed |
| metaKey | bool | **Optional**. Windows or Command key are pressed |
| shiftKey | bool | **Optional**. Shift is pressed |
| capsLock | bool | **Optional**. Caps-lock is active |
| numLock | bool | **Optional**. Num-lock is active |
| leftMouseButton | bool | **Optional**. Primary (left) mouse button is pressed |
| rightMouseButton | bool | **Optional**. Secondary (right) mouse button is pressed |
| auxiliaryMouseButton | bool | **Optional**. Auxiliary (middle) mouse button is pressed |
| location | string | **Optional**. Indicates location of the pressed key if relevant.<br>*left* – left key<br>*right* – right key<br>*numPad* / *keyPad* – numeric keypad |
