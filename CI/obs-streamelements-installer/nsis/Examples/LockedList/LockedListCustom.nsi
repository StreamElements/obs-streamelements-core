Name `LockedList Test`
OutFile LockedListTest.exe
RequestExecutionLevel user

!include MUI2.nsh

!insertmacro MUI_PAGE_WELCOME
Page Custom LockedListShow
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_LANGUAGE English

Function .onInit
  InitPluginsDir
  File /oname=$PLUGINSDIR\myapp.ico `${NSISDIR}\Contrib\Graphics\Icons\arrow2-install.ico`
FunctionEnd

Function LockedListShow
  !insertmacro MUI_HEADER_TEXT `LockedList Test` `Using AddModule and notepad.exe`
  GetFunctionAddress $R0 MyAppCallback
  LockedList::AddCustom /icon $PLUGINSDIR\myapp.ico `My App v1.0` myapp.exe $R0
  LockedList::Dialog
  Pop $R0
FunctionEnd

Function MyAppCallback
  Pop $R0

  ; Message box instead of actual logical test...
  ${If} ${Cmd} `MessageBox MB_YESNO|MB_ICONQUESTION 'Is $R0 running?' IDYES`
    Push true
  ${Else}
    Push false
  ${EndIf}

FunctionEnd

Section `IsFileLocked test` Section_IsFileLockedTest

  ; IsFileLocked test #1.
  FileOpen $R1 $PLUGINSDIR\Lock.tmp w
  LockedList::IsFileLocked $PLUGINSDIR\Lock.tmp
  Pop $R0
  ${If} $R0 == true
    MessageBox MB_OK|MB_ICONINFORMATION `$PLUGINSDIR\Lock.tmp is locked.`
  ${Else}
    MessageBox MB_OK|MB_ICONINFORMATION `$PLUGINSDIR\Lock.tmp is NOT locked!!??`
  ${EndIf}
  FileClose $R1

  ; IsFileLocked test #2.
  LockedList::IsFileLocked $PLUGINSDIR\Lock.tmp
  Pop $R0
  ${If} $R0 == true
    MessageBox MB_OK|MB_ICONINFORMATION `$PLUGINSDIR\Lock.tmp IS locked!!??`
  ${Else}
    MessageBox MB_OK|MB_ICONINFORMATION `$PLUGINSDIR\Lock.tmp is not locked.`
  ${EndIf}

SectionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Section_IsFileLockedTest} `Tests IsFileLocked on a locked file.`
!insertmacro MUI_FUNCTION_DESCRIPTION_END