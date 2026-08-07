!include common.nsh
!include "FileFunc.nsh"
!include "TextFunc.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "WinVer.nsh"
!include "Version.generated.nsh"

!include "science.nsh"

# Installer file name
OutFile "obs-streamelements-setup.exe"

# Product display name
Name "${PRODUCT_NAME} v${PRODUCT_VERSION}"

# Brand on bottom of installer window
BrandingText "${PRODUCT_NAME} v${PRODUCT_VERSION} (Setup v${SETUP_VERSION})"

#
# Disable "Ignore" option in Abort/Retry/Ignore dialog
#
AllowSkipFiles off
FileErrorText "Can not write to file:$\r$\n$0$\r$\n$\r$\nOBS Studio or one of its components might not have finished shutting down yet.$\r$\n$\r$\nPlease wait for a few moments and try again."

Var /GLOBAL AutoStart

ReserveFile "resources\obs-probe-libobs-version-32.exe"
ReserveFile "resources\obs-probe-libobs-version-64.exe"

ReserveFile "resources\MicrosoftEasyFix51044.msi"

########################################################################
# Version information
########################################################################

    !define /date BUILD_YEAR "%Y"

    VIAddVersionKey /LANG=0 "ProductName" "${PRODUCT_SHORT_NAME}"

    VIAddVersionKey /LANG=0 "CompanyName" "${PRODUCT_PUBLISHER}"

    VIAddVersionKey /LANG=0 "LegalCopyright" "Copyright (C) 2018-${BUILD_YEAR} ${PRODUCT_PUBLISHER}"

    ; FileDescription is what shows in the UAC elevation prompt when signed
    VIAddVersionKey /LANG=0 "FileDescription" "${PRODUCT_NAME}"

    VIAddVersionKey /LANG=0 "FileVersion" "${PRODUCT_VERSION}"
    VIAddVersionKey /LANG=0 "ProductVersion" "${PRODUCT_VERSION}"

    VIProductVersion "${PRODUCT_VERSION}"
    VIFileVersion "${PRODUCT_VERSION}"

########################################################################
# Global vars
########################################################################

    Var /GLOBAL g_obsVersion
    Var /GLOBAL g_downloadUrl
    Var /GLOBAL g_downloadTargetPath
    Var /GLOBAL g_downloadText
    Var /GLOBAL g_obsTempPath32
    Var /GLOBAL g_obsTempPath64

    Var /GLOBAL g_scienceFields
    Var /GLOBAL g_scienceProps

########################################################################
# HEAP global vars
########################################################################

    !define HEAP_EVENT_PREFIX "OBS.Live Setup: "

    Var /GLOBAL g_instAutostart
    Var /GLOBAL g_instCurrentScreenName
    Var /GLOBAL g_instCurrentScreenIndex
    Var /GLOBAL g_instAbortSource
    Var /GLOBAL g_instAbortReason
    Var /GLOBAL g_instCreateDesktopShortCut
    Var /GLOBAL g_instObsExecProgram
    Var /GLOBAL g_instCompleted
    Var /GLOBAL g_instPrevEvent

########################################################################
# Utilities
########################################################################

    !define StrStr "!insertmacro StrStr"
    
    !macro StrStr ResultVar String SubString
    Push `${String}`
    Push `${SubString}`
    Call StrStr
    Pop `${ResultVar}`
    !macroend

    ; Usage:
    ; ${Trim} $trimmedString $originalString
    !define Trim "!insertmacro Trim"
    
    !macro Trim ResultVar String
    Push "${String}"
    Call Trim
    Pop "${ResultVar}"
    !macroend

    ; Trim
    ;   Removes leading & trailing whitespace from a string
    ; Usage:
    ;   Push 
    ;   Call Trim
    ;   Pop 
    Function Trim
        Exch $R1 ; Original string
        Push $R2
    
    Loop:
        StrCpy $R2 "$R1" 1
        StrCmp "$R2" " " TrimLeft
        StrCmp "$R2" "$\r" TrimLeft
        StrCmp "$R2" "$\n" TrimLeft
        StrCmp "$R2" "$\t" TrimLeft
        GoTo Loop2
    TrimLeft:	
        StrCpy $R1 "$R1" "" 1
        Goto Loop
    
    Loop2:
        StrCpy $R2 "$R1" 1 -1
        StrCmp "$R2" " " TrimRight
        StrCmp "$R2" "$\r" TrimRight
        StrCmp "$R2" "$\n" TrimRight
        StrCmp "$R2" "$\t" TrimRight
        GoTo Done
    TrimRight:	
        StrCpy $R1 "$R1" -1
        Goto Loop2
    
    Done:
        Pop $R2
        Exch $R1
    FunctionEnd

    Function VersionCompare
        !define VersionCompare `!insertmacro VersionCompareCall`

        ; Result:
        ;    0  Versions are equal
        ;    1  Version1 is newer
        ;    2  Version2 is newer
        !macro VersionCompareCall _VER1 _VER2 _RESULT
            Push `${_VER1}`
            Push `${_VER2}`
            Call VersionCompare
            Pop ${_RESULT}
        !macroend
    
        Exch $1
        Exch
        Exch $0
        Exch
        Push $2
        Push $3
        Push $4
        Push $5
        Push $6
        Push $7
    
        begin:
        StrCpy $2 -1
        IntOp $2 $2 + 1
        StrCpy $3 $0 1 $2
        StrCmp $3 '' +2
        StrCmp $3 '.' 0 -3
        StrCpy $4 $0 $2
        IntOp $2 $2 + 1
        StrCpy $0 $0 '' $2
    
        StrCpy $2 -1
        IntOp $2 $2 + 1
        StrCpy $3 $1 1 $2
        StrCmp $3 '' +2
        StrCmp $3 '.' 0 -3
        StrCpy $5 $1 $2
        IntOp $2 $2 + 1
        StrCpy $1 $1 '' $2
    
        StrCmp $4$5 '' equal
    
        StrCpy $6 -1
        IntOp $6 $6 + 1
        StrCpy $3 $4 1 $6
        StrCmp $3 '0' -2
        StrCmp $3 '' 0 +2
        StrCpy $4 0
    
        StrCpy $7 -1
        IntOp $7 $7 + 1
        StrCpy $3 $5 1 $7
        StrCmp $3 '0' -2
        StrCmp $3 '' 0 +2
        StrCpy $5 0
    
        StrCmp $4 0 0 +2
        StrCmp $5 0 begin newer2
        StrCmp $5 0 newer1
        IntCmp $6 $7 0 newer1 newer2
    
        StrCpy $4 '1$4'
        StrCpy $5 '1$5'
        IntCmp $4 $5 begin newer2 newer1
    
        equal:
        StrCpy $0 0
        goto end
        newer1:
        StrCpy $0 1
        goto end
        newer2:
        StrCpy $0 2
    
        end:
        Pop $7
        Pop $6
        Pop $5
        Pop $4
        Pop $3
        Pop $2
        Pop $1
        Exch $0
    FunctionEnd

