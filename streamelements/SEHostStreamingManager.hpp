#pragma once

//
// OBS's own single stream: whether it is running, and asking it to start or
// stop.
//
// This is NOT what outputManager owns. That one manages SE.Live's own
// streaming, recording and replay-buffer outputs -- add, remove, enable,
// disable, seven methods across eighteen host API methods. This class is the
// host application's one native stream, the thing OBS's Start Streaming
// button controls. Worth saying out loud, because the names are close enough
// that merging them later would look like tidying.
//
// It exists so that exactly one class in the plug-in calls
// obs_frontend_streaming_*. Two handler bodies read and stopped it directly,
// and SENativeOBSControlsManager -- a Qt class about a button -- called the
// same three functions from five places.
//
// The dependency runs UI controls -> streaming, not the other way round: a
// UI class asking a service for the host's state reads correctly, where a
// service reaching into a UI class to start a stream does not.
//

#include "cef-headers.hpp"

class SEHostStreamingManager {
public:
	SEHostStreamingManager();
	~SEHostStreamingManager();

public:
	//
	// { isStreamingActive }. A dictionary rather than a bool because that
	// is the shape the host API method already answered with, and pages
	// read the key.
	//
	void SerializeStreamingStatus(CefRefPtr<CefValue> &output);

	//
	// Asks OBS to stop. Reached from a page.
	//
	void RequestStop();

public:
	//
	// For SENativeOBSControlsManager, which tracks the native button's
	// six states and drives the start-streaming handover. Not on the host
	// API surface: no handler calls either of these.
	//
	// Starting a stream from a page goes through requestStreamingStart,
	// which stays on the UI-controls manager, because starting one here
	// *is* clicking that button -- OBS's own confirmation and bandwidth
	// test hang off it.
	//
	bool IsStreaming();

	void RequestStart();
};
