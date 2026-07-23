!include "common.nsh"

!include "science.nsh"

# Installer file name
OutFile "obs-streamelements-uninstaller.exe"

# Product display name
Name "${PRODUCT_NAME}"

# Brand on bottom of installer window
BrandingText "${PRODUCT_NAME} Uninstaller"

ReserveFile "resources\obs-probe-libobs-version-32.exe"
ReserveFile "resources\obs-probe-libobs-version-64.exe"
ReserveFile "resources\MicrosoftEasyFix51044.msi"

########################################################################
# Global vars
########################################################################

    Var /GLOBAL g_scienceFields
    Var /GLOBAL g_scienceProps

########################################################################
# HEAP global vars
########################################################################

    Var /GLOBAL g_instCurrentScreenName
    Var /GLOBAL g_instCurrentScreenIndex
    Var /GLOBAL g_instAbortSource
    Var /GLOBAL g_instAbortReason
    Var /GLOBAL g_instCompleted
    Var /GLOBAL g_instPrevEvent

########################################################################
# Utilities
########################################################################

    ; Usage:
    ; ${Trim} $trimmedString $originalString
    !define Trim "!insertmacro Trim"
    
    !macro Trim ResultVar String
    Push "${String}"
    Call Trim
    Pop "${ResultVar}"
    !macroend

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
    !define MUI_WELCOMEPAGE_TITLE "Goodbye, friend, we'll miss you..."
    ;!define MUI_WELCOMEPAGE_TITLE_3LINES
    !define MUI_WELCOMEPAGE_TEXT "Click 'Next' to remove the SE.Live StreamElements Add-On from OBS Studio."

    ; Finish page
    !define MUI_FINISHPAGE_TITLE "${PRODUCT_NAME} was removed."
    !define MUI_FINISHPAGE_TEXT "${PRODUCT_NAME} has been removed from your computer.$\r$\n$\r$\nClick 'Finish' to close setup."
    !define MUI_FINISHPAGE_TEXT_REBOOT "Your computer must be restarted in order to complete the uninstallation of ${PRODUCT_NAME}. Do you want to reboot now?"
    !define MUI_FINISHPAGE_TEXT_LARGE

    !define MUI_FINISHPAGE_LINK_LOCATION "${PRODUCT_WEB_SITE}"
    !define MUI_FINISHPAGE_LINK "Visit ${PRODUCT_PUBLISHER} Website"

    ; Pages sequence
    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowWelcome
    !insertmacro MUI_PAGE_WELCOME

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowInstall
    !insertmacro MUI_PAGE_INSTFILES

    !define MUI_PAGE_CUSTOMFUNCTION_SHOW onShowFinish
    !insertmacro MUI_PAGE_FINISH

    ; Language, must be located after MUI_PAGE_XXX macros above
    !insertmacro MUI_LANGUAGE "English"

    Var /GLOBAL g_Locked_Files_Checked

Function onGUIInit
    ;-- Aero::Apply /NOBRANDING ""
FunctionEnd

Function LockedListShow
    StrCmp $g_Locked_Files_Checked "true" already_checked

    DetailPrint "Checking for locked files..."
    SetPluginUnload alwaysoff
    NSISLockDetector::AddWildcardPattern "$INSTDIR\bin\*.exe"
    NSISLockDetector::AddWildcardPattern "$INSTDIR\bin\*.dll"
    NSISLockDetector::AddWildcardPattern "$INSTDIR\obs-plugins\*.exe"
    NSISLockDetector::AddWildcardPattern "$INSTDIR\obs-plugins\*.dll"
    NSISLockDetector::AddWildcardPattern "$INSTDIR\data\*.exe"
    NSISLockDetector::AddWildcardPattern "$INSTDIR\data\*.dll"
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

Section "section_main" section_main
    # DetailPrint to appear both in details view and status bar
    SetDetailsPrint both

    Call QueryInstallDir

    SetOutPath $INSTDIR

    DetailPrint "${MSG_DETAILS1}"
    DetailPrint "${MSG_DETAILS2}"

    Call LockedListShow

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements.dll"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements.pdb"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core.dll"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core.pdb"

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements.qt5.dymod"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements.qt5.pdb"

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements.qt6.dymod"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements.qt6.pdb"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core.qt5.dymod"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core.qt5.pdb"

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core.qt6.dymod"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core.qt6.pdb"

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core-streamelements-restore-script-host.exe"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core-streamelements-restore-script-host.pdb"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-set-machine-config.exe"

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements_qt5.dll"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements_qt5.pdb"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core_qt5.dll"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core_qt5.pdb"

    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements_qt6.dll"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements_qt6.pdb"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core_qt6.dll"
    Delete "$INSTDIR\obs-plugins\64bit\obs-streamelements-core_qt6.pdb"

    Delete "$INSTDIR\bin\64bit\BsSndRpt64.exe"
    Delete "$INSTDIR\bin\64bit\BugSplat64.dll"
    Delete "$INSTDIR\bin\64bit\BugSplatHD64.exe"
    Delete "$INSTDIR\bin\64bit\BugSplatRc64.dll"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements.qt5.dymod"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements.qt5.pdb"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements.qt6.dymod"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements.qt6.pdb"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core.qt5.dymod"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core.qt5.pdb"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core.qt6.dymod"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core.qt6.pdb"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core-streamelements-restore-script-host.exe"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core-streamelements-restore-script-host.pdb"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-set-machine-config.exe"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements.dll"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements.pdb"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core.dll"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core.pdb"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements_qt5.dll"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements_qt5.pdb"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core_qt5.dll"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core_qt5.pdb"

    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements_qt6.dll"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements_qt6.pdb"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core_qt6.dll"
    Delete "$INSTDIR\obs-plugins\32bit\obs-streamelements-core_qt6.pdb"

    Delete "$INSTDIR\bin\32bit\BsSndRpt.exe"
    Delete "$INSTDIR\bin\32bit\BugSplat.dll"
    Delete "$INSTDIR\bin\32bit\BugSplatHD.exe"
    Delete "$INSTDIR\bin\32bit\BugSplatRc.dll"

    Delete "$DESKTOP\${PRODUCT_NAME}.lnk"
    DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\${PRODUCT_CODE_NAME}"

    Delete /REBOOTOK "$EXEPATH"

    SetRebootFlag true
