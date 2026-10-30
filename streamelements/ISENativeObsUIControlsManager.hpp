#pragma once

#include "cef-headers.hpp"

//
// The preview frame and title bar are compiled out, and the switch
// lives here because three places are guarded on it: these four
// declarations, SENativeObsUIControlsManager's own, and the four
// handler bodies that call them. It was defined in the implementation
// header until this interface arrived, which the handler no longer
// includes -- and an undefined name in an #if evaluates to 0, so the
// handler's guard would have kept compiling them out by accident and
// gone on doing so if anyone ever set this to 1.
//
#define SE_ENABLE_CENTRAL_WIDGET_DECORATIONS 0

//
// What the host API needs from OBS's own UI controls: the preview frame and
// title bar, the start-streaming handover, and the manage-broadcast button's
// visibility.
//
// StartStreaming() does not start a stream. It drives OBS's native
// Start Streaming button through the handover protocol, which is why
// requestStreamingStart still points here rather than at
// SEHostStreamingManager -- that one owns obs_frontend_streaming_*, and this
// class asks it. The two Advise* methods are the page's half of the same
// protocol: it was asked whether a stream may start, and these are its
// answer.
//
// Off the contract, and the reason this class needed an interface at all:
// finding, clicking and styling the native button, its six visual states,
// the hotkey routing, the widget-visibility trackers and the
// acknowledge-timeout tracker -- 28 Qt headers' worth, almost none of it in
// a signature.
//
class ISENativeObsUIControlsManager {
public:
	virtual ~ISENativeObsUIControlsManager() {}

	//
	// The handover: a page registers itself as the handler for a start
	// request, then accepts or rejects each one.
	//
	virtual bool DeserializeStartStreamingUIHandlerProperties(
		CefRefPtr<CefValue> input) = 0;
	virtual void AdviseRequestStartStreamingAccepted() = 0;
	virtual void AdviseRequestStartStreamingRejected() = 0;

	//
	// Drives the native button, rather than calling libobs: OBS's own
	// confirmation and bandwidth-test paths have to run.
	//
	virtual void StartStreaming() = 0;

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	virtual bool DeserializePreviewTitleBar(CefRefPtr<CefValue> input) = 0;
	virtual void HidePreviewTitleBar() = 0;

	virtual bool DeserializePreviewFrame(CefRefPtr<CefValue> input) = 0;
	virtual void HidePreviewFrame() = 0;
#endif

	virtual bool GetNativeManageBroadcastButtonVisibility() = 0;

	//
	// Called by SESessionManager, not by a handler.
	//
	virtual void Reset() = 0;
};
