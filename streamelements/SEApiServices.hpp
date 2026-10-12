#pragma once

#include <memory>
#include <shared_mutex>

class SEGlobalStateManager;

class SEObsSceneManager;
class SEMenuManager;
class SEOutputManager;
class SEVideoCompositionManager;
class SEAudioCompositionManager;
class SESharedVideoCompositionManager;
class SEOutputSettingsManager;
class SEHotkeyManager;
class SEBackupManager;
class SEWebsocketApiServer;
class SEBandwidthTestManager;
class SEExternalSceneDataProviderManager;
class SEProfilesManager;
class SEAnalyticsEventsManager;
class SERazerWyvrnManager;
class SEHttpClient;

//
// The services the host API surface needs.
//
// SEApiMessageHandler registers around two hundred methods spanning a dozen
// subsystems, so taking them one at a time would mean a constructor nobody
// can read. This is that constructor, named. It exists for the API handlers
// and the widgets that own them; everything else takes the one or two
// services it actually uses.
//
// SEGlobalStateManager::Initialize() creates the bundle before any of the
// services, because the widget manager is built long before the compositions
// are, and publishes each one into it as it comes up. Readers run on the
// websocket thread, so the slots are guarded. Every getter can therefore
// return null, and a handler whose service is missing skips registering the
// methods that use it rather than registering one that fails when called.
//
// The slots own what they hold, so a service cannot be destroyed underneath
// a page that is still calling it, and it lives for exactly as long as
// something can still call it.
//
// Three services are deliberately absent for that same reason, and adding
// them here would be a mistake: the browser widget manager, the worker
// manager and the native controls manager each own a widget, every widget
// owns a handler, and every handler owns this bundle. Owning them from here
// closes a ring that refcounting never breaks -- and it does not read as a
// leak when it happens. The widgets outlive OBS's own teardown, the Qt event
// loop never returns, and it looks exactly like a hang in our shutdown path.
//
// A handler that needs one of those three asks the host for it, which is
// where they are owned.
//
class SEApiServices {
public:
	//
	// The owning manager, borrowed and never owned: it owns, directly or
	// indirectly, everything that holds this bundle, so a shared_ptr here
	// would be a cycle.
	//
	// It is here for the calls that are operations on the host itself --
	// PersistState(), the dialog and user-interface-state entry points --
	// rather than calls on a service. Those want to become services of
	// their own; until they do, this is the honest name for the edge.
	//
	SEApiServices(SEGlobalStateManager *host) : m_host(host) {}

	SEGlobalStateManager *GetHost() const { return m_host; }