########################################################################
# Configure MUI2 (Modern UI 2)
########################################################################

    !include "MUI2.nsh"
    !include LogicLib.nsh
    !include x64.nsh
    !include InstallOptions.nsh

    ; Custom GUI Init function
    !define MUI_CUSTOMFUNCTION_GUIINIT onGUIInit

    ; Page header
    !define MUI_ICON "resources\icon.ico"
    !define MUI_HEADERIMAGE
    !define MUI_HEADERIMAGE_RIGHT
    !define MUI_HEADERIMAGE_BITMAP "resources\150x57.bmp"
    !define MUI_HEADERIMAGE_BITMAP_STRETCH FitControl

    ; Welcome/finish page
    !define MUI_WELCOMEFINISHPAGE_BITMAP "resources\164x314.bmp"
    !define MUI_WELCOMEFINISHPAGE_BITMAP_STRETCH FitControl

    ; General page
    !define MUI_PAGE_HEADER_TEXT "Become a Streaming Legend"
    !define MUI_PAGE_HEADER_SUBTEXT "StreamElements: The Ultimate Streamer Platform"

    ; Welcome page
    !define MUI_WELCOMEPAGE_TITLE "Welcome to StreamElements!"
    ;!define MUI_WELCOMEPAGE_TITLE_3LINES
    !define MUI_WELCOMEPAGE_TEXT "Jump on Board! You're about to set up your OBS Studio with StreamElements Add-On"

    ; License page
    !define MUI_LICENSEPAGE_TEXT_TOP "Please read the license agreement below."
    !define MUI_LICENSEPAGE_TEXT_BOTTOM "You must agree to the license terms above to install ${PRODUCT_NAME}"
    !define MUI_LICENSEPAGE_BUTTON  "I Agree"

    ; Directory page
    !define MUI_DIRECTORYPAGE_TEXT_TOP "Setup will install ${PRODUCT_SHORT_NAME} in the following folder.$\r$\nTo install in a different folder, click Browse and select a folder.$\r$\n$\r$\nClick Next to continue."
    !define MUI_DIRECTORYPAGE_TEXT_DESTINATION "Destination Folder"

    ; Components page
    !define MUI_COMPONENTSPAGE_HEADER_TEXT "MUI_COMPONENTSPAGE_HEADER_TEXT"
    !define MUI_COMPONENTSPAGE_HEADER_SUBTEXT "MUI_COMPONENTSPAGE_HEADER_SUBTEXT"

    !define MUI_COMPONENTSPAGE_TEXT_TOP "Pick the features of ${PRODUCT_SHORT_NAME} you'd like to install."
    !define MUI_COMPONENTSPAGE_TEXT_COMPLIST "Select components to install:"

    ; Finish page
    !define MUI_FINISHPAGE_TITLE "Buckle up! We're ready to go!"
    !define MUI_FINISHPAGE_TEXT "${PRODUCT_NAME} has been installed on your computer.$\r$\n$\r$\nClick Finish to close setup."
    !define MUI_FINISHPAGE_TEXT_LARGE

    !define MUI_FINISHPAGE_RUN
    !define MUI_FINISHPAGE_RUN_FUNCTION ObsExecProgram
    !define MUI_FINISHPAGE_RUN_TEXT "Launch ${PRODUCT_SHORT_NAME} when I click ${MUI_FINISHPAGE_BUTTON}"
    
    !define MUI_FINISHPAGE_SHOWREADME
    !define MUI_FINISHPAGE_SHOWREADME_TEXT "Create Desktop Shortcut"
    !define MUI_FINISHPAGE_SHOWREADME_FUNCTION CreateDesktopShortCut
    ;!define MUI_FINISHPAGE_SHOWREADME_NOTCHECKED
    
    !define MUI_FINISHPAGE_LINK_LOCATION "${PRODUCT_WEB_SITE}"
    !define MUI_FINISHPAGE_LINK "Visit ${PRODUCT_PUBLISHER} Website"

    ; Abort warning
    !define MUI_ABORTWARNING
    !define MUI_ABORTWARNING_TEXT "You are about to abort ${PRODUCT_NAME} setup. Are you sure?"

    ; Pages sequence
    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowWelcome
    !insertmacro MUI_PAGE_WELCOME

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowLicense
    !insertmacro MUI_PAGE_LICENSE "resources\license.txt"

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowFolder
    !insertmacro MUI_PAGE_DIRECTORY

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowComponents
    !insertmacro MUI_PAGE_COMPONENTS

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowInstall
    !insertmacro MUI_PAGE_INSTFILES

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowFinish
    !insertmacro MUI_PAGE_FINISH

    ; Language, must be located after MUI_PAGE_XXX macros above
    !insertmacro MUI_LANGUAGE "English"

    Var /GLOBAL g_Locked_Files_Checked
    Var /GLOBAL g_TLS12_Enabled

Function onGUIInit
    ;-- Aero::Apply /NOBRANDING ""
FunctionEnd

;Function NullFunction
;FunctionEnd

Function LockedListShow
    StrCmp $g_Locked_Files_Checked "true" already_checked

    DetailPrint "Checking for locked files..."
    SetPluginUnload alwaysoff
    NSISLockDetector::AddWildcardPattern "$INSTDIR\*.exe"
    NSISLockDetector::AddWildcardPattern "$INSTDIR\*.dll"
    NSISLockDetector::SetMode "restartmanager"
    NSISLockDetector::Dialog
    ;LockedList::AddFolder $INSTDIR\bin
    ;LockedList::AddFolder $INSTDIR\obs-plugins
    ;LockedList::Dialog /autonext /autoclose "" "" "" "Close All"
    Pop $R0
    StrCmp "$R0" "OK" programs_ok programs_error

programs_error:
    Push "User refused to close running OBS components"
    Call AbortByLocalError
    
programs_ok:
    StrCpy $g_Locked_Files_Checked "true"
already_checked:
FunctionEnd

Function EnableTLS12
    StrCmp $g_TLS12_Enabled "true" already_enabled

    ExecWait '"msiexec" /i "$PLUGINSDIR\MicrosoftEasyFix51044.msi" /passive /quiet' $0

    StrCpy $g_TLS12_Enabled "true"

already_enabled:
FunctionEnd

Section "OBS Studio" section_download_obs_studio
    # DetailPrint to appear both in details view and status bar
    SetDetailsPrint both

    SetOutPath $INSTDIR

    Call LockedListShow
    Call DownloadAndInstallOBS
