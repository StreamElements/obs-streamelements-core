#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the host streaming manager: OBS's own
// single native stream, read and stopped.
//
// Starting it is deliberately absent. requestStreamingStart goes to
// SENativeObsUIControlsManager instead, because starting a stream here
// *is* the native button handover -- OBS's own confirmation and
// bandwidth-test paths have to run. RequestStart() exists on the
// concrete class for that one caller and has no business on an API
// contract.
//
// Not to be confused with ISEOutputManager, which owns SE.Live's own
// outputs.
//
class ISEHostStreamingManager {
public:
	virtual ~ISEHostStreamingManager() {}

	//
	// Stops the native stream, through OBS's frontend API.
	//
	virtual void RequestStop() = 0;

	//
	// Whether the native stream is currently active.
	//
	virtual void SerializeStreamingStatus(CefRefPtr<CefValue> &output) = 0;
};
