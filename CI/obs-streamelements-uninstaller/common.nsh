!include "FileFunc.nsh"
!include "TextFunc.nsh"
!include "LogicLib.nsh"
!include "Sections.nsh"
!include "WinVer.nsh"

!define PRODUCT_NAME "StreamElements SE.Live" ; Public facing name
!define PRODUCT_CODE_NAME "StreamElements OBS.Live" ; For internal use in Windows Registry for uninstaller links
!define PRODUCT_PUBLISHER "StreamElements"
!define PRODUCT_WEB_SITE "http://www.streamelements.com"

!define OBS_STUDIO_DOWNLOAD_BASE_URL "https://strms.net/obslive-download-obs-studio" ; -x.x.x-win32.exe ; -x.x.x-win64.exe ; -x.x.x-win.exe

!define MSG_DOWNLOAD_ERROR_RETRY "Failed downloading installation package.$\r$\n$\r$\nClick Retry to resume downloading or Cancel to abort."
!define MSG_DOWNLOAD_CANCEL_CONFIRM "Are you sure that you want to stop download and abort OBS.Live setup?"

!define MSG_DETAILS1 "Waving goodbye to the StreamElements ChatBot"
!define MSG_DETAILS2 "Unplugging your chat and activity feed from OBS Studio"

!define POPUP_QUIZ_DEFAULT_URL "https://obs.streamelements.com/uninstall-popup-quiz"

Function PreventMultipleInstances
    # Prevent multiple instances
    System::Call 'kernel32::CreateMutex(p 0, i 0, t "obs_streamelements_installer") p .r1 ?e'
    Pop $R0
    
    StrCmp $R0 0 no_other_installer

    MessageBox MB_OK|MB_ICONEXCLAMATION "The installer is already running."
        Push "Another running instance of the installer was detected"
        Call AbortByLocalError

    no_other_installer:
FunctionEnd

!define StrStr "!insertmacro StrStr"

!macro StrStr ResultVar String SubString
Push `${String}`
Push `${SubString}`
Call StrStr
Pop `${ResultVar}`
!macroend

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