SectionEnd

!macro _InstallSystemFile _src _dest
    SetOverwrite try

    ClearErrors
    DetailPrint "Trying to install: $OUTDIR\${_dest}"
    File "/oname=${_dest}" "${_src}"

    ${If} ${Errors}
        DetailPrint "Falling back to install with MoveFileEx: $OUTDIR\${_dest}"
        Push $0
        Push $1

        StrCpy $0 "$OUTDIR"
        GetTempFileName $1
        Delete "$1"

        CreateDirectory "$1"
        SetOutPath "$1"
        DetailPrint "Extracting: $1\${_dest}"
        File "/oname=${_dest}" "${_src}"
        SetOutPath "$0"

        Push $R0
        Push $R1
        ; Call MoveFileEx on the file above (Params: <source>, <destination>, 4) 5 == Move on Reboot && Replace Existing
	    System::Call "kernel32::MoveFileEx(t '$1\${_dest}', t '$OUTDIR\${_dest}', i 5) b.r0 ?e"
        Pop $R1 ; the ?e flag from System::Call pushes the result of GetLastError() onto the stack.
        ${If} "$R0" = "0"
            ; MoveFileEx failed: this can happen for all sorts of reasons
            DetailPrint "System::Call kernel32::MoveFileEx failed ($R1) for $OUTDIR\${_dest}"

            ; Don't fail
            ; Push "System::Call kernel32::MoveFileEx failed for $OUTDIR\${_dest}"
            ; Call AbortByLocalError

            ; Remove temporary folder since we won't need it
            RMDir /r /REBOOTOK "$1"
        ${Else}
            ; Indicate reboot
            SetRebootFlag true

            DetailPrint "$OUTDIR\${_dest} will be replaced on reboot"
        ${EndIf}
        Pop $R1
        Pop $R0

        ; Don't remove temp folder: we'll need it to move the file after reboot
        ; RMDir /r /REBOOTOK "$1"

        Pop $1
        Pop $0
    ${Else}
        DetailPrint "$OUTDIR\${_dest} was installed"
    ${EndIf}
    SetOverwrite on
!macroend
!define InstallSystemFile '!insertmacro _InstallSystemFile'

!macro _InstallNewerFile _src _dest _version
    Push $8
    StrCpy $8 "0"

    ${If} ${FileExists} "$OUTDIR\${_dest}"
        ;DetailPrint "File exists: $OUTDIR\${_dest}"
        Push $R2

        ${GetFileVersion} "$OUTDIR\${_dest}" $R2
        DetailPrint "GetFileVersion: ${_dest} $R2"

        ${VersionCompare} "${_version}" "$R2" $R2

        ${If} "$R2" = "1"
            StrCpy $8 "1"
        ${Else}
            DetailPrint "Skip overwriting newer file: ${_dest}"
        ${EndIF}
    ${Else}
        ;DetailPrint "NO FILE: $OUTDIR\${_dest}"
        StrCpy $8 "1"
    ${EndIF}

    ${If} "$8" = "1"
        ${InstallSystemFile} "${_src}" "${_dest}"
    ${EndIf}
    Pop $8
!macroend
!define InstallNewerFile '!insertmacro _InstallNewerFile'

Function InstallMSRedist64
    ${If} ${RunningX64}
        Push $9
        StrCpy $9 "$OUTDIR"

        SetOutPath "$WINDIR\System32"

        ${DisableX64FSRedirection}
        ${InstallNewerFile} "resources\vcruntime\x64\ucrtbase.dll" "ucrtbase.dll" "10.0.10586.15"
        ${InstallNewerFile} "resources\vcruntime\x64\concrt140.dll" "concrt140.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\msvcp140.dll" "msvcp140.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\msvcp140_1.dll" "msvcp140_1.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\msvcp140_2.dll" "msvcp140_2.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\msvcp140_atomic_wait.dll" "msvcp140_atomic_wait.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\msvcp140_codecvt_ids.dll" "msvcp140_codecvt_ids.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\vccorlib140.dll" "vccorlib140.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\vcruntime140.dll" "vcruntime140.dll" "14.28.29910.0"
        ${InstallNewerFile} "resources\vcruntime\x64\vcruntime140_1.dll" "vcruntime140_1.dll" "14.28.29910.0"
        ${EnableX64FSRedirection}

        SetOutPath "$9"
        Pop $9
    ${EndIf}
FunctionEnd

Function InstallMSRedist32
    Push $9
    StrCpy $9 "$OUTDIR"

    ${If} ${RunningX64}
        SetOutPath "$WINDIR\SysWOW64"

        ${DisableX64FSRedirection}
    ${Else}
        SetOutPath "$WINDIR\System32"
    ${EndIf}

    ${InstallNewerFile} "resources\vcruntime\x86\ucrtbase.dll" "ucrtbase.dll" "10.0.10586.15"
    ${InstallNewerFile} "resources\vcruntime\x86\concrt140.dll" "concrt140.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\msvcp140.dll" "msvcp140.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\msvcp140_1.dll" "msvcp140_1.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\msvcp140_2.dll" "msvcp140_2.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\msvcp140_atomic_wait.dll" "msvcp140_atomic_wait.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\msvcp140_codecvt_ids.dll" "msvcp140_codecvt_ids.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\vccorlib140.dll" "vccorlib140.dll" "14.28.29910.0"
    ${InstallNewerFile} "resources\vcruntime\x86\vcruntime140.dll" "vcruntime140.dll" "14.28.29910.0"

    ${If} ${RunningX64}
        ${EnableX64FSRedirection}
    ${EndIf}

    SetOutPath "$9"
    Pop $9
FunctionEnd