	std::shared_ptr<SEObsSceneManager> GetObsSceneManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_obsSceneManager;
	}

	void SetObsSceneManager(std::shared_ptr<SEObsSceneManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_obsSceneManager = value;
	}

	std::shared_ptr<SEMenuManager> GetMenuManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_menuManager;
	}

	void SetMenuManager(std::shared_ptr<SEMenuManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_menuManager = value;
	}

	std::shared_ptr<SEOutputManager> GetOutputManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_outputManager;
	}

	void SetOutputManager(std::shared_ptr<SEOutputManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_outputManager = value;
	}

	std::shared_ptr<SEVideoCompositionManager>
	GetVideoCompositionManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_videoCompositionManager;
	}

	void SetVideoCompositionManager(
		std::shared_ptr<SEVideoCompositionManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_videoCompositionManager = value;
	}

	std::shared_ptr<SEAudioCompositionManager>
	GetAudioCompositionManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_audioCompositionManager;
	}

	void SetAudioCompositionManager(
		std::shared_ptr<SEAudioCompositionManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_audioCompositionManager = value;
	}

	std::shared_ptr<SESharedVideoCompositionManager>
	GetSharedVideoCompositionManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_sharedVideoCompositionManager;
	}

	void SetSharedVideoCompositionManager(
		std::shared_ptr<SESharedVideoCompositionManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_sharedVideoCompositionManager = value;
	}

	std::shared_ptr<SEOutputSettingsManager>
	GetOutputSettingsManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_outputSettingsManager;
	}

	void
	SetOutputSettingsManager(std::shared_ptr<SEOutputSettingsManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_outputSettingsManager = value;
	}

	std::shared_ptr<SEHotkeyManager> GetHotkeyManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_hotkeyManager;
	}

	void SetHotkeyManager(std::shared_ptr<SEHotkeyManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_hotkeyManager = value;
	}

	std::shared_ptr<SEBackupManager> GetBackupManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_backupManager;
	}

	void SetBackupManager(std::shared_ptr<SEBackupManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_backupManager = value;
	}

	std::shared_ptr<SEWebsocketApiServer> GetWebsocketApiServer() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_websocketApiServer;
	}

	void SetWebsocketApiServer(std::shared_ptr<SEWebsocketApiServer> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_websocketApiServer = value;
	}

	std::shared_ptr<SEBandwidthTestManager> GetBandwidthTestManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_bandwidthTestManager;
	}

	void
	SetBandwidthTestManager(std::shared_ptr<SEBandwidthTestManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_bandwidthTestManager = value;
	}

	std::shared_ptr<SEExternalSceneDataProviderManager>
	GetExternalSceneDataProviderManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_externalSceneDataProviderManager;
	}

	void SetExternalSceneDataProviderManager(
		std::shared_ptr<SEExternalSceneDataProviderManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_externalSceneDataProviderManager = value;
	}

	std::shared_ptr<SEProfilesManager> GetProfilesManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_profilesManager;
	}

	void SetProfilesManager(std::shared_ptr<SEProfilesManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_profilesManager = value;
	}

	std::shared_ptr<SEAnalyticsEventsManager>
	GetAnalyticsEventsManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_analyticsEventsManager;
	}

	void SetAnalyticsEventsManager(
		std::shared_ptr<SEAnalyticsEventsManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_analyticsEventsManager = value;
	}

#ifdef SE_ENABLE_WYVRN
	std::shared_ptr<SERazerWyvrnManager> GetRazerWyvrnManager() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_razerWyvrnManager;
	}

	void SetRazerWyvrnManager(std::shared_ptr<SERazerWyvrnManager> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_razerWyvrnManager = value;
	}
#endif

	std::shared_ptr<SEHttpClient> GetHttpClient() const
	{
		std::shared_lock<std::shared_mutex> lock(m_mutex);

		return m_httpClient;
	}

	void SetHttpClient(std::shared_ptr<SEHttpClient> value)
	{
		std::unique_lock<std::shared_mutex> lock(m_mutex);

		m_httpClient = value;
	}

private:
	SEGlobalStateManager *m_host = nullptr;

	mutable std::shared_mutex m_mutex;

	std::shared_ptr<SEObsSceneManager> m_obsSceneManager;
	std::shared_ptr<SEMenuManager> m_menuManager;
	std::shared_ptr<SEOutputManager> m_outputManager;
	std::shared_ptr<SEVideoCompositionManager> m_videoCompositionManager;
	std::shared_ptr<SEAudioCompositionManager> m_audioCompositionManager;
	std::shared_ptr<SESharedVideoCompositionManager>
		m_sharedVideoCompositionManager;
	std::shared_ptr<SEOutputSettingsManager> m_outputSettingsManager;
	std::shared_ptr<SEHotkeyManager> m_hotkeyManager;
	std::shared_ptr<SEBackupManager> m_backupManager;
	std::shared_ptr<SEWebsocketApiServer> m_websocketApiServer;
	std::shared_ptr<SEBandwidthTestManager> m_bandwidthTestManager;
	std::shared_ptr<SEExternalSceneDataProviderManager>
		m_externalSceneDataProviderManager;
	std::shared_ptr<SEProfilesManager> m_profilesManager;
	std::shared_ptr<SEAnalyticsEventsManager> m_analyticsEventsManager;
#ifdef SE_ENABLE_WYVRN
	std::shared_ptr<SERazerWyvrnManager> m_razerWyvrnManager;
#endif
	std::shared_ptr<SEHttpClient> m_httpClient;
};
