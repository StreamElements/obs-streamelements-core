#pragma once

#include "cef-headers.hpp"

//
// The second level of the interface graph, and the only one: four methods a
// handler body calls on a composition it was handed by the composition
// manager.
//
// The audio side has none at all. Its manager's three getters hand out
// shared_ptr<SEAudioCompositionBase> and nothing outside the concrete
// classes ever calls a method on one -- measured, not assumed, which is why
// ISEAudioCompositionManager carries no getter.
//
// What stays on SEVideoCompositionBase is everything that speaks obs:
// scenes_t, GetCurrentSceneRef, AddScene, SetCurrentScene, the scene-item
// lookups, GetTransitionRef. None of it is named in a handler body.
//
class ISEVideoComposition {
public:
	virtual ~ISEVideoComposition() {}

	//
	// Writes a PNG to the screenshot path OBS is configured with. Named
	// for what the page asked for; it is a request, with no result.
	//
	virtual void TakeScreenshot() = 0;

	virtual void SerializeTransition(CefRefPtr<CefValue> &output) = 0;
	virtual void DeserializeTransition(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) = 0;

	//
	// Opens OBS's own properties dialog for the current transition.
	// False when there is no transition to open one for.
	//
	virtual bool ShowTransitionPropertiesDialog() = 0;
};
