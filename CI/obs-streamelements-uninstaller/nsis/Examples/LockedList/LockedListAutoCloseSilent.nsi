Name `LockedList Test`
OutFile LockedListTest.exe
ShowInstDetails show

!include MUI2.nsh

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Function SilentSearchCallback

  Pop $R0
  Pop $R1
  Pop $R2

  ${If} $R0 == -1
    MessageBox MB_OK|MB_ICONEXCLAMATION `Error auto closing $R2!`
  ${ElseIf} ${Cmd} `MessageBox MB_YESNO|MB_ICONQUESTION 'Close $R2? (Id: $R0, Path: $R1)' IDYES`
    Push autoclose
  ${Else}
    Push false
  ${EndIf}

FunctionEnd

Section
  LockedList::AddClass MozillaWindowClass
  LockedList::AddClass Chrome_WidgetWin_0
  LockedList::AddClass IEFrame
  GetFunctionAddress $R0 SilentSearchCallback
  LockedList::SilentSearch $R0
  Pop $R0
  DetailPrint $R0
  SetAutoClose false
SectionEnd