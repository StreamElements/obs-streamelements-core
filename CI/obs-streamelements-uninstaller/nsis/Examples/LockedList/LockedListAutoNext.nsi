Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user

!include MUI2.nsh

!define MUI_PAGE_CUSTOMFUNCTION_LEAVE WelcomeLeave
!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow LockedListLeave
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Function WelcomeLeave
  StrCpy $R1 0
FunctionEnd

Function LockedListShow
  StrCmp $R1 0 +2 ; Skip the page if clicking Back from the next page.
    Abort
  !insertmacro MUI_HEADER_TEXT `LockedList Test` `Using AddModule and notepad.exe`
  LockedList::AddModule \notepad.exe
  LockedList::Dialog /autonext
  Pop $R0
FunctionEnd

Function LockedListLeave
  StrCpy $R1 1
FunctionEnd

Section
SectionEnd