# MouseEventInfo

**API 1.36+**

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| type | string | Event type:<br>*mousedown*<br>*mouseup*<br>*mousemove*<br>*wheel*<br>**Note**: to generate a mouse click, you’ll want to send ‘*mousedown’* followed by ‘*mouseup*’.<br>To generate a doubleclick, you’ll want to send ‘*mousedown*’ followed by ‘*mouseup*’ and then ‘*mousedown*’ + ‘*mouseup*’ with ‘*eventCount*’ = 2. |
| button | string | Required when *type = mousedown* or *type = mouseup*. Otherwise has no meaning.<br>*left* – left mouse button<br>*right* – right mouse button<br>*middle / auxiliary* – aux / middle mouse button |
| x | number | Mouse cursor X position relative to browser top-left corner. |
| y | number | Mouse cursor Y position relative to browser top-left corner. |
| deltaX | number | **Optional**. Has meaning only when *type = wheel*. Indicates horizontal wheel movement delta. |
| deltaY | number | **Optional**. Has meaning only when *type = wheel*. Indicates horizontal wheel movement delta. |
| eventCount | number | **Optional**. Has meaning only when *type = mousedown* or *type = mouseup*. Indicates the number of events to consider.<br>Doubleclick will indicate *eventCount = 2* on the second *mousedown/mouseup* pair. |
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
