#pragma once

#include "cef-headers.hpp"

//
// The last thing standing between SEApiMessageHandler.cpp and a closure
// with no obs and no Qt in it, and the only one of part 2's contracts that
// is not over a service.
//
// Three handler bodies -- removeVideoCompositionViewOverlay,
// getVideoCompositionViewOverlayProperties and
// setVideoCompositionViewOverlayProperties -- reach the handler's own
// browser widget through SEApiMessageHandlerBase::GetBrowserWidget() and
// call these three methods on it. Nothing SEApiServices carries is
// involved, which is why thirteen service interfaces did not reach it:
// SEBrowserWidget.hpp was arriving through SEBrowserWidgetManager.hpp and
// was the real source of all 7 obs and 22 Qt headers the whole time.
//
// SEApiMessageHandlerBase holds the widget as this type rather than as
// SEBrowserWidget *, and that placement is forced: the base header has only
// a forward declaration, and an upcast needs the complete type. The two
// SetBrowserWidget() calls are both inside SEBrowserWidget's own
// constructor and destructor, where it is in scope, so the conversion
// happens at the one place in the program that can perform it.
//
// What stays on SEBrowserWidget is the rest of a QWidget's life, including
// SetVideoCompositionView -- which takes a shared_ptr<SEVideoCompositionBase>
// and a QRect, is called by the widget manager rather than by a handler,
// and is the reason that class still needs the concrete composition type.
//
class ISEVideoCompositionViewHost {
public:
	virtual ~ISEVideoCompositionViewHost() {}

public:
	//
	// Tears the composition view down. Safe when there is none.
	//
	virtual void RemoveVideoCompositionView() = 0;

	virtual void
	SerializeVideoCompositionView(CefRefPtr<CefValue> &output) = 0;

	virtual void
	DeserializeVideoCompositionView(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) = 0;
};
