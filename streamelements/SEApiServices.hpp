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
class SEHostEventDispatcher;
class SEMessageBus;
class SESharedVideoCompositionManager;
class SEVideoCompositionManager;
class SEAudioCompositionManager;
class SEOutputManager;
class SEObsActiveSceneTracker;
class SEStatePersistence;
class SEStatusBarMessageManager;
class SEPopupWindowManager;
class SEBrowserDialogManager;
class SESessionManager;

//
// The objects SEGlobalStateManager::Initialize() creates, in one place, so
// that something which needs several of them can be handed them all at once
// instead of reaching for a singleton. A container and nothing else: plain
// data members, no methods, no accessors, no policy. Expect it to be broken
// into smaller pieces as consumers turn out to need only a part of it.
//
// A member is null until Initialize() has got that far. After that it stays
// set for as long as the container lives: the container's lifetime is the
// root manager's, and nothing empties it at teardown.
//
// Anything in here whose own lifetime is controlled by a service in here --
// the browser widgets a widget manager destroys, the dialogs a dialog
// manager destroys, and the API handlers they own -- holds this container
// through a weak_ptr. That rule is what keeps the graph acyclic, and it is
// enforced by
// check_nothing_but_the_manager_owns_the_services_container.
//
// Field order matters: fields are destroyed in reverse declaration order.
// statePersistence borrows raw pointers to four of the managers above it, so
// it must stay below them, and the managers that own dialogs and pop-up
// windows stay below it so their widgets go before the docks.
//
struct QCef;

struct SEApiServices {
	//
	// Borrowed, not owned: SEGlobalStateManager creates the browser panel
	// and deletes it in Shutdown(). It is in here because it is one of
	// the objects Initialize() creates, and because the things that need
	// it -- browser widgets -- are handed this bundle and not the panel.
	//
	QCef *cef = nullptr;

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

	//
	// Host events on their way to pages. Everything that announces a
	// host* event holds this rather than fetching the server.
	//
	std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher;

	//
	// Local and external event listeners -- pages, background workers,
	// browser sources and external controllers.
	//
	std::shared_ptr<SEMessageBus> messageBus;
	std::shared_ptr<SESharedVideoCompositionManager>
		sharedVideoCompositionManager;
	std::shared_ptr<SEVideoCompositionManager> videoCompositionManager;
	std::shared_ptr<SEAudioCompositionManager> audioCompositionManager;
	std::shared_ptr<SEOutputManager> outputManager;
	std::shared_ptr<SEObsActiveSceneTracker> obsActiveSceneTracker;
	std::shared_ptr<SEStatePersistence> statePersistence;

	//
	// Extracted from SEGlobalStateManager, and declared after
	// statePersistence on purpose: fields are destroyed in reverse
	// declaration order, and what these own has to go before the
	// managers above that serialize it.
	//
	std::shared_ptr<SEStatusBarMessageManager> statusBarMessageManager;
	std::shared_ptr<SEPopupWindowManager> popupWindowManager;
	std::shared_ptr<SEBrowserDialogManager> browserDialogManager;
	std::shared_ptr<SESessionManager> sessionManager;
};
