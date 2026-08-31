# QuerySoftwareUpdateArgs

| **Property** | **Type** | **Description** |
| --- | --- | --- |
| allowDowngrade | bool | Optional. When *true* – allow downgrade to version older than the current.<br>Default: false<br>**API 1.29+** |
| forceInstall | bool | Optional. When *true* – force installation process to begin without prompting the user.<br>Default: false<br>**API 1.29+** |
| allowUseLastResponse | bool | Optional. When *false* – do not respect the “do not ask me again” user setting of the update prompt dialog.<br>Default: true<br>**API 1.29+** |
