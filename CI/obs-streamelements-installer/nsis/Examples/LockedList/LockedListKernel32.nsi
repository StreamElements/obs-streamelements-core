Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user

!include MUI2.nsh
!include x64.nsh

!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Function LockedListShow
  !insertmacro MUI_HEADER_TEXT `LockedList Test` `Using AddModule and kernel32.dll`
  ${If} ${RunningX64}
    File /oname=$PLUGINSDIR\LockedList64.dll `${NSISDIR}\Plugins\LockedList64.dll`
  ${EndIf}
  LockedList::AddModule \kernel32.dll
  LockedList::Dialog /ignore Ignore!
  Pop $R0
FunctionEnd

Section
SectionEnd