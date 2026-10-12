#pragma once

#include <memory>

class SEBrowserWidgetManager;
class SEObsSceneManager;
class SEMenuManager;
class SEBandwidthTestManager;
class SEOutputSettingsManager;
class SEWorkerManager;
class SEHotkeyManager;
class SEPerformanceHistoryTracker;
class SEAnalyticsEventsManager;
class SEExternalSceneDataProviderManager;
class SEHttpClient;
class SELocalFilesystemHttpServer;
class SENativeOBSControlsManager;
class SEProfilesManager;
class SEBackupManager;
class SERazerWyvrnManager;
class SECleanupManager;
class SEPreviewManager;
class SEWebsocketApiServer;
class SESharedVideoCompositionManager;
class SEVideoCompositionManager;
class SEAudioCompositionManager;
class SEOutputManager;
class SEObsActiveSceneTracker;

//
// The objects SEGlobalStateManager::Initialize() creates, in one place, so
// that something which needs several of them can be handed them all at once
// instead of reaching for a singleton. A container and nothing else: plain
// data members, no methods, no accessors, no policy. Expect it to be broken
// into smaller pieces as consumers turn out to need only a part of it.
//
// A member is null until Initialize() has got that far, and null again once
// the manager's destructor has emptied the container -- which it does
// because the widget managers in here own widgets that hold a reference
// back to this, and refcounting alone never unwinds that.
//
struct SEApiServices {
	std::shared_ptr<SEBrowserWidgetManager> widgetManager;
	std::shared_ptr<SEObsSceneManager> obsSceneManager;
	std::shared_ptr<SEMenuManager> menuManager;
	std::shared_ptr<SEBandwidthTestManager> bandwidthTestManager;
	std::shared_ptr<SEOutputSettingsManager> outputSettingsManager;
	std::shared_ptr<SEWorkerManager> workerManager;
	std::shared_ptr<SEHotkeyManager> hotkeyManager;
	std::shared_ptr<SEPerformanceHistoryTracker> performanceHistoryTracker;
	std::shared_ptr<SEAnalyticsEventsManager> analyticsEventsManager;
	std::shared_ptr<SEExternalSceneDataProviderManager>
		externalSceneDataProviderManager;
	std::shared_ptr<SEHttpClient> httpClient;
	std::shared_ptr<SELocalFilesystemHttpServer> localFilesystemHttpServer;
	std::shared_ptr<SENativeOBSControlsManager> nativeObsControlsManager;
	std::shared_ptr<SEProfilesManager> profilesManager;
	std::shared_ptr<SEBackupManager> backupManager;
#ifdef SE_ENABLE_WYVRN
	std::shared_ptr<SERazerWyvrnManager> razerWyvrnManager;
#endif
	std::shared_ptr<SECleanupManager> cleanupManager;
	std::shared_ptr<SEPreviewManager> previewManager;
	std::shared_ptr<SEWebsocketApiServer> websocketApiServer;
	std::shared_ptr<SESharedVideoCompositionManager>
		sharedVideoCompositionManager;
	std::shared_ptr<SEVideoCompositionManager> videoCompositionManager;
	std::shared_ptr<SEAudioCompositionManager> audioCompositionManager;
	std::shared_ptr<SEOutputManager> outputManager;
	std::shared_ptr<SEObsActiveSceneTracker> obsActiveSceneTracker;
};
