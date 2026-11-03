#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the output settings manager: the encoder
// catalogue, and the encoding and streaming settings.
//
// The three encoder getters are a kind-split of one method that used to
// take an obs_encoder_type. That split is what kept an obs type out of
// this header, and so it is the reason this interface can exist at all.
//
// Worth knowing while reading the implementation: the `if
// (!input.get())` guard in the two setters is a null check, not a type
// check. A page's "x" arrives as a perfectly non-null VTYPE_STRING,
// which is why both now test the type as well.
//
class ISEOutputSettingsManager {
public:
	virtual ~ISEOutputSettingsManager() {}

	//
	// Every available encoder class, video and audio together.
	//
	virtual void GetAvailableEncoders(CefRefPtr<CefValue> &result) = 0;

	//
	// Video encoder classes only.
	//
	virtual void GetAvailableVideoEncoders(CefRefPtr<CefValue> &result) = 0;

	//
	// Audio encoder classes only.
	//
	virtual void GetAvailableAudioEncoders(CefRefPtr<CefValue> &result) = 0;

	//
	// The settable properties of one encoder class.
	//
	virtual void GetEncoderProperties(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) = 0;

	//
	// The current encoding settings.
	//
	virtual bool GetEncodingSettings(CefRefPtr<CefValue> &output) = 0;

	//
	// Writes them.
	//
	virtual bool SetEncodingSettings(CefRefPtr<CefValue> input) = 0;

	//
	// Writes the streaming server URL and key.
	//
	virtual bool SetStreamingSettings(CefRefPtr<CefValue> input) = 0;
};
