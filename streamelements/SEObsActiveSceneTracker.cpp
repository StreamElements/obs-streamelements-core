#include "SEObsActiveSceneTracker.hpp"
#include "SEMessageBus.hpp"
#include "SEGlobalStateManager.hpp"

#include "callback/calldata.h"

static void dispatch_event(std::string name, std::string args)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", name, args);

	std::string externalEventName =
		name.c_str() + 4; /* remove 'host' prefix */
	externalEventName[0] =
		tolower(externalEventName[0]); /* lower case first letter */

	auto msgBus = SEMessageBus::GetInstance();

	if (!msgBus)
		return;

	msgBus->NotifyAllExternalEventListeners(
		SEMessageBus::DEST_ALL_EXTERNAL,
		SEMessageBus::SOURCE_APPLICATION, "OBS",
		externalEventName,
		CefParseJSON(args, JSON_PARSER_ALLOW_TRAILING_COMMAS));
}

SEObsActiveSceneTracker::SEObsActiveSceneTracker()
{
	UpdateTransition();

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);
}

SEObsActiveSceneTracker::~SEObsActiveSceneTracker()
{
	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	ClearTransition();
}

void SEObsActiveSceneTracker::handle_obs_frontend_event(
	enum obs_frontend_event event,
	void* data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self =
		static_cast<SEObsActiveSceneTracker *>(data);

	switch (event)
	{
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED:
	case OBS_FRONTEND_EVENT_TRANSITION_DURATION_CHANGED:
		self->UpdateTransition();
		break;

	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		self->UpdateTransition();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		self->ClearTransition();
		break;
			
	default:
		break;
	}
}

void SEObsActiveSceneTracker::ClearTransition()
{
	if (m_currentTransition) {
		auto handler =
			obs_source_get_signal_handler(m_currentTransition);

		signal_handler_disconnect(handler, "transition_start",
					  handle_transition_start, this);

		signal_handler_disconnect(handler, "update",
					  handle_transition_changed, this);

		signal_handler_disconnect(handler, "update_properties",
					  handle_transition_changed, this);

		signal_handler_disconnect(handler, "rename",
					  handle_transition_changed, this);

		obs_source_release(SETRACE_DECREF(m_currentTransition));

		m_currentTransition = nullptr;
	}
}

void SEObsActiveSceneTracker::UpdateTransition()
{
	ClearTransition();

	m_currentTransition =
		SETRACE_ADDREF(obs_frontend_get_current_transition());

	if (!m_currentTransition)
		return;

	auto handler = obs_source_get_signal_handler(m_currentTransition);

	signal_handler_connect(handler, "transition_start",
			       handle_transition_start, this);

	signal_handler_connect(handler, "update", handle_transition_changed,
			       this);

	signal_handler_connect(handler, "update_properties",
			       handle_transition_changed, this);

	signal_handler_connect(handler, "rename", handle_transition_changed,
			       this);

	DispatchTransitionChanged();
}

void SEObsActiveSceneTracker::DispatchTransitionChanged()
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto videoCompositionManager =
		SEGlobalStateManager::GetInstance()
			->GetVideoCompositionManager();

	if (!videoCompositionManager)
		return;

	auto videoComposition =
		videoCompositionManager->GetObsNativeVideoComposition();

	if (!videoComposition)
		return;

	json11::Json json = json11::Json::object{
		{"videoCompositionId", videoComposition->GetId()},
	};

	std::string name = "hostActiveTransitionChanged";
	std::string args = json.dump();

	dispatch_event(name, args);

	CefRefPtr<CefValue> compositionJsonValue = CefValue::Create();

	videoComposition->SerializeComposition(compositionJsonValue);

	dispatch_event("hostVideoCompositionChanged",
		       CefWriteJSON(compositionJsonValue, JSON_WRITER_DEFAULT));
}

void SEObsActiveSceneTracker::handle_transition_start(
	void* my_data, calldata_t* cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = static_cast<SEObsActiveSceneTracker *>(my_data);
	OBSSourceAutoRelease destSource = nullptr;

	//
	// In case you are wondering what is going on here, here's a discussion around transitions and studio mode
	//
	// https://obsproject.com/forum/threads/transition-starting-event.81769/
	//

	if (obs_frontend_preview_program_mode_active()) {
		destSource = SETRACE_SCOPEREF(obs_frontend_get_current_scene());
	} else {
		obs_source_t *source =
			static_cast<obs_source_t *>(calldata_ptr(cd, "source"));

		if (!source)
			return;

		destSource = SETRACE_SCOPEREF(obs_transition_get_source(
			source, OBS_TRANSITION_SOURCE_B));

		//if (!destSource)
		//	destSource = obs_transition_get_source(
		//		source, OBS_TRANSITION_SOURCE_A);
	}

	if (!destSource)
		return;

	const char *sourceName = obs_source_get_name(destSource);

	if (sourceName) {
		json11::Json json = json11::Json::object{
			{"sceneId", GetIdFromPointer(destSource.Get())},
			{"videoCompositionId",
			 SEGlobalStateManager::GetInstance()
				 ->GetVideoCompositionManager()
				 ->GetObsNativeVideoComposition()
				 ->GetId()},
			{"name", sourceName},
			{"width", (int)obs_source_get_width(destSource)},
			{"height", (int)obs_source_get_height(destSource)}};

		std::string name = "hostActiveSceneChanging";
		std::string args = json.dump();

		dispatch_event(name, args);
	}
}

void SEObsActiveSceneTracker::handle_transition_changed(void* my_data, calldata_t* cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	SEObsActiveSceneTracker *self =
		static_cast<SEObsActiveSceneTracker *>(my_data);

	self->DispatchTransitionChanged();

}