Section "${PRODUCT_SHORT_NAME} Add-On" section_install_streamelements
    # DetailPrint to appear both in details view and status bar
    SetDetailsPrint both

    Call InstallMSRedist64
    Call InstallMSRedist32

    SetOutPath "$INSTDIR"

    Call LockedListShow

    #########################################################################
    # Environment
    #########################################################################

    # Set persistent Qt environment variables to override Qt 5.15.2 bug with:
    # QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough)
    #
    # We use setx.exe since it notifies the shell of environment change.
    #
    ExecShellWait "" "$SYSDIR\setx.exe" 'QT_SCALE_FACTOR_ROUNDING_POLICY RoundPreferFloor' SW_HIDE
    ExecShellWait "" "$SYSDIR\setx.exe" 'QT_SCALE_FACTOR_ROUNDING_POLICY RoundPreferFloor /M' SW_HIDE

    !define env_hklm 'HKLM "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"'
    !define env_hkcu 'HKCU "Environment"'

    # Backup - set environment through registry
    WriteRegExpandStr ${env_hklm} QT_SCALE_FACTOR_ROUNDING_POLICY RoundPreferFloor
    WriteRegExpandStr ${env_hkcu} QT_SCALE_FACTOR_ROUNDING_POLICY RoundPreferFloor

    # Set temporary Qt environment variables to override Qt 5.15.2 bug with:
    # QGuiApplication::setHighDpiScaleFactorRoundingPolicy(Qt::HighDpiScaleFactorRoundingPolicy::PassThrough)
    #
    # We need this for "Launch when I click finish" to work properly
    System::Call 'Kernel32::SetEnvironmentVariable(t, t)i ("QT_SCALE_FACTOR_ROUNDING_POLICY", "RoundPreferFloor").r0'

    #########################################################################
    # Icon & uninstaller
    #########################################################################

    File /oname=streamelements.ico resources\icon.ico

    File /oname=obs-streamelements-uninstaller.exe ..\obs-streamelements-uninstaller\obs-streamelements-uninstaller.exe

    #########################################################################
    # Data
    #########################################################################

    !ifndef SKIP_64BIT_CONTENT
        SetOutPath $INSTDIR\data\obs-plugins\obs-streamelements-core

        File /r ..\download\obs-streamelements-core\build64_qt6\data\obs-plugins\obs-streamelements-core\*.*
    !else
        !ifndef SKIP_32BIT_CONTENT
            SetOutPath $INSTDIR\data\obs-plugins\obs-streamelements-core

            File /r ..\download\obs-streamelements-core\build32_qt6\data\obs-plugins\obs-streamelements-core\*.*
        !endif
    !endif

    #########################################################################
    # 64 bit
    #########################################################################
!ifndef SKIP_64BIT_CONTENT
    SetOutPath $INSTDIR\bin\64bit

    File ..\download\obs-streamelements-core\build64_qt6\bin\64bit\BsSndRpt64.exe
    File ..\download\obs-streamelements-core\build64_qt6\bin\64bit\BugSplat64.dll
    File ..\download\obs-streamelements-core\build64_qt6\bin\64bit\BugSplatHD64.exe
    File ..\download\obs-streamelements-core\build64_qt6\bin\64bit\BugSplatRc64.dll

    SetOutPath $INSTDIR\obs-plugins\64bit\locales

    File /nonfatal ..\download\obs-streamelements-core\build64_qt6\obs-plugins\64bit\locales\*.*

    SetOutPath $INSTDIR\obs-plugins\64bit

    File /oname=obs-streamelements-core.dll ..\download\obs-streamelements-core\build64_qt6\obs-plugins\64bit\obs-streamelements-core.dll
    File /oname=obs-streamelements-core.pdb ..\download\obs-streamelements-core\build64_qt6\obs-plugins\64bit\obs-streamelements-core.pdb
    
    File /nonfatal ..\download\obs-streamelements-core\build64_qt6\obs-plugins\64bit\obs-streamelements-set-machine-config.*

    Delete /REBOOTOK "$OUTDIR\obs-streamelements.dll"
    Delete /REBOOTOK "$OUTDIR\obs-streamelements.pdb"

    Delete "$OUTDIR\obs-streamelements.qt5.dymod"
    Delete "$OUTDIR\obs-streamelements.qt5.pdb"
    Delete "$OUTDIR\obs-streamelements-core.qt5.dymod"
    Delete "$OUTDIR\obs-streamelements-core.qt5.pdb"
!endif

    #########################################################################
    # 32 bit
    #########################################################################

    ${If} ${RunningX64}
    ; Check if we can skip 32-bit installation on 64-bit system

        IfFileExists "$INSTDIR\bin\32bit\obs32.exe" setup_obs_32 skip_obs_32
    ${Else}
        goto setup_obs_32
    ${EndIf}

    setup_obs_32:

!ifndef SKIP_32BIT_CONTENT
    SetOutPath $INSTDIR\bin\32bit

    File ..\download\obs-streamelements-core\build32_qt6\bin\32bit\BsSndRpt.exe
    File ..\download\obs-streamelements-core\build32_qt6\bin\32bit\BugSplat.dll
    File ..\download\obs-streamelements-core\build32_qt6\bin\32bit\BugSplatHD.exe
    File ..\download\obs-streamelements-core\build32_qt6\bin\32bit\BugSplatRc.dll

    SetOutPath $INSTDIR\obs-plugins\32bit\locales

    File /nonfatal ..\download\obs-streamelements-core\build32_qt6\obs-plugins\32bit\locales\*.*

    SetOutPath $INSTDIR\obs-plugins\32bit

    File /oname=obs-streamelements-core.dll ..\download\obs-streamelements-core\build32_qt6\obs-plugins\32bit\obs-streamelements-core.dll
    File /oname=obs-streamelements-core.pdb ..\download\obs-streamelements-core\build32_qt6\obs-plugins\32bit\obs-streamelements-core.pdb
    
    File /nonfatal ..\download\obs-streamelements-core\build32_qt6\obs-plugins\32bit\obs-streamelements-set-machine-config.*

    Delete /REBOOTOK "$OUTDIR\obs-streamelements.dll"
    Delete /REBOOTOK "$OUTDIR\obs-streamelements.pdb"

    Delete "$OUTDIR\obs-streamelements.qt5.dymod"
    Delete "$OUTDIR\obs-streamelements.qt5.pdb"
    Delete "$OUTDIR\obs-streamelements-core.qt5.dymod"
    Delete "$OUTDIR\obs-streamelements-core.qt5.pdb"
!endif

skip_obs_32:

    ; --------------------------------------------------------------------------------------
    ; -- Remove OBS safe mode marker
    ; --------------------------------------------------------------------------------------

    SetShellVarContext current
    Delete "$APPDATA\obs-studio\safe_mode"

    SetShellVarContext all
    Delete "$APPDATA\obs-studio\safe_mode"

    ; --------------------------------------------------------------------------------------
    ; -- Uninstall info
    ; --------------------------------------------------------------------------------------

    ; http://nsis.sourceforge.net/Add_uninstall_information_to_Add/Remove_Programs

    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "DisplayName" "${PRODUCT_NAME}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "UninstallString" "$\"$INSTDIR\obs-streamelements-uninstaller.exe$\""
    
    ; Optional
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "QuietUninstallString" "$\"$INSTDIR\obs-streamelements-uninstaller.exe$\" /S /D=$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "InstallLocation" "$INSTDIR"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "DisplayIcon" "$INSTDIR\streamelements.ico"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "Publisher" "${PRODUCT_PUBLISHER}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "HelpLink" "${PRODUCT_SUPPORT_URL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "URLUpdateInfo" "${PRODUCT_NEWS_URL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "URLInfoAbout" "${PRODUCT_ABOUT_URL}"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "DisplayVersion" "${PRODUCT_VERSION}"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "NoModify" "1"
    WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "NoRepair" "1"
    WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}" \
                "Comments" "${PRODUCT_NAME} v${PRODUCT_VERSION} (Setup v${SETUP_VERSION})"
