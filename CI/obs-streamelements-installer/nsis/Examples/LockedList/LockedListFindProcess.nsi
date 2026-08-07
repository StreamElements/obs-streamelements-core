Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user
ShowInstDetails show

!include MUI2.nsh

!define MUI_FINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Section
  LockedList::FindProcess notepad.exe
  Pop $R0
  ${If} $R0 != ``
    DetailPrint $R0
    Pop $R0
    DetailPrint $R0
    Pop $R0
    DetailPrint $R0
  ${EndIf}
SectionEnd