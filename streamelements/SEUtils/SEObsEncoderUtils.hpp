#pragma once

//
// OBS transitions and encoders: serialization, lookup of an existing
// transition, and construction of video and audio encoders from a
// description.
//

#include <cef-headers.hpp>

#include <obs.h>

#include <string>

CefRefPtr<CefValue>
SerializeObsEncoderProperties(std::string id, obs_data_t *settings = nullptr);
void SerializeObsTransition(std::string videoCompositionId, obs_source_t *t,
			    int durationMilliseconds,
			    CefRefPtr<CefValue> &output);

obs_source_t *GetExistingObsTransition(std::string lookupId);

bool DeserializeObsTransition(CefRefPtr<CefValue> input, obs_source_t **t,
			      int *durationMilliseconds, bool useExisting);
CefRefPtr<CefDictionaryValue> SerializeObsEncoder(obs_encoder_t *e);

obs_encoder_t *DeserializeObsVideoEncoder(CefRefPtr<CefValue> input);
obs_encoder_t *DeserializeObsAudioEncoder(CefRefPtr<CefValue> input,
					  int mixer_idx = -1);