SectionEnd

; Modern install component descriptions
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
	!insertmacro MUI_DESCRIPTION_TEXT ${section_download_obs_studio} "Download and install OBS Studio ${OBS_DOWNLOAD_VERSION}"
	!insertmacro MUI_DESCRIPTION_TEXT ${section_install_streamelements} "Install ${PRODUCT_NAME} Add-On on top of OBS Studio"
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Function .onGUIEnd
    Call onInstallationEnd
FunctionEnd

Function .onInstSuccess 
    Call FinishInstallation
FunctionEnd

Function .onInit
    Call scienceInit
    Call PreventMultipleInstances

    SectionSetSize ${section_download_obs_studio} 150000
    SectionSetSize ${section_install_streamelements} 100000

    SectionSetFlags ${section_install_streamelements} 17 ; 17 = 1 (selected) + 16 (readonly)

    InitPluginsDir

    Call QueryInstallDir
    Call VerifyObsStudioInstalled
FunctionEnd

Function .onVerifyInstDir
    Call VerifyObsStudioInstalled
FunctionEnd

Function QueryInstallDir
    StrCmp "$INSTDIR" "" query done

query:
    # Query 64bit location
    ReadRegStr $INSTDIR HKLM "SOFTWARE\WOW6432Node\OBS Studio" ""
    IfErrors query32bit
    goto done

query32bit:
    # Try 32bit location
    ReadRegStr $INSTDIR HKLM "SOFTWARE\OBS Studio" ""
    IfErrors setdefault
    goto done

setdefault:
    # Set default installation folder
    ;InstallDir "$PROGRAMFILES32\obs-studio3"
    StrCpy $INSTDIR "$PROGRAMFILES32\obs-studio"
done:
FunctionEnd

Function VerifyObsStudioInstalled
    # Default: OBS is installed
    SectionSetFlags ${section_download_obs_studio} 0 ; not selected

    # Check OBS version
    Call CheckObsStudioVersion

    ; No OBS Studio
    StrCmp "$g_obsVersion" "" no_obs

    ; Old OBS Studio
    ${VersionCompare} "${OBS_REQUIRE_VERSION}" "$g_obsVersion" $R0
    StrCmp "$R0" "1" old_obs

    goto has_obs

    old_obs:
        DetailPrint "Detected OBS Studio version $g_obsVersion is too old."
        
        ; Require OBS Studio download and installation
        SectionSetFlags ${section_download_obs_studio} 17 ; 17 = 1 (selected) + 16 (readonly)
        goto done

    no_obs:
        DetailPrint "OBS Studio was not detected on this computer."

        ; Require OBS Studio download and installation
        SectionSetFlags ${section_download_obs_studio} 17 ; 17 = 1 (selected) + 16 (readonly)

    has_obs:

    done:
FunctionEnd

Function GetFileSize
  Exch $0
  Push $1
  FileOpen $1 $0 "r"
  FileSeek $1 0 END $0
  FileClose $1
  Pop $1
  Exch $0
FunctionEnd

Function CheckObsStudioVersion
    StrCpy $g_obsVersion ""

    # Check if OBS already exists
    IfFileExists "$INSTDIR\bin\64bit\obs64.exe" check_obs_64
    IfFileExists "$INSTDIR\bin\32bit\obs32.exe" check_obs_32

    goto no_obs

    check_obs_64:
        ${If} ${RunningX64}
            File /oname=$PLUGINSDIR\obs-probe-libobs-version-64.exe "resources\obs-probe-libobs-version-64.exe"
            SetOutPath "$INSTDIR\bin\64bit"
            nsExec::ExecToStack '"$PLUGINSDIR\obs-probe-libobs-version-64.exe"'
            Pop $0 ; Return
            Pop $g_obsVersion ; Output
            StrCmp "$g_obsVersion" "" check_obs_32
            StrCmp "$0" "0" has_obs_64 no_obs
        ${EndIf}

    check_obs_32:
        File /oname=$PLUGINSDIR\obs-probe-libobs-version-32.exe "resources\obs-probe-libobs-version-32.exe"
        SetOutPath "$INSTDIR\bin\32bit"
        nsExec::ExecToStack '"$PLUGINSDIR\obs-probe-libobs-version-32.exe"'
        Pop $0 ; Return
        Pop $g_obsVersion ; Output
        StrCmp "$g_obsVersion" "" no_obs
        StrCmp "$0" "0" has_obs_32 no_obs

    has_obs_64:
        goto verify_files

    has_obs_32:
        goto verify_files

    verify_files:
        goto has_obs

    no_obs:
        StrCpy $g_obsVersion ""
        goto done

    has_obs:
        goto done

    done:
        ${Trim} $g_obsVersion $g_obsVersion
FunctionEnd

;----------------------------------------------------------------------------
; Title             : Go to a NSIS page
; Short Name        : RelGotoPage
; Last Changed      : 22/Feb/2005
; Code Type         : Function
; Code Sub-Type     : Special Restricted Call, One-way StrCpy Input
;----------------------------------------------------------------------------
; Description       : Makes NSIS to go to a specified page relatively from
;                     the current page. See this below for more information:
;                     "http://nsis.sf.net/wiki/Go to a NSIS page"
;----------------------------------------------------------------------------
; Function Call     : StrCpy $R9 "(number|X)"
;
;                     - If a number &gt; 0: Goes foward that number of
;                       pages. Code of that page will be executed, not
;                       returning to this point. If it excess the number of
;                       pages that are after that page, it simulates a
;                       "Cancel" click.
;
;                     - If a number &lt; 0: Goes back that number of pages.
;                       Code of that page will be executed, not returning to
;                       this point. If it excess the number of pages that
;                       are before that page, it simulates a "Cancel" click.
;
;                     - If X: Simulates a "Cancel" click. Code will go to
;                       callback functions, not returning to this point.
;
;                     - If 0: Continues on the same page. Code will still
;                        be running after the call.
;
;                     Call RelGotoPage
;----------------------------------------------------------------------------
; Author            : Diego Pedroso
; Author Reg. Name  : deguix
;----------------------------------------------------------------------------
;
; Source: http://nsis.sourceforge.net/Go_to_a_NSIS_page
;
Function RelGotoPage
  IntCmp $R9 0 0 Move Move
    StrCmp $R9 "X" 0 Move
      StrCpy $R9 "120"
 
  Move:
  SendMessage $HWNDPARENT "0x408" "$R9" ""
