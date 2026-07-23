!define PRODUCT_SHORT_NAME "SE.Live" ; Public
!define PRODUCT_NAME "StreamElements SE.Live" ; Public
!define PRODUCT_CODE_NAME "StreamElements OBS.Live" ; Internal use for uninstaller registry entries
!define PRODUCT_PUBLISHER "StreamElements"
!define PRODUCT_WEB_SITE "http://www.streamelements.com"
!define PRODUCT_ABOUT_URL "http://www.streamelements.com"
!define PRODUCT_NEWS_URL "https://se.live/"
!define PRODUCT_SUPPORT_URL "https://discordapp.com/channels/141203863863558144/466251347587629077"

# !define OBS_DOWNLOAD_VERSION "27.2.4"
#!define OBS_DOWNLOAD_URL_64 "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-${OBS_DOWNLOAD_VERSION}-Full-Installer-x64.exe"
#!define OBS_DOWNLOAD_URL_32 "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-${OBS_DOWNLOAD_VERSION}-Full-Installer-x86.exe"
!define OBS_DOWNLOAD_VERSION ""
!define OBS_DOWNLOAD_URL_64 "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-31.1.2-Windows-x64-Installer.exe"
!define OBS_DOWNLOAD_URL_32 "https://cdn-fastly.obsproject.com/downloads/OBS-Studio-27.2.4-Full-Installer-x86.exe"
!define MSVC_REDIST_X64_URL "https://cdn.streamelements.com/obs/dist/third-party/microsoft/vs2019/vcredist_x64.exe"
!define MSVC_REDIST_X86_URL "https://cdn.streamelements.com/obs/dist/third-party/microsoft/vs2019/vcredist_x86.exe"

!define OBS_REQUIRE_VERSION "31.1.0"

!define MSG_DOWNLOAD_ERROR_RETRY "Failed downloading installation package.$\r$\n$\r$\nClick Retry to resume downloading or Cancel to abort."
!define MSG_DOWNLOAD_CANCEL_CONFIRM "Are you sure that you want to stop download and abort ${PRODUCT_SHORT_NAME} setup?"
; TODO: Code signing
;!finalize '"C:\Program Files (x86)\Windows Kits\10\bin\x64\signtool.exe"'

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
