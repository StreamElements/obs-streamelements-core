Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user
ShowInstDetails show

!include MUI2.nsh
!include x64.nsh

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Section

  ${If} ${RunningX64}
    File /oname=$PLUGINSDIR\LockedList64.dll `${NSISDIR}\Plugins\LockedList64.dll`
  ${EndIf}

  Exec `"$SYSDIR\notepad.exe"`
  Sleep 2000
  LockedList::CloseProcess notepad2.exe

  Exec `"$SYSDIR\notepad.exe"`
  Sleep 2000
  LockedList::CloseProcess /kill notepad2.exe

SectionEnd