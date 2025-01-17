#pragma once

#include "obs.h"
#include "obs.hpp"
#include "obs-frontend-api.h"

#include "StreamElementsUtils.hpp"

class StreamElementsObsActiveSceneTracker
{
public:
	StreamElementsObsActiveSceneTracker();
	~StreamElementsObsActiveSceneTracker();

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);

	static void handle_transition_start(void *my_data, calldata_t *cd);

	void ClearTransition();
	void UpdateTransition();

private:
	obs_source_t* m_currentTransition = nullptr;
};
