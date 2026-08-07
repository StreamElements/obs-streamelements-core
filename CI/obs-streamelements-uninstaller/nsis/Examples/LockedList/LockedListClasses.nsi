Name `LockedList Test`
OutFile LockedListTest.exe

!include MUI2.nsh

!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Function LockedListShow
  !insertmacro MUI_HEADER_TEXT `LockedList Test` `Using AddModule and notepad.exe`
  LockedList::AddClass *Mozilla*
  LockedList::Dialog
  Pop $R0
FunctionEnd

Section
SectionEnd