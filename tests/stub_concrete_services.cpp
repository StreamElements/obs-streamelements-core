//
// The link seams for the fifteen services SEApiServices still carries
// concretely: 51 methods, plus their constructors and destructors, plus the
// eleven free functions the handler bodies call.
//
// Why these are definitions rather than fakes: a pure virtual needs no
// definition, so an interface can be stood in for by deriving from it. These
// fifteen have no interface -- the handler bodies call the concrete class --
// so the only way to satisfy the linker is to BE the definition. That is the
// same trick api_handler_harness.cpp already uses for blog, obs_initialized
// and __QtPostTask_Impl, at a larger scale.
//
// This is a real limit and worth stating: one definition per method per
// binary, so unlike an interface fake these cannot differ between two tests
// in the same executable. They record and answer through the same journal, so
// behaviour is still programmable per test; what is not available is two
// instances behaving differently at once. No handler body needs that, because
// there is one services container.
//
// The list is the linker's, not the headers': it names exactly what
// SEApiMessageHandler.cpp references. A body that stops calling one of these
// leaves an unused definition here, which is harmless; a body that starts
// calling a new one fails to link, which is the signal.
//
// The constructors take their dependencies and ignore them. Every parameter
// is a pointer or a shared_ptr to a type that is only forward-declared at
// the call site, which is why FakeApiServices can pass null for all of them.
//
// One warning comes out of this on clang and it is an artefact of the trick
// rather than a defect: -Wunused-private-field on
// SEStatusBarMessageManager::m_mainWindow. The production .cpp uses it; this
// build does not compile that .cpp, so from here the field has no reader.
// Harmless -- tests/ does not use -Werror -- and emphatically not something
// to "fix" in the production header.
//

#include "fake_api_services.hpp"

#include "streamelements/SEAuxiliaryControlsBuilder.hpp"
#include "streamelements/SEBackupManager.hpp"
#include "streamelements/SEBrowserDialogManager.hpp"
#include "streamelements/SEHostInfoProvider.hpp"
#include "streamelements/SEHostStreamingManager.hpp"
#include "streamelements/SEHostUpdateManager.hpp"
#include "streamelements/SEHttpClient.hpp"
#include "streamelements/SEObsApiTransactionManager.hpp"
#include "streamelements/SEOutputManager.hpp"
#include "streamelements/SEOutputSettingsManager.hpp"
#include "streamelements/SEProfilesManager.hpp"
#include "streamelements/SESessionManager.hpp"
#include "streamelements/SEShellServices.hpp"
#include "streamelements/SEStatePersistence.hpp"
#include "streamelements/SEStatusBarMessageManager.hpp"

#include "streamelements/SEUtils/SEEnvConfigUtils.hpp"
#include "streamelements/SEUtils/SEObsSourceCatalogUtils.hpp"

#include <string>

// --- constructors and destructors -------------------------------------
//
// Parameter names are omitted deliberately: nothing is stored, and the
// suite builds with -Wall -Wextra on clang, where a named unused parameter
// is a warning.
//

SEAuxiliaryControlsBuilder::SEAuxiliaryControlsBuilder(
	std::weak_ptr<SEApiServices>)
{
}

SEAuxiliaryControlsBuilder::~SEAuxiliaryControlsBuilder() {}

SEBackupManager::SEBackupManager(std::shared_ptr<SECleanupManager>,
				 std::shared_ptr<SELocalFilesystemHttpServer>,
				 std::shared_ptr<SEConfig>)
{
}

SEBackupManager::~SEBackupManager() {}

SEBrowserDialogManager::SEBrowserDialogManager(QMainWindow *,
					       std::shared_ptr<SEApiServices>)
{
}

SEBrowserDialogManager::~SEBrowserDialogManager() {}

SEHostInfoProvider::SEHostInfoProvider(std::shared_ptr<SEAnalyticsEventsManager>)
{
}

SEHostInfoProvider::~SEHostInfoProvider() {}

SEHostStreamingManager::SEHostStreamingManager() {}

SEHostStreamingManager::~SEHostStreamingManager() {}

SEHostUpdateManager::SEHostUpdateManager() {}

SEHostUpdateManager::~SEHostUpdateManager() {}

//
// SEHttpClient defines its own constructor and destructor inline in
// its header, so there is nothing to supply here.
//

SEObsApiTransactionManager::SEObsApiTransactionManager(
	std::shared_ptr<SEPleaseWaitWindow>)
{
}

SEObsApiTransactionManager::~SEObsApiTransactionManager() {}

