#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the audio composition manager: the four
// methods behind the audio composition handlers.
//
// Everything else the concrete class does is either the root's or its own:
// the encoder-class and transition-class enumerations the handlers call go
// through the video manager instead, GetObsNativeAudioComposition and the
// two GetAudioCompositionById overloads hand out
// shared_ptr<SEAudioCompositionBase> -- an obs-flavoured type no handler
// body names -- and Reset() has no caller outside the root here, unlike on
// its video and shared-video counterparts.
//
class ISEAudioCompositionManager {
public:
	virtual ~ISEAudioCompositionManager() {}

	virtual void SerializeAllCompositions(CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeComposition(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeExistingCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output) = 0;
};
