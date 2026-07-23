Name `SelfDel plug-in example`
OutFile Example.exe
XPStyle on

; Administrator privileges required.
RequestExecutionLevel admin

Page InstFiles

Section
  ;SelfDel::Del /RMDIR
  ;SelfDel::Del /REBOOT
  ;SelfDel::Del /SHUTDOWN
  SelfDel::Del
  SetAutoClose true
SectionEnd