SEOutputManager::SEOutputManager(std::shared_ptr<SEVideoCompositionManager>,
				 std::shared_ptr<SEAudioCompositionManager>,
				 std::shared_ptr<SEHostEventDispatcher>)
{
}

SEOutputManager::~SEOutputManager() {}

SEOutputSettingsManager::SEOutputSettingsManager() {}

SEOutputSettingsManager::~SEOutputSettingsManager() {}

SEProfilesManager::SEProfilesManager() {}

SEProfilesManager::~SEProfilesManager() {}

SESessionManager::SESessionManager(QMainWindow *, QCefCookieManager *,
				   std::shared_ptr<SEApiServices>)
{
}

SESessionManager::~SESessionManager() {}

SEShellServices::SEShellServices() {}

SEShellServices::~SEShellServices() {}

SEStatePersistence::SEStatePersistence(QMainWindow *, SEBrowserWidgetManager *,
				       SEWorkerManager *, SEHotkeyManager *,
				       SENativeObsUIControlsManager *,
				       std::shared_ptr<SEHostEventDispatcher>,
				       std::shared_ptr<SEConfig>)
{
}

SEStatePersistence::~SEStatePersistence() {}

SEStatusBarMessageManager::SEStatusBarMessageManager(QMainWindow *) {}

SEStatusBarMessageManager::~SEStatusBarMessageManager() {}

// --- the methods ------------------------------------------------------

// --- SEAuxiliaryControlsBuilder: 1 method(s) ---

bool SEAuxiliaryControlsBuilder::ShowMenuAtMousePointerPosition(
	CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEAuxiliaryControlsBuilder",
			 "ShowMenuAtMousePointerPosition", a0);
	return se_fakes::BoolFor("SEAuxiliaryControlsBuilder",
				 "ShowMenuAtMousePointerPosition");
}

// --- SEBackupManager: 4 method(s) ---

void SEBackupManager::CreateLocalBackupPackage(CefRefPtr<CefValue> a0,
					       CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEBackupManager", "CreateLocalBackupPackage", a0);
	se_fakes::FillValue("SEBackupManager", "CreateLocalBackupPackage", a1);
}

void SEBackupManager::QueryBackupPackageContent(CefRefPtr<CefValue> a0,
						CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEBackupManager", "QueryBackupPackageContent", a0);
	se_fakes::FillValue("SEBackupManager", "QueryBackupPackageContent", a1);
}

void SEBackupManager::QueryLocalBackupPackageReferencedFiles(
	CefRefPtr<CefValue> a0, CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEBackupManager",
			 "QueryLocalBackupPackageReferencedFiles", a0);
	se_fakes::FillValue("SEBackupManager",
			    "QueryLocalBackupPackageReferencedFiles", a1);
}

void SEBackupManager::RestoreBackupPackageContent(CefRefPtr<CefValue> a0,
						  CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEBackupManager", "RestoreBackupPackageContent", a0);
	se_fakes::FillValue("SEBackupManager", "RestoreBackupPackageContent",
			    a1);
}

// --- SEBrowserDialogManager: 6 method(s) ---

bool SEBrowserDialogManager::DeserializeCloseNonModalDialogsByIds(
	CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEBrowserDialogManager",
			 "DeserializeCloseNonModalDialogsByIds", a0);
	return se_fakes::BoolFor("SEBrowserDialogManager",
				 "DeserializeCloseNonModalDialogsByIds");
}

bool SEBrowserDialogManager::DeserializeFocusNonModalDialogById(
	CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEBrowserDialogManager",
			 "DeserializeFocusNonModalDialogById", a0);
	return se_fakes::BoolFor("SEBrowserDialogManager",
				 "DeserializeFocusNonModalDialogById");
}

bool SEBrowserDialogManager::DeserializeModalDialog(CefRefPtr<CefValue> a0,
						    CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEBrowserDialogManager", "DeserializeModalDialog",
			 a0);
	se_fakes::FillValue("SEBrowserDialogManager", "DeserializeModalDialog",
			    a1);
	return se_fakes::BoolFor("SEBrowserDialogManager",
				 "DeserializeModalDialog");
}

std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
SEBrowserDialogManager::DeserializeNonModalDialog(CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEBrowserDialogManager", "DeserializeNonModalDialog",
			 a0);
	return se_fakes::PromiseFor("SEBrowserDialogManager",
				    "DeserializeNonModalDialog");
}

bool SEBrowserDialogManager::DeserializeNonModalDialogDimensionsById(
	CefRefPtr<CefValue> a0, CefRefPtr<CefValue> a1)
{
	se_fakes::Record("SEBrowserDialogManager",
			 "DeserializeNonModalDialogDimensionsById", a0, a1);
	return se_fakes::BoolFor("SEBrowserDialogManager",
				 "DeserializeNonModalDialogDimensionsById");
}