FunctionEnd

Function DownloadAndInstallOBS
    Call onBeginDownloadOBS

    ; If this is empty, 32-bit OBS Studio won't be installed
    StrCpy $g_obsTempPath32 ""

    ${If} ${RunningX64}
        ; Check if we can skip 32-bit installation on 64-bit system

!ifndef SKIP_64BIT_CONTENT
        IfFileExists "$INSTDIR\bin\32bit\obs32.exe" get_obs_32 skip_obs_32
!else
        goto get_obs_32
!endif
    ${Else}
        goto get_obs_32
    ${EndIf}

get_obs_32:
    ; 32-bit platform *or* 32-bit obs32.exe exists

    GetTempFileName $g_obsTempPath32
    StrCpy $g_obsTempPath32 "$g_obsTempPath32.exe"

    StrCpy $g_downloadTargetPath $g_obsTempPath32
    StrCpy $g_downloadText "OBS Studio (Win32) ${OBS_DOWNLOAD_VERSION}"
    StrCpy $g_downloadUrl "${OBS_DOWNLOAD_URL_32}"
    Call DownloadRequiredFile

skip_obs_32:

    ${If} ${RunningX64}
        ; Download 64-bit OBS only on 64-bit platforms

        GetTempFileName $g_obsTempPath64
        StrCpy $g_obsTempPath64 "$g_obsTempPath64.exe"

        StrCpy $g_downloadTargetPath $g_obsTempPath64
        StrCpy $g_downloadText "OBS Studio (Win64) ${OBS_DOWNLOAD_VERSION}"
        StrCpy $g_downloadUrl "${OBS_DOWNLOAD_URL_64}"
        Call DownloadRequiredFile
    ${EndIf}

    goto done_download

done_download:
    ${If} ${RunningX64}
        StrCmp "$g_obsTempPath32" "" skip_install_obs_32 install_obs_32
    ${Else}
        goto install_obs_32
    ${EndIf}

install_obs_32:

    ; Install OBS 32-bit
    DetailPrint "Exec OBS installer (Win32)"
    ExecWait '"$g_obsTempPath32" /S /D=$INSTDIR' $0
    IfErrors install_error
    Delete "$g_obsTempPath32.exe"

    DetailPrint "Exit code $0"

skip_install_obs_32:

    ${If} ${RunningX64}
        ; Install OBS 64-bit
        DetailPrint "Exec OBS installer (Win64)"
        ExecWait '"$g_obsTempPath64" /S /D=$INSTDIR' $0
        IfErrors install_error
        Delete "$g_obsTempPath64.exe"

        DetailPrint "Exit code $0"
    ${EndIf}

    goto done

install_error:
    MessageBox MB_ICONSTOP "Failed installing OBS Studio"
    Delete "$0.exe"
    Push "Failed installing OBS Studio"
    Call AbortByLocalError

    goto done

done:
FunctionEnd

Function DownloadRequiredFile
    Push $1

    Call EnableTLS12
    Call ConnectInternet

    Inetc::get /WEAKSECURITY /QUESTION "${MSG_DOWNLOAD_CANCEL_CONFIRM}" /CAPTION "Downloading $g_downloadText..." /RESUME "${MSG_DOWNLOAD_ERROR_RETRY}" "$g_downloadUrl" "$g_downloadTargetPath" /END

    Pop $1 ; return value = exit code, "OK" means OK
    StrCmp "$1" "OK" download_ok
    StrCmp "$1" "Cancelled" download_cancel
    goto download_error

download_ok:
    goto done

download_error:
    DetailPrint "Warning: failed downloading $g_downloadText from $g_downloadUrl"
    Push "Failed downloading $g_downloadText from $g_downloadUrl"
    Call AbortByNetworkError
    goto done

download_cancel:
    Push "User cancelled downloading $g_downloadText from $g_downloadUrl"
    Call AbortByUserRequest
    goto done

done:
FunctionEnd

Function ObsExecProgram
    StrCpy $g_instObsExecProgram "true"

    ${If} ${RunningX64}
        ; Set working dir
        SetOutPath "$INSTDIR\bin\64bit"

        ; Start OBS
        Exec "$INSTDIR\bin\64bit\obs64.exe"
    ${Else}
        ; Set working dir
        SetOutPath "$INSTDIR\bin\32bit"

        ; Start OBS
        Exec "$INSTDIR\bin\32bit\obs32.exe"
    ${EndIf}
FunctionEnd

Function CreateDesktopShortCut
    StrCpy $g_instCreateDesktopShortCut "true"

    ${If} ${RunningX64}
        CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\bin\64bit\obs64.exe" "" "$INSTDIR\streamelements.ico"
    ${Else}
        CreateShortCut "$DESKTOP\${PRODUCT_NAME}.lnk" "$INSTDIR\bin\32bit\obs32.exe" "" "$INSTDIR\streamelements.ico"
    ${EndIf}
FunctionEnd

Function ConnectInternet
 
   Push $R0
     
     ClearErrors
     Dialer::AttemptConnect
     IfErrors noie3
     
     Pop $R0
     StrCmp $R0 "online" connected
       MessageBox MB_OK|MB_ICONSTOP "Cannot connect to the internet."
       Quit ;This will quit the installer. You might want to add your own error handling.
     
     noie3:
   
     ; IE3 not installed
     MessageBox MB_OK|MB_ICONINFORMATION "Please connect to the internet now."
     
     connected:
   
   Pop $R0
   
FunctionEnd
 
Function StrStr
/*After this point:
  ------------------------------------------
  $R0 = SubString (input)
  $R1 = String (input)
  $R2 = SubStringLen (temp)
  $R3 = StrLen (temp)
  $R4 = StartCharPos (temp)
  $R5 = TempStr (temp)*/
 
  ;Get input from user
  Exch $R0
  Exch
  Exch $R1
  Push $R2
  Push $R3
  Push $R4
  Push $R5
 
  ;Get "String" and "SubString" length
  StrLen $R2 $R0
  StrLen $R3 $R1
  ;Start "StartCharPos" counter
  StrCpy $R4 0
 
  ;Loop until "SubString" is found or "String" reaches its end
  ${Do}
    ;Remove everything before and after the searched part ("TempStr")
    StrCpy $R5 $R1 $R2 $R4
 
    ;Compare "TempStr" with "SubString"
    ${IfThen} $R5 == $R0 ${|} ${ExitDo} ${|}
    ;If not "SubString", this could be "String"'s end
    ${IfThen} $R4 >= $R3 ${|} ${ExitDo} ${|}
    ;If not, continue the loop
    IntOp $R4 $R4 + 1
  ${Loop}
 
