!include MUI2.nsh
!include WinVer.nsh

; The locked file to test on.
!define THE_FILE $TEMP\Locked.tmp
; Please lock the file for me (i.e. it isn't currently locked).
!define LOCK_THE_FILE

Name LockedListTest
OutFile LockedListTest.exe
RequestExecutionLevel user
ShowInstDetails show

!define MUI_FINISHPAGE_NOAUTOCLOSE

!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_COMPONENTS
Page Custom LockedListPageShow
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_WELCOME
UninstPage Custom un.LockedListPageShow
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_UNPAGE_FINISH

!insertmacro MUI_LANGUAGE English

ReserveFile `${NSISDIR}\Plugins\LockedList.dll`

Function .onInit

  ${Unless} ${AtLeastWinNt4}
    MessageBox MB_OK|MB_ICONSTOP `You cannot run this installer on < Win NT`
    Abort
  ${EndUnless}

  InitPluginsDir

FunctionEnd

Section `Install test` Section_InstallTest

SectionEnd

Section `Uninstall test` Section_UninstallTest

  DetailPrint `Launching uninstaller...`
  WriteUninstaller $EXEDIR\LockedListUninstallTest.exe
  ExecWait `"$EXEDIR\LockedListUninstallTest.exe" _?=$EXEDIR`
  Delete $EXEDIR\LockedListUninstallTest.exe

SectionEnd

Function SilentSearchCallback
  Pop $R0
  Pop $R1
  Pop $R2

  DetailPrint `Id: $R0`
  DetailPrint `Path: $R1`
  DetailPrint `Description: $R2`

  Push true
FunctionEnd

Section `SilentSearch test` Section_SilentSearchTest

  DetailPrint `Testing LockedList without threading, please wait...`

  LockedList::AddFile `${THE_FILE}`
  LockedList::AddModule $PLUGINSDIR\LockedList.dll

  # Begin the search now.
  GetFunctionAddress $R0 SilentSearchCallback
  LockedList::SilentSearch $R0

  DetailPrint `Searching... 100%`

SectionEnd

Section `SilentSearch asynchronous test` Section_SilentSearchThreadTest

  DetailPrint `Testing LockedList with threading, please wait...`

  LockedList::AddFile `${THE_FILE}`
  LockedList::AddModule $PLUGINSDIR\LockedList.dll

  # Begin the search in a separate thread.
  GetFunctionAddress $R0 SilentSearchCallback
  LockedList::SilentSearch /async $R0

  # Loop while the search takes place. We could do other stuff here.
  ${Do}
    LockedList::SilentWait /time 500
    Pop $R0
  ${LoopWhile} $R0 == wait

  DetailPrint `Searching... 100%`

SectionEnd

Section Uninstall
SectionEnd

Function LockedListPageShow

  ${IfNot} ${SectionIsSelected} ${Section_InstallTest}
    Abort
  ${EndIf}

!ifdef LOCK_THE_FILE

  !tempfile TEMP
  !appendfile `${TEMP}` `Name LockFile$\r$\n`
  !appendfile `${TEMP}` `Caption "File locked for testing"$\r$\n`
  !appendfile `${TEMP}` `OutFile ${TEMP}.exe$\r$\n`
  !appendfile `${TEMP}` `XPStyle on$\r$\n`
  !appendfile `${TEMP}` `SilentInstall silent$\r$\n`
  !appendfile `${TEMP}` `Function .onInit$\r$\n`
  !appendfile `${TEMP}` `FileOpen $R0 "${THE_FILE}" w$\r$\n`
  !appendfile `${TEMP}` `MessageBox MB_OK "Click OK to unlock the file ${THE_FILE}."$\r$\n`
  !appendfile `${TEMP}` `FileClose $R0$\r$\n`
  !appendfile `${TEMP}` `Delete "${THE_FILE}"$\r$\n`
  !appendfile `${TEMP}` `FunctionEnd$\r$\n`
  !appendfile `${TEMP}` `Section$\r$\n`
  !appendfile `${TEMP}` `SectionEnd$\r$\n`
  !execute `"${NSISDIR}\makensis.exe" "${TEMP}"`
  File /oname=$PLUGINSDIR\LockFile.exe `${TEMP}.exe`
  !delfile `${TEMP}`

  Exec `"$PLUGINSDIR\LockFile.exe"`

  Sleep 1000
  BringToFront

!endif

  !insertmacro MUI_HEADER_TEXT `LockedList install dialog` `This is a list of programs that have our files held hostage...`

  LockedList::AddFile `${THE_FILE}`
  LockedList::Dialog /ignore ``
  Pop $R0

FunctionEnd

Function un.LockedListPageShow

  !insertmacro MUI_HEADER_TEXT `LockedList uninstall dialog` `This is a list of programs that have our files held hostage...`

  LockedList::AddFile `${THE_FILE}`
  LockedList::Dialog /ignore ``
  Pop $R0

FunctionEnd

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${Section_InstallTest} `Tests the LockedList plug-in in this installer executable.`
  !insertmacro MUI_DESCRIPTION_TEXT ${Section_UninstallTest} `Tests the LockedList plug-in in the dummy uninstaller executable.`
  !insertmacro MUI_DESCRIPTION_TEXT ${Section_SilentSearchTest} `Calls SilentSearch and waits for it to finish.`
  !insertmacro MUI_DESCRIPTION_TEXT ${Section_SilentSearchThreadTest} `Calls SilentSearch using threading to showing progress indication.`
!insertmacro MUI_FUNCTION_DESCRIPTION_END