void SEBrowserDialogManager::SerializeAllNonModalDialogs(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEBrowserDialogManager",
			 "SerializeAllNonModalDialogs");
	se_fakes::FillValue("SEBrowserDialogManager",
			    "SerializeAllNonModalDialogs", a0);
}

// --- SEHostInfoProvider: 5 method(s) ---

void SEHostInfoProvider::SerializeHostProperties(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostInfoProvider", "SerializeHostProperties");
	se_fakes::FillValue("SEHostInfoProvider", "SerializeHostProperties",
			    a0);
}

void SEHostInfoProvider::SerializeLoadedHostModules(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostInfoProvider", "SerializeLoadedHostModules");
	se_fakes::FillValue("SEHostInfoProvider", "SerializeLoadedHostModules",
			    a0);
}

void SEHostInfoProvider::SerializeSystemCPUUsageTimes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostInfoProvider", "SerializeSystemCPUUsageTimes");
	se_fakes::FillValue("SEHostInfoProvider",
			    "SerializeSystemCPUUsageTimes", a0);
}

void SEHostInfoProvider::SerializeSystemHardwareProperties(
	CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostInfoProvider",
			 "SerializeSystemHardwareProperties");
	se_fakes::FillValue("SEHostInfoProvider",
			    "SerializeSystemHardwareProperties", a0);
}

void SEHostInfoProvider::SerializeSystemMemoryUsage(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostInfoProvider", "SerializeSystemMemoryUsage");
	se_fakes::FillValue("SEHostInfoProvider", "SerializeSystemMemoryUsage",
			    a0);
}

// --- SEHostStreamingManager: 2 method(s) ---

void SEHostStreamingManager::RequestStop()
{
	se_fakes::Record("SEHostStreamingManager", "RequestStop");
}

void SEHostStreamingManager::SerializeStreamingStatus(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostStreamingManager", "SerializeStreamingStatus");
	se_fakes::FillValue("SEHostStreamingManager",
			    "SerializeStreamingStatus", a0);
}

// --- SEHostUpdateManager: 3 method(s) ---

void SEHostUpdateManager::DeserializeReleaseGroupProperties(
	CefRefPtr<CefValue> a0, CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEHostUpdateManager",
			 "DeserializeReleaseGroupProperties", a0);
	se_fakes::FillValue("SEHostUpdateManager",
			    "DeserializeReleaseGroupProperties", a1);
}

void SEHostUpdateManager::RequestCheckForUpdates(bool a0, bool a1, bool a2)
{
	se_fakes::Record("SEHostUpdateManager", "RequestCheckForUpdates", a0,
			 a1, a2);
}

void SEHostUpdateManager::SerializeReleaseGroupProperties(
	CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEHostUpdateManager",
			 "SerializeReleaseGroupProperties");
	se_fakes::FillValue("SEHostUpdateManager",
			    "SerializeReleaseGroupProperties", a0);
}

// --- SEHttpClient: 1 method(s) ---

//
// a1 is the result, taken by value, so the only way to answer is to mutate
// what it points at -- which is also why httpRequestText cannot be given a
// reply by replacing the handle.
//
void SEHttpClient::DeserializeHttpRequestText(CefRefPtr<CefValue> a0,
					      CefRefPtr<CefValue> a1)
{
	se_fakes::Record("SEHttpClient", "DeserializeHttpRequestText", a0);
	se_fakes::FillValueInPlace("SEHttpClient", "DeserializeHttpRequestText",
				   a1);
}

// --- SEObsApiTransactionManager: 4 method(s) ---

void SEObsApiTransactionManager::BeginDeferSave()
{
	se_fakes::Record("SEObsApiTransactionManager", "BeginDeferSave");
}

void SEObsApiTransactionManager::Complete(std::string a0)
{
	se_fakes::Record("SEObsApiTransactionManager", "Complete", a0);
}

std::string SEObsApiTransactionManager::Create(int a0)
{
	se_fakes::Record("SEObsApiTransactionManager", "Create", a0);
	return se_fakes::StringFor("SEObsApiTransactionManager", "Create");
}

void SEObsApiTransactionManager::EndDeferSave()
{
	se_fakes::Record("SEObsApiTransactionManager", "EndDeferSave");
}

// --- SEOutputManager: 7 method(s) ---

