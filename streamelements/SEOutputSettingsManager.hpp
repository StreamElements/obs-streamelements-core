#pragma once

#include "ISEOutputSettingsManager.hpp"

#include "../cef-headers.hpp"

class SEOutputSettingsManager : public ISEOutputSettingsManager {
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
	void GetAvailableEncoders(CefRefPtr<CefValue> &result) override;
	void GetAvailableVideoEncoders(CefRefPtr<CefValue> &result) override;
	void GetAvailableAudioEncoders(CefRefPtr<CefValue> &result) override;

	bool SetStreamingSettings(CefRefPtr<CefValue> input) override;

	bool SetEncodingSettings(CefRefPtr<CefValue> input) override;
	bool GetEncodingSettings(CefRefPtr<CefValue> &output) override;

	void GetEncoderProperties(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output) override;

private:
	void StopAllOutputs();
};
