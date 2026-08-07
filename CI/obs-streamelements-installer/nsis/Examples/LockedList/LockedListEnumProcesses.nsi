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

Function EnumProcessesCallback
  Pop $R0
  Pop $R1
  Pop $R2

  DetailPrint `Id: $R0`
  DetailPrint `Path: $R1`
  DetailPrint `Description: $R2`

  Push true
FunctionEnd

Section
  GetFunctionAddress $R0 EnumProcessesCallback
  LockedList::EnumProcesses $R0
SectionEnd