void SEOutputManager::DeserializeOutput(ObsOutputType a0,
					CefRefPtr<CefValue> a1,
					CefRefPtr<CefValue> &a2)
{
	se_fakes::Record("SEOutputManager", "DeserializeOutput", a0, a1);
	se_fakes::FillValue("SEOutputManager", "DeserializeOutput", a2);
}

void SEOutputManager::DisableOutputsByIds(ObsOutputType a0,
					  CefRefPtr<CefValue> a1,
					  CefRefPtr<CefValue> &a2)
{
	se_fakes::Record("SEOutputManager", "DisableOutputsByIds", a0, a1);
	se_fakes::FillValue("SEOutputManager", "DisableOutputsByIds", a2);
}

void SEOutputManager::EnableOutputsByIds(ObsOutputType a0,
					 CefRefPtr<CefValue> a1,
					 CefRefPtr<CefValue> &a2)
{
	se_fakes::Record("SEOutputManager", "EnableOutputsByIds", a0, a1);
	se_fakes::FillValue("SEOutputManager", "EnableOutputsByIds", a2);
}

void SEOutputManager::RemoveOutputsByIds(ObsOutputType a0,
					 CefRefPtr<CefValue> a1,
					 CefRefPtr<CefValue> &a2)
{
	se_fakes::Record("SEOutputManager", "RemoveOutputsByIds", a0, a1);
	se_fakes::FillValue("SEOutputManager", "RemoveOutputsByIds", a2);
}

void SEOutputManager::SerializeAllOutputs(ObsOutputType a0,
					  CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEOutputManager", "SerializeAllOutputs", a0);
	se_fakes::FillValue("SEOutputManager", "SerializeAllOutputs", a1);
}

void SEOutputManager::TriggerSaveReplayBufferById(CefRefPtr<CefValue> a0,
						  CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEOutputManager", "TriggerSaveReplayBufferById", a0);
	se_fakes::FillValue("SEOutputManager", "TriggerSaveReplayBufferById",
			    a1);
}

void SEOutputManager::TriggerSplitRecordingOutputById(CefRefPtr<CefValue> a0,
						      CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEOutputManager", "TriggerSplitRecordingOutputById",
			 a0);
	se_fakes::FillValue("SEOutputManager",
			    "TriggerSplitRecordingOutputById", a1);
}

// --- SEOutputSettingsManager: 7 method(s) ---

void SEOutputSettingsManager::GetAvailableAudioEncoders(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEOutputSettingsManager",
			 "GetAvailableAudioEncoders");
	se_fakes::FillValue("SEOutputSettingsManager",
			    "GetAvailableAudioEncoders", a0);
}

void SEOutputSettingsManager::GetAvailableEncoders(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEOutputSettingsManager", "GetAvailableEncoders");
	se_fakes::FillValue("SEOutputSettingsManager", "GetAvailableEncoders",
			    a0);
}

void SEOutputSettingsManager::GetAvailableVideoEncoders(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEOutputSettingsManager",
			 "GetAvailableVideoEncoders");
	se_fakes::FillValue("SEOutputSettingsManager",
			    "GetAvailableVideoEncoders", a0);
}

void SEOutputSettingsManager::GetEncoderProperties(CefRefPtr<CefValue> a0,
						   CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEOutputSettingsManager", "GetEncoderProperties", a0);
	se_fakes::FillValue("SEOutputSettingsManager", "GetEncoderProperties",
			    a1);
}

bool SEOutputSettingsManager::GetEncodingSettings(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEOutputSettingsManager", "GetEncodingSettings");
	se_fakes::FillValue("SEOutputSettingsManager", "GetEncodingSettings",
			    a0);
	return se_fakes::BoolFor("SEOutputSettingsManager",
				 "GetEncodingSettings");
}

bool SEOutputSettingsManager::SetEncodingSettings(CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEOutputSettingsManager", "SetEncodingSettings", a0);
	return se_fakes::BoolFor("SEOutputSettingsManager",
				 "SetEncodingSettings");
}

bool SEOutputSettingsManager::SetStreamingSettings(CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEOutputSettingsManager", "SetStreamingSettings", a0);
	return se_fakes::BoolFor("SEOutputSettingsManager",
				 "SetStreamingSettings");
}

// --- SEProfilesManager: 3 method(s) ---

bool SEProfilesManager::DeserializeCurrentProfileById(CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEProfilesManager", "DeserializeCurrentProfileById",
			 a0);
	return se_fakes::BoolFor("SEProfilesManager",
				 "DeserializeCurrentProfileById");
}

void SEProfilesManager::SerializeAllProfiles(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEProfilesManager", "SerializeAllProfiles");
	se_fakes::FillValue("SEProfilesManager", "SerializeAllProfiles", a0);
}

