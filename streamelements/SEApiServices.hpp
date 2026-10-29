#pragma once

#include <memory>

class SEBrowserWidgetManager;
class SEObsSceneManager;
class SEMenuManager;
class ISEBandwidthTestManager;
class SEOutputSettingsManager;
class ISEWorkerManager;
class ISEHotkeyManager;
class SEPerformanceHistoryTracker;
class SEAnalyticsEventsManager;
class ISEExternalSceneDataProviderManager;
class SEHttpClient;
class SELocalFilesystemHttpServer;
class SENativeObsUIControlsManager;
class SEProfilesManager;
class SEBackupManager;
class SERazerWyvrnManager;
class SECleanupManager;
class SEPreviewManager;
class ISEApiMessageSink;
class SEHostEventDispatcher;
class SEMessageBus;
class SESharedVideoCompositionManager;
class SEVideoCompositionManager;
class SEAudioCompositionManager;
class SEOutputManager;
class SEObsActiveSceneTracker;
class SEStatePersistence;
class SEStatusBarMessageManager;
class ISEPopupWindowManager;
class SEBrowserDialogManager;
class SESessionManager;
class SEAuxiliaryControlsBuilder;
class SEConfig;
class SEPleaseWaitWindow;
class SEObsApiTransactionManager;
class SEShellServices;
class SEHostStreamingManager;
class SEHostUpdateManager;
class SEHostInfoProvider;

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
struct QCefCookieManager;

struct SEApiServices {
	//
	// False until Initialize() has finished, and false again from the
	// moment Shutdown() begins. The one non-pointer member, and the
	// question an API message handler asks before it dispatches: a
	// browser source can connect to the websocket server while the
	// plug-in is still starting, and a handler that ran then would reach
	// into services that are constructed but not yet wired together.
	//
	// It replaces SEGlobalStateManager::IsInstanceAvailable(), which
	// asked exactly this -- "does the root exist and is it initialized?"
	// -- of global state (CORE-757). The "does it exist" half is the
	// weak_ptr lock that every holder already does.
	//
	bool ready = false;

	//
	// Borrowed, not owned: SEGlobalStateManager creates the browser panel
	// and deletes it in Shutdown(). It is in here because it is one of
	// the objects Initialize() creates, and because the things that need
	// it -- browser widgets -- are handed this bundle and not the panel.
	//
	QCef *cef = nullptr;

	//
	// Borrowed on the same terms, and for the one consumer that needs
	// it: a browser widget that is not incognito opens on the shared
	// cookie store. Both are nulled here before Shutdown() deletes
	// them.
	//
	QCefCookieManager *cefCookieManager = nullptr;

	std::shared_ptr<SEBrowserWidgetManager> widgetManager;
	std::shared_ptr<SEObsSceneManager> obsSceneManager;
	std::shared_ptr<SEMenuManager> menuManager;
	std::shared_ptr<ISEBandwidthTestManager> bandwidthTestManager;
	std::shared_ptr<SEOutputSettingsManager> outputSettingsManager;
	std::shared_ptr<ISEWorkerManager> workerManager;
	std::shared_ptr<ISEHotkeyManager> hotkeyManager;
	std::shared_ptr<SEPerformanceHistoryTracker> performanceHistoryTracker;
	std::shared_ptr<SEAnalyticsEventsManager> analyticsEventsManager;
	std::shared_ptr<ISEExternalSceneDataProviderManager>
		externalSceneDataProviderManager;
	std::shared_ptr<SEHttpClient> httpClient;
	std::shared_ptr<SELocalFilesystemHttpServer> localFilesystemHttpServer;
	std::shared_ptr<SENativeObsUIControlsManager> nativeObsUIControlsManager;
	std::shared_ptr<SEProfilesManager> profilesManager;
	std::shared_ptr<SEBackupManager> backupManager;
#ifdef SE_ENABLE_WYVRN
	std::shared_ptr<SERazerWyvrnManager> razerWyvrnManager;
#endif
	std::shared_ptr<SECleanupManager> cleanupManager;
	std::shared_ptr<SEPreviewManager> previewManager;
	//
	// The transport, as an interface. The root holds the concrete
	// SEWebsocketApiServer and hands that to the host event dispatcher,
	// which needs methods that are not on here; a consumer that only
	// answers a page gets this.
	//
	std::shared_ptr<ISEApiMessageSink> apiMessageSink;

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

	//
	// Opening something outside our own process: a URL in the default
	// browser, a path in the file manager.
	//
	std::shared_ptr<SEShellServices> shellServices;

	//
	// Three host-level concerns that had no owner at all: OBS's own
	// single stream, the release channel and the updater, and a
	// read-only description of the host and machine. Each was libobs
	// called straight from a handler body.
	//
	std::shared_ptr<SEHostStreamingManager> hostStreamingManager;
	std::shared_ptr<SEHostUpdateManager> hostUpdateManager;
	std::shared_ptr<SEHostInfoProvider> hostInfoProvider;
	std::shared_ptr<ISEPopupWindowManager> popupWindowManager;
	std::shared_ptr<SEBrowserDialogManager> browserDialogManager;
	std::shared_ptr<SESessionManager> sessionManager;

	//
	// Owns no widget and borrows no raw pointer, so unlike the four above
	// it its position here is not load-bearing.
	//
	std::shared_ptr<SEAuxiliaryControlsBuilder> auxiliaryControlsBuilder;

	//
	// The plug-in's own ini. First to be constructed and last to be
	// written: Shutdown() calls CloseAndPreventFurtherReopens() on it
	// near the end, after which it reads as a config full of defaults
	// rather than a crash.
	//
	std::shared_ptr<SEConfig> config;

	//
	// The modal "please wait" dialog, and the open-transaction registry
	// that raises it. Both were singletons reached from a file-static
	// map in SEUtils (CORE-757).
	//
	std::shared_ptr<SEPleaseWaitWindow> pleaseWaitWindow;
	std::shared_ptr<SEObsApiTransactionManager> obsApiTransactionManager;
};
