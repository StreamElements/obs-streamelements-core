#pragma once

#include <obs.h>
#include "../cef-headers.hpp"

class SEOutputSettingsManager
{
public:
	SEOutputSettingsManager();
	virtual ~SEOutputSettingsManager();

public:
	//
	// All of them, and the two kind-restricted views of the same walk.
	//
	// It was one method taking an obs_encoder_type*, null meaning "all",
	// which put a libobs type in the only signature the host API reaches
	// it through -- and made the two kinds a local in a handler body.
	// The vocabulary is on the implementation side now.
	//
	void GetAvailableEncoders(CefRefPtr<CefValue> &result);
	void GetAvailableVideoEncoders(CefRefPtr<CefValue> &result);
	void GetAvailableAudioEncoders(CefRefPtr<CefValue> &result);

	bool SetStreamingSettings(CefRefPtr<CefValue> input);

	bool SetEncodingSettings(CefRefPtr<CefValue> input);
	bool GetEncodingSettings(CefRefPtr<CefValue>& output);

	void GetEncoderProperties(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output);

private:
	void GetAvailableEncodersOfType(CefRefPtr<CefValue> &result,
					obs_encoder_type *encoder_type);

	void StopAllOutputs();
};