SectionEnd

Function .onGUIEnd
    Call onInstallationEnd
FunctionEnd

Function .onInstSuccess 
    Call FinishInstallation
FunctionEnd

Function .onInit
    Call scienceInit
    Call PreventMultipleInstances

    InitPluginsDir

    # no OBS detected, download and install OBS
    SectionSetFlags ${section_main} 17 ; 17 = 1 (selected) + 16 (readonly)
FunctionEnd

Function .onVerifyInstDir
FunctionEnd

Function QueryInstallDir
    StrCmp "$INSTDIR" "" query done

query:
    # Query 64bit location
    ClearErrors
    ReadRegStr $INSTDIR HKLM "SOFTWARE\WOW6432Node\OBS Studio" ""
    IfErrors query32bit
    goto done

query32bit:
    # Try 32bit location
    ClearErrors
    ReadRegStr $INSTDIR HKLM "SOFTWARE\OBS Studio" ""
    IfErrors setdefault
    goto done

setdefault:
    # Set default installation folder
    ;InstallDir "$PROGRAMFILES32\obs-studio3"
    StrCpy $INSTDIR ""
done:
FunctionEnd

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
    StrCpy $g_scienceFields '["plugin_version", ""], ["setup_version", ""], ["seconds_since_start", "$R0"], ["seconds_since_previous_event", "$R1"]'

    ; --- Get windows version ---
    ${GetWindowsVersion} $R0

    StrCpy $g_scienceProps '"feature": "obs_core_plugin_uninstaller", "name": "$R8", "sessionId": "$g_scienceSessionId", "_meta": {"mobile": false, "os": "Windows", "platform": "desktop", "osversion": "$R0"}'

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

    Push 'se_live_uninstaller_abort'
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

    Push 'se_live_uninstaller_abort'
    Push ''
    Call SendTrackingEvent
    Call scienceFlush

    Abort
FunctionEnd

Function onInstallationEnd
    StrCmp $g_instCompleted "true" skip

    Push "User cancelled uninstaller"
    Call AbortByUserRequest

skip:
FunctionEnd

Function FinishInstallation
    StrCpy $g_instCompleted "true"

    Push $R9

    Push 'se_live_uninstaller_completed'
    Push ''
    Call SendTrackingEvent
    Pop $R9

    Call scienceFlush
FunctionEnd

; -----

Function onShowWelcome
    ; Set "install" button text to "Next >"
    GetDlgItem $0 $HWNDPARENT 1
    SendMessage $0 ${WM_SETTEXT} 0 "STR:$(^NextBtn)"

    IntCmp $g_instCurrentScreenIndex 10 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 10
    StrCpy $g_instCurrentScreenName "Welcome"
    Goto done

    done:
        Push 'se_live_uninstaller_start'
        Push ''
        Call SendTrackingEvent

skip:
FunctionEnd

Function onShowInstall
    IntCmp $g_instCurrentScreenIndex 20 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 20
    StrCpy $g_instCurrentScreenName "Uninstall Progress"

    Push 'se_live_uninstaller_show_progress'
    Push ''
    Call SendTrackingEvent
skip:
FunctionEnd

Function onShowFinish
    IntCmp $g_instCurrentScreenIndex 30 skip proceed skip
proceed:
    StrCpy $g_instCurrentScreenIndex 30
    StrCpy $g_instCurrentScreenName "Uninstall Finished"

    Push 'se_live_uninstaller_show_uninstall_finished'
    Push ''
    Call SendTrackingEvent

    Call ShowFarewellPopup
skip:
FunctionEnd

########################################################################
# Download and install
########################################################################

Var /GLOBAL g_popupQuizUrl

Function ShowFarewellPopup
    SetPluginUnload alwaysoff
    Push "UninstallerPopupQuizUrl"
    NSISConfig::ReadProductEnvironmentConfigurationString
    Pop $g_popupQuizUrl

    StrCmp "$g_popupQuizUrl" "" no_url has_url
    no_url:
        StrCpy $g_popupQuizUrl "${POPUP_QUIZ_DEFAULT_URL}"
    has_url:

        ${StrStr} $0 "$g_popupQuizUrl" "?"
        StrCmp "$0" "" no_qs_args has_qs_args

        no_qs_args:
            StrCpy $g_popupQuizUrl "$g_popupQuizUrl?identity=$g_scienceHostMachineUniqueId&session=$g_scienceSessionId"
            goto proceed

        has_qs_args:
            StrCpy $g_popupQuizUrl "$g_popupQuizUrl&identity=$g_scienceHostMachineUniqueId&session=$g_scienceSessionId"
            goto proceed

        proceed:
            ;nsWeb::IsInet 0
            ;StrCmp "$0" "1" has_inet no_inet
            goto has_inet

            has_inet:
                ;nsWeb::ShowWebInPopUp "$g_popupQuizUrl"
                ExecShell "open" "$g_popupQuizUrl"
                goto done

            no_inet:
                ; No internet connect: no point in opening the pop-up URL
                goto done

done:
FunctionEnd
