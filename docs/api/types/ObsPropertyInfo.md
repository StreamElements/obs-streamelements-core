# ObsPropertyInfo

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| name | string | OBS property name |
| label | string | OBS property localized label |
| description | string | OBS property localized description |
| dataType | string | OBS data type:<br>**bool**<br>**integer**<br>**float**<br>**string**<br>**font**<br>**array**<br>**frame_rate**<br>**group** (*not a real data type, only indication that a property is a group*) |
| controlType | string | checkbox<br>number<br>text |
| controlMode | string | For controlType == number:<br>**scroller**<br>**slider**<br>For controlType == text:<br>**text**<br>**password**<br>**textarea**<br>**color**<br>For controlType == path:<br>**open**<br>**save**<br>**folder**<br>For controlType == select:<br>**dynamic**<br>**static**<br>For controlType == list:<br>**dynamic**<br>For controlType == group:<br>**normal**<br>**checkable** |
| defaultValue | any | Property default value |
| valueFormat | string | Property value format |
| items | array | For controlType == select, array of:<br>**name**<br>**enabled**<br>**value** (*integer/float/string*)<br>For controlType == group, array of:<br>**ObsPropertyInfo** |