/*After this point:
  ------------------------------------------
  $R0 = ResultVar (output)*/
 
  ;Remove part before "SubString" on "String" (if there has one)
  StrCpy $R0 $R1 `` $R4
 
  ;Return output to user
  Pop $R5
  Pop $R4
  Pop $R3
  Pop $R2
  Pop $R1
  Exch $R0
FunctionEnd

; GetWindowsVersion 4.1.1 (2015-06-22)
;
; Based on Yazno's function, http://yazno.tripod.com/powerpimpit/
; Update by Joost Verburg
; Update (Macro, Define, Windows 7 detection) - John T. Haller of PortableApps.com - 2008-01-07
; Update (Windows 8 detection) - Marek Mizanin (Zanir) - 2013-02-07
; Update (Windows 8.1 detection) - John T. Haller of PortableApps.com - 2014-04-04
; Update (Windows 10 TP detection) - John T. Haller of PortableApps.com - 2014-10-01
; Update (Windows 10 TP4 detection, and added include guards) - Kairu - 2015-06-22
;
; Usage: ${GetWindowsVersion} $R0
;
; $R0 contains: 95, 98, ME, NT x.x, 2000, XP, 2003, Vista, 7, 8, 8.1, 10.0 or '' (for unknown)
 
Function GetWindowsVersion
 
  Push $R0
  Push $R1
 
  ; check if Windows 10 family (CurrentMajorVersionNumber is new introduced in Windows 10)
  ReadRegStr $R0 HKLM \
    "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentMajorVersionNumber
 
  StrCmp $R0 '' 0 lbl_winnt
 
  ClearErrors
 
  ; check if Windows NT family
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows NT\CurrentVersion" CurrentVersion
 
  IfErrors 0 lbl_winnt
 
  ; we are not NT
  ReadRegStr $R0 HKLM \
  "SOFTWARE\Microsoft\Windows\CurrentVersion" VersionNumber
 
  StrCpy $R1 $R0 1
  StrCmp $R1 '4' 0 lbl_error
 
  StrCpy $R1 $R0 3
 
  StrCmp $R1 '4.0' lbl_win32_95
  StrCmp $R1 '4.9' lbl_win32_ME lbl_win32_98
 
  lbl_win32_95:
    StrCpy $R0 '95'
  Goto lbl_done
 
  lbl_win32_98:
    StrCpy $R0 '98'
  Goto lbl_done
 
  lbl_win32_ME:
    StrCpy $R0 'ME'
  Goto lbl_done
 
  lbl_winnt:
 
  StrCpy $R1 $R0 1
 
  StrCmp $R1 '3' lbl_winnt_x
  StrCmp $R1 '4' lbl_winnt_x
 
  StrCpy $R1 $R0 3
 
  StrCmp $R1 '5.0' lbl_winnt_2000
  StrCmp $R1 '5.1' lbl_winnt_XP
  StrCmp $R1 '5.2' lbl_winnt_2003
  StrCmp $R1 '6.0' lbl_winnt_vista
  StrCmp $R1 '6.1' lbl_winnt_7
  StrCmp $R1 '6.2' lbl_winnt_8
  StrCmp $R1 '6.3' lbl_winnt_81
  StrCmp $R1 '10' lbl_winnt_10
 
  StrCpy $R1 $R0 4
 
  StrCmp $R1 '10.0' lbl_winnt_10
  Goto lbl_error
 
  lbl_winnt_x:
    StrCpy $R0 "NT $R0" 6
  Goto lbl_done
 
  lbl_winnt_2000:
    Strcpy $R0 '2000'
  Goto lbl_done
 
  lbl_winnt_XP:
    Strcpy $R0 'XP'
  Goto lbl_done
 
  lbl_winnt_2003:
    Strcpy $R0 '2003'
  Goto lbl_done
 
  lbl_winnt_vista:
    Strcpy $R0 'Vista'
  Goto lbl_done
 
  lbl_winnt_7:
    Strcpy $R0 '7'
  Goto lbl_done
 
  lbl_winnt_8:
    Strcpy $R0 '8'
  Goto lbl_done
 
  lbl_winnt_81:
    Strcpy $R0 '8.1'
  Goto lbl_done
 
  lbl_winnt_10:
    Strcpy $R0 '10'
  Goto lbl_done
 
  lbl_error:
    Strcpy $R0 ''
  lbl_done:
 
  Pop $R1
  Exch $R0
 
FunctionEnd
 
!macro GetWindowsVersion OUTPUT_VALUE
	Call GetWindowsVersion
	Pop `${OUTPUT_VALUE}`
!macroend
 
!define GetWindowsVersion '!insertmacro "GetWindowsVersion"'

########################################################################
# Custom functions
########################################################################

Function SendTrackingEvent
    StrCpy $g_scienceFields ""
    StrCpy $g_scienceProps ""

    Pop $R9 ; Additional fields
    Pop $R8 ; Event Name

    Push $R0
    Push $R1

    Call scienceGetDuration
    Pop $R1 ; -- Seconds since previous
    Pop $R0 ; -- Seconds since start
    StrCpy $g_scienceFields '["plugin_version", "${PRODUCT_VERSION}"], ["setup_version", "${SETUP_VERSION}"], ["seconds_since_start", "$R0"], ["seconds_since_previous_event", "$R1"]'

    ; --- Get windows version ---
    ${GetWindowsVersion} $R0

    StrCpy $g_scienceProps '"feature": "obs_core_plugin_installer", "name": "$R8", "sessionId": "$g_scienceSessionId", "_meta": {"mobile": false, "os": "Windows", "platform": "desktop", "osversion": "$R0"}'

    Pop $R1
    Pop $R0

    ; Report hostMachineId
    StrCpy $g_scienceProps '$g_scienceProps, "hostMachineId": "$g_scienceHostMachineUniqueId"'

    ; --- Check if there is an abort source ---
    StrCmp $g_instAbortSource "" no_abort_source
    StrCpy $g_scienceProps '$g_scienceProps, "source": "$g_instAbortSource"'
no_abort_source:

    ; --- Check if there is an abort reason ---
    StrCmp $g_instAbortReason "" no_abort_reason
    StrCpy $g_scienceProps '$g_scienceProps, "message": "$g_instAbortReason"'
no_abort_reason:

    ; --- Previous event name ---
    StrCpy $g_scienceFields '$g_scienceFields, ["prev_event_name", "$g_instPrevEvent"]'

    ; --- Add current screen index and name ---
    StrCpy $g_scienceFields '$g_scienceFields, ["curr_step_index", "$g_instCurrentScreenIndex"]'
    StrCpy $g_scienceFields '$g_scienceFields, ["curr_step_name", "$g_instCurrentScreenName"]'

    ; --- Add auto-start mode ---
    StrCpy $g_scienceFields '$g_scienceFields, ["auto_start", "$g_instAutostart"]'

    ; --- Check components ---
    IntCmp $g_instCurrentScreenIndex 50 has_components no_components has_components
