#pragma once

#define STREAMELEMENTS_UPDATE_CONFIG_FILENAME "obs-streamelements-updater.ini"
#define STREAMELEMENTS_UPDATE_MANIFEST_FILENAME "obs-streamelements.manifest"

#ifdef __APPLE__
#define STREAMELEMENTS_DEFAULT_UPDATE_MANIFEST_URL "https://cdn.streamelements.com/obs/dist/obs-streamelements/macos/latest/obs-streamelements.manifest"
#define STREAMELEMENTS_UPDATE_PACKAGE_FILENAME "obs-streamelements-update.pkg"
#else
#define STREAMELEMENTS_DEFAULT_UPDATE_MANIFEST_URL "https://cdn.streamelements.com/obs/dist/obs-streamelements/windows/latest/obs-streamelements.manifest"
#define STREAMELEMENTS_UPDATE_PACKAGE_FILENAME "obs-streamelements-update.exe"
#endif

#define STREAMELEMENTS_PRODUCT_NAME "OBS.Live"

#define Message_DownloadingUpdateManifest "Downloading StreamElements SE.Live Manifest ..."
#define Message_DownloadingUpdatePackage "Downloading StreamElements SE.Live Add-On ..."
#define Message_UpdateFailed_Title "Update failed"
#define Message_UpdateFailed_Text "Failed downloading and installing StreamElements update package. Please try again later."
#define Message_ActiveOutput_Title "Cannot update while output is active"
#define Message_ActiveOutput_Text "To update SE.Live, please make sure that streaming and recording are not active"
