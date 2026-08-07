Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user

!include MUI2.nsh

;!define MUI_UI modern_modified.exe

!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

!define IDC_BUTTON_IGNORE 1190

Function _PixelsToDialogUnits
Exch $R0
Exch
Exch $R1
Push $R2
  System::Call `*(i R1, i R0, i 0, i 0) i .R2`
  System::Call `user32::MapDialogRect(i $HWNDPARENT, i R2)`
  System::Call `*$R2(i, i, i, i) i (i .R0, i .R1)`
  System::Free $R2
Pop $R2
Exch $R1
Exch
Exch $R0
FunctionEnd

!macro _PixelsToDialogUnits X Y OutX OutY
  Push ${X}
  Push ${Y}
  Call _PixelsToDialogUnits
  Pop ${OutX}
  Pop ${OutY}
!macroend
!define PixelsToDialogUnits `!insertmacro _PixelsToDialogUnits`

Function LockedListShow

  System::Call `kernel32::GetModuleHandle(i 0) i .R4`
  ${PixelsToDialogUnits} 116 201 $R0 $R1
  ${PixelsToDialogUnits} 50 14 $R2 $R3
  System::Call `user32::CreateWindowEx(i 0, t 'BUTTON', t '&Ignore', i ${BS_PUSHBUTTON}|${WS_CHILD}|${WS_TABSTOP}|${WS_VISIBLE}, i R0, i R1, i R2, i R3, i $HWNDPARENT, i ${IDC_BUTTON_IGNORE}, i R4, i 0) i .s`
  Pop $R0

  GetDlgItem $R1 $HWNDPARENT 1
  SendMessage $R1 ${WM_GETFONT} 0 0 $R1
  SendMessage $R0 ${WM_SETFONT} $R1 0

  !insertmacro MUI_HEADER_TEXT `LockedList Test` `Using AddModule and notepad.exe`
  LockedList::AddCaption *Notepad*
  LockedList::Dialog /ignorebtn ${IDC_BUTTON_IGNORE}
  Pop $R1

  System::Call `user32::DestroyWindow(i R0)`

FunctionEnd

Section
SectionEnd