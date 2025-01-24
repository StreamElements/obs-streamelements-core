#include "StreamElementsObsActiveSceneTracker.hpp"
#include "StreamElementsMessageBus.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include "callback/calldata.h"

static void dispatch_event(std::string name, std::string args)
{
	if (!StreamElementsGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", name, args);

	std::string externalEventName =
		name.c_str() + 4; /* remove 'host' prefix */
	externalEventName[0] =
		tolower(externalEventName[0]); /* lower case first letter */

	auto msgBus = StreamElementsMessageBus::GetInstance();

	if (!msgBus)
		return;

	msgBus->NotifyAllExternalEventListeners(
		StreamElementsMessageBus::DEST_ALL_EXTERNAL,
		StreamElementsMessageBus::SOURCE_APPLICATION, "OBS",
		externalEventName,
		CefParseJSON(args, JSON_PARSER_ALLOW_TRAILING_COMMAS));
}

StreamElementsObsActiveSceneTracker::StreamElementsObsActiveSceneTracker()
{
	UpdateTransition();

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);
}

StreamElementsObsActiveSceneTracker::~StreamElementsObsActiveSceneTracker()
{
	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	ClearTransition();
}

void StreamElementsObsActiveSceneTracker::handle_obs_frontend_event(
	enum obs_frontend_event event,
	void* data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self =
		static_cast<StreamElementsObsActiveSceneTracker *>(data);

	switch (event)
	{
	case OBS_FRONTEND_EVENT_TRANSITION_CHANGED:
		self->UpdateTransition();
		break;

	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED:
		self->UpdateTransition();
		break;

	case OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP:
		self->ClearTransition();
		break;
	}
}

void StreamElementsObsActiveSceneTracker::ClearTransition()
{
	if (m_currentTransition) {
		auto handler =
			obs_source_get_signal_handler(m_currentTransition);

		signal_handler_disconnect(handler, "transition_start",
					  handle_transition_start, this);

		obs_source_release(m_currentTransition);

		m_currentTransition = nullptr;
	}
}

void StreamElementsObsActiveSceneTracker::UpdateTransition()
{
	ClearTransition();

	m_currentTransition = obs_frontend_get_current_transition();

	if (!m_currentTransition)
		return;

	auto handler = obs_source_get_signal_handler(m_currentTransition);

	signal_handler_connect(handler, "transition_start",
			       handle_transition_start, this);
}

void StreamElementsObsActiveSceneTracker::handle_transition_start(
	void* my_data, calldata_t* cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = static_cast<StreamElementsObsActiveSceneTracker *>(my_data);
	OBSSourceAutoRelease destSource = nullptr;

	//
	// In case you are wondering what is going on here, here's a discussion around transitions and studio mode
	//
	// https://obsproject.com/forum/threads/transition-starting-event.81769/
	//

	if (obs_frontend_preview_program_mode_active()) {
		destSource = obs_frontend_get_current_scene();
	} else {
		obs_source_t *source =
			static_cast<obs_source_t *>(calldata_ptr(cd, "source"));

		if (!source)
			return;

		destSource = obs_transition_get_source(source,
						       OBS_TRANSITION_SOURCE_B);

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
			 StreamElementsGlobalStateManager::GetInstance()
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
