#pragma once

#include "cef-headers.hpp"

class StreamElementsSharedVideoCompositionManager {
public:
	StreamElementsSharedVideoCompositionManager() {}
	~StreamElementsSharedVideoCompositionManager() {}

	void DeserializeSharedVideoComposition(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	void SerializeAllSharedVideoCompositions(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output);

	void RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);
};
