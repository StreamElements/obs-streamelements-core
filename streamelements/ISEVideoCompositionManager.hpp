#pragma once

#include "cef-headers.hpp"

#include "ISEVideoComposition.hpp"

#include <memory>
#include <string>

//
// What the host API needs from the video composition manager: the
// compositions themselves, the encoder and transition class catalogues, and
// a handle on one composition.
//
// The three getters are named differently from the concrete class's, and
// that is forced rather than chosen. SEVideoCompositionManager's
// GetVideoCompositionById and GetObsNativeVideoComposition return
// shared_ptr<SEVideoCompositionBase>, which eleven call sites in six classes
// need as the concrete type -- SEOutput, SEObsSceneManager, SEOutputManager,
// SESharedVideoCompositionManager, SEBrowserWidget (which passes it straight
// into SetVideoCompositionView) and SEVideoComposition itself. shared_ptr
// has no return-type covariance, so an override cannot narrow the return to
// the interface, and those eleven cannot move. The implementation therefore
// keeps its own getters and adds these three as upcasting wrappers over
// them.
//
// The new names are also the better ones. "ById" was wrong of the CefValue
// overload: it reads an id out of a dictionary if there is one there and
// answers the OBS-native composition if there is not, which is "the
// composition this request refers to", not a lookup.
//
class ISEVideoCompositionManager {
public:
	virtual ~ISEVideoCompositionManager() {}

public:
	virtual void SerializeAllCompositions(CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeComposition(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeExistingCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output) = 0;

public:
	//
	// Split by kind so that no handler body names OBS_ENCODER_VIDEO or
	// OBS_ENCODER_AUDIO; the obs-typed pair that does the work stays on
	// the implementation.
	//
	virtual void
	SerializeAvailableVideoEncoderClasses(CefRefPtr<CefValue> &output) = 0;
	virtual void
	SerializeAvailableAudioEncoderClasses(CefRefPtr<CefValue> &output) = 0;

	virtual void SerializeAvailableVideoEncoderClassProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;
	virtual void SerializeAvailableAudioEncoderClassProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void
	SerializeAvailableTransitionClasses(CefRefPtr<CefValue> &output) = 0;

public:
	//
	// The composition a request refers to: the id in the dictionary if
	// there is one, otherwise the OBS-native composition. Null when the
	// dictionary names an id that no longer exists.
	//
	virtual std::shared_ptr<ISEVideoComposition>
	GetVideoComposition(CefRefPtr<CefValue> input) = 0;

	//
	// By id, and null when there is no such composition.
	//
	virtual std::shared_ptr<ISEVideoComposition>
	GetVideoComposition(std::string id) = 0;

	virtual std::shared_ptr<ISEVideoComposition>
	GetNativeVideoComposition() = 0;

public:
	//
	// Called by SESessionManager, not by a handler.
	//
	virtual void Reset() = 0;
};
