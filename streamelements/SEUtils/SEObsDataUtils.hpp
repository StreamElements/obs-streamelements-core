#pragma once

//
// Serialization between OBS sources, properties and data objects and the
// local CefValue model. Needs libobs.
//

#include <cef-headers.hpp>

#include <obs.h>

#include <string>
#include <vector>
#include <cstdint>

CefRefPtr<CefListValue> SerializeObsSourceFilters(obs_source_t *source,
						  bool serializeProperties);
bool DeserializeObsSourceFilters(obs_source_t *source,
				 CefRefPtr<CefValue> filtersValue);

void SerializeObsSource(obs_source_t *source, CefRefPtr<CefDictionaryValue> dic,
			bool isExistingSource, bool serializeProperties);

bool SerializeObsProperty(obs_property_t *prop, CefRefPtr<CefValue> &output);
bool SerializeObsProperties(obs_properties_t *props,
			    CefRefPtr<CefValue> &output);

CefRefPtr<CefValue> SerializeObsData(obs_data_t *data);
bool DeserializeObsData(CefRefPtr<CefValue> input, obs_data_t *data);
uint32_t GetInt32FromAlignmentId(std::string alignment);
std::string GetAlignmentIdFromInt32(uint32_t a);