void SEProfilesManager::SerializeCurrentProfile(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEProfilesManager", "SerializeCurrentProfile");
	se_fakes::FillValue("SEProfilesManager", "SerializeCurrentProfile", a0);
}

// --- SESessionManager: 2 method(s) ---

void SESessionManager::DeleteCookies()
{
	se_fakes::Record("SESessionManager", "DeleteCookies");
}

void SESessionManager::Reset(bool a0, SESessionManager::UiModifier a1)
{
	se_fakes::Record("SESessionManager", "Reset", a0, a1);
}

// --- SEShellServices: 2 method(s) ---

bool SEShellServices::OpenDefaultBrowser(std::string a0)
{
	se_fakes::Record("SEShellServices", "OpenDefaultBrowser", a0);
	return se_fakes::BoolFor("SEShellServices", "OpenDefaultBrowser");
}

void SEShellServices::RevealFileInGraphicalShell(CefRefPtr<CefValue> a0,
						 CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEShellServices", "RevealFileInGraphicalShell", a0);
	se_fakes::FillValue("SEShellServices", "RevealFileInGraphicalShell",
			    a1);
}

// --- SEStatePersistence: 3 method(s) ---

bool SEStatePersistence::DeserializeUserInterfaceState(CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEStatePersistence", "DeserializeUserInterfaceState",
			 a0);
	return se_fakes::BoolFor("SEStatePersistence",
				 "DeserializeUserInterfaceState");
}

void SEStatePersistence::PersistState(bool a0)
{
	se_fakes::Record("SEStatePersistence", "PersistState", a0);
}

void SEStatePersistence::SerializeUserInterfaceState(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEStatePersistence", "SerializeUserInterfaceState");
	se_fakes::FillValue("SEStatePersistence", "SerializeUserInterfaceState",
			    a0);
}

// --- SEStatusBarMessageManager: 1 method(s) ---

bool SEStatusBarMessageManager::DeserializeStatusBarTemporaryMessage(
	CefRefPtr<CefValue> a0)
{
	se_fakes::Record("SEStatusBarMessageManager",
			 "DeserializeStatusBarTemporaryMessage", a0);
	return se_fakes::BoolFor("SEStatusBarMessageManager",
				 "DeserializeStatusBarTemporaryMessage");
}

// --- the free functions: 11, ten of them the source catalog ---
//
// Not services and needing no interface: a free function declared in a
// clean header compiles anywhere and the test links its own body, which
// is the cheapest of the three shapes. The ten catalog wrappers are the
// ones that matter -- they carry all 27 OBS_SOURCE_* constants on their
// own side of the header, so a handler body naming one names no obs type.
//

std::string GetCurrentThemeName()
{
	se_fakes::Record("SEEnvConfigUtils", "GetCurrentThemeName");
	return se_fakes::StringFor("SEEnvConfigUtils", "GetCurrentThemeName");
}

void SerializeAvailableAudioFilterSourceTypes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeAvailableAudioFilterSourceTypes");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeAvailableAudioFilterSourceTypes", a0);
}

void SerializeAvailableAudioInputSourceTypes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeAvailableAudioInputSourceTypes");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeAvailableAudioInputSourceTypes", a0);
}

void SerializeAvailableFilterSourceTypes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeAvailableFilterSourceTypes");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeAvailableFilterSourceTypes", a0);
}

void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeAvailableInputSourceTypes");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeAvailableInputSourceTypes", a0);
}

void SerializeAvailableVideoFilterSourceTypes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeAvailableVideoFilterSourceTypes");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeAvailableVideoFilterSourceTypes", a0);
}

void SerializeAvailableVideoInputSourceTypes(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeAvailableVideoInputSourceTypes");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeAvailableVideoInputSourceTypes", a0);
}

void SerializeExistingAudioInputSources(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeExistingAudioInputSources");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeExistingAudioInputSources", a0);
}

void SerializeExistingInputSources(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeExistingInputSources");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeExistingInputSources", a0);
}

void SerializeExistingVideoInputSources(CefRefPtr<CefValue> &a0)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeExistingVideoInputSources");
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeExistingVideoInputSources", a0);
}

void SerializeObsSourceProperties(CefRefPtr<CefValue> a0,
				  CefRefPtr<CefValue> &a1)
{
	se_fakes::Record("SEObsSourceCatalogUtils",
			 "SerializeObsSourceProperties", a0);
	se_fakes::FillValue("SEObsSourceCatalogUtils",
			    "SerializeObsSourceProperties", a1);
}