has_components:
    StrCpy $g_scienceFields '$g_scienceFields, ["installation_started", "true"]'

    Push $0
    Push $1

        SectionGetFlags ${section_download_obs_studio} $0
        IntOp $1 $0 & ${SF_RO}
        IntOp $0 $0 & ${SF_SELECTED}

        IntCmp $0 ${SF_SELECTED} has_download_obs no_download_obs
            has_download_obs:
                StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested", "true"]'

                IntCmp $1 ${SF_RO} force_download_obs voluntary_download_obs
                    force_download_obs:
                        StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested_by", "forced_by_setup"]'
                        goto after_download_obs
                    voluntary_download_obs:
                        StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested_by", "user"]'
                        goto after_download_obs

           no_download_obs:
                StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested", "false"]'
                StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested_by", ""]'

            after_download_obs:

        Pop $1
        Pop $0

    goto after_components
no_components:
    StrCpy $g_scienceFields '$g_scienceFields, ["installation_started", "false"]'
    StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested", ""]'
    StrCpy $g_scienceFields '$g_scienceFields, ["obs_download_requested_by", ""]'
    goto after_components
after_components:

    ; --- Check for additional fields
    StrCmp $R9 "" no_additional_fields
    StrCpy $g_scienceFields "$g_scienceFields, $R9"
    no_additional_fields:

    ; --- Add fields to the payload
    StrCpy $g_scienceProps '$g_scienceProps, "fields": [ $g_scienceFields ]'

    StrCpy $g_scienceProps '{ $g_scienceProps }'

    ; --- Save previous event name ---
    StrCpy $g_instPrevEvent "$R8"

    SetPluginUnload alwaysoff
    NSISHTTP::HttpPostStringWait "https://api.streamelements.com/science/insert/obslive" "application/json" "$g_scienceProps"
FunctionEnd

Function AbortByUserRequest
    StrCpy $g_instCompleted "true"

    Pop $g_instAbortReason
    StrCpy $g_instAbortSource "user"

    DetailPrint "$g_instAbortReason"

    Push 'se_live_installer_abort'
    Push ''
    Call SendTrackingEvent
    Call scienceFlush

    Abort
FunctionEnd

Function AbortByNetworkError
    StrCpy $g_instCompleted "true"

    Pop $g_instAbortReason
    StrCpy $g_instAbortSource "network"

    DetailPrint "$g_instAbortReason"

    Push 'se_live_installer_abort'
    Push ''
    Call SendTrackingEvent
    Call scienceFlush

    Abort
FunctionEnd

Function AbortByLocalError
    StrCpy $g_instCompleted "true"

    Pop $g_instAbortReason
    StrCpy $g_instAbortSource "local_machine"

    DetailPrint "$g_instAbortReason"

    Push 'se_live_installer_abort'
    Push ''
    Call SendTrackingEvent
    Call scienceFlush

    Abort
FunctionEnd

Function onInstallationEnd
    StrCmp $g_instCompleted "true" skip

    Push "User cancelled installation"
    Call AbortByUserRequest

skip:
FunctionEnd

Function FinishInstallation
    StrCpy $g_instCompleted "true"

    Push $R9

    StrCmp $g_instCreateDesktopShortCut "true" desktop_shortcut no_desktop_shortcut
    desktop_shortcut:
        StrCpy $R9 '[ "desktop_shortcut_requested", "true" ]'
        goto after_desktop_shortcut
    no_desktop_shortcut:
        StrCpy $R9 '[ "desktop_shortcut_requested", "false" ]'
        goto after_desktop_shortcut
    after_desktop_shortcut:


    StrCmp $g_instObsExecProgram "true" obs_exec no_obs_exec
    obs_exec:
        StrCpy $R9 '$R9, [ "start_obs_requested", "true" ]'
        goto after_obs_exec
    no_obs_exec:
        StrCpy $R9 '$R9, [ "start_obs_requested", "false" ]'
        goto after_obs_exec
    after_obs_exec:

    Push 'se_live_installer_completed'
    Push '$R9'
    Call SendTrackingEvent
    Pop $R9

    Call scienceFlush
FunctionEnd

; -----

Function onShowWelcome
    IntCmp $g_instCurrentScreenIndex 10 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 10
    StrCpy $g_instCurrentScreenName "Welcome"

    ; -- QueryAutoStart
    ${GetParameters} $R0

    ${StrStr} $AutoStart $R0 "/AUTOSTART"

    StrCmp $AutoStart "" done autostart

    autostart:
        StrCpy $g_instAutostart "true"

        Push 'se_live_installer_start'
        Push ''
        Call SendTrackingEvent

        StrCpy $R9 4 ;Relative page number. See below.
        Call RelGotoPage
        goto skip

    done:
        Push 'se_live_installer_start'
        Push ''
        Call SendTrackingEvent


skip:
FunctionEnd

Function onShowLicense
    IntCmp $g_instCurrentScreenIndex 20 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 20
    StrCpy $g_instCurrentScreenName "User Agreement"

    Push 'se_live_installer_show_user_agreement'
    Push ''
    Call SendTrackingEvent
skip:
FunctionEnd

Function onShowFolder
    IntCmp $g_instCurrentScreenIndex 30 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 30
    StrCpy $g_instCurrentScreenName "Select Target Folder"

    Push 'se_live_installer_show_select_target_folder'
    Push ''
    Call SendTrackingEvent
skip:
FunctionEnd

Function onShowComponents
    IntCmp $g_instCurrentScreenIndex 40 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 40
    StrCpy $g_instCurrentScreenName "Choose Components"

    Push 'se_live_installer_show_choose_components'
    Push ''
    Call SendTrackingEvent
skip:
FunctionEnd

Function onShowInstall
    IntCmp $g_instCurrentScreenIndex 50 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 50
    StrCpy $g_instCurrentScreenName "Install Progress"

    Push 'se_live_installer_show_install_progress'
    Push ''
    Call SendTrackingEvent
skip:
FunctionEnd

Function onShowFinish
    IntCmp $g_instCurrentScreenIndex 60 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 60
    StrCpy $g_instCurrentScreenName "Install Finished"

    Push 'se_live_installer_show_install_finished'
    Push ''
    Call SendTrackingEvent

skip:
FunctionEnd

Function onBeginDownloadOBS
    Push 'se_live_installer_download_obs_studio'
    Push ''
    Call SendTrackingEvent
FunctionEnd
