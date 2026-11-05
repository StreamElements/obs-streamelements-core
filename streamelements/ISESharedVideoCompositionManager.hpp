#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the shared video composition manager: OBS
// canvases shared with the host, and the edges between them and our own
// video compositions.
//
// The concrete class is also an SEVideoCompositionEventListener and keeps an
// obs_frontend_event callback, neither of which a consumer of the services
// container touches.
//
class ISESharedVideoCompositionManager {
public:
	virtual ~ISESharedVideoCompositionManager() {}

	virtual void
	SerializeAllSharedVideoCompositions(CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeSharedVideoComposition(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) = 0;

	virtual void DeserializeSharedVideoCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void
	RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) = 0;

	virtual void ConnectVideoCompositionToSharedVideoComposition(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	virtual void
	DisconnectVideoCompositionsFromSharedVideoCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) = 0;

	//
	// Called by SESessionManager, not by a handler: signing out drops every
	// shared composition. On the contract for the usual reason -- what
	// stays off it is what only the root calls, and this fails that test.
	//
	virtual void Reset() = 0;
};
