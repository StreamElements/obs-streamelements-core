#pragma once

#include "obs.h"
#include "obs.hpp"
#include "obs-frontend-api.h"

#include "SEUtils.hpp"

#include <memory>

//
// Forward declaration rather than an include: the real header drags windows.h
// in ahead of winsock2.h on these translation units.
//
class SEWebsocketApiServer;
class SEVideoCompositionManager;

class SEObsActiveSceneTracker
{
public:
	SEObsActiveSceneTracker(std::shared_ptr<SEWebsocketApiServer> apiServer,
				std::shared_ptr<SEVideoCompositionManager>
					videoCompositionManager);
	~SEObsActiveSceneTracker();

private:
	std::shared_ptr<SEWebsocketApiServer> m_apiServer;
	std::shared_ptr<SEVideoCompositionManager> m_videoCompositionManager;

	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);
	static void handle_transition_start(void *my_data, calldata_t *cd);
	static void handle_transition_changed(void *my_data, calldata_t *cd);

	void ClearTransition();
	void UpdateTransition();

	void DispatchTransitionChanged();

private:
	obs_source_t* m_currentTransition = nullptr;
};
