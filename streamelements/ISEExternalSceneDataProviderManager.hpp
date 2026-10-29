#pragma once

#include "cef-headers.hpp"

//
// What the host API needs from the external scene data providers: a
// read-only description of what each one can offer.
//
class ISEExternalSceneDataProviderManager {
public:
	virtual ~ISEExternalSceneDataProviderManager() {}

	virtual void SerializeProviders(CefRefPtr<CefValue> result) = 0;

	virtual bool
	SerializeProviderSceneCollections(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> result) = 0;

	//
	// "Colletion" is the spelling the implementation has always had, and
	// it is preserved rather than corrected: renaming a method is a
	// change of its own, not something to fold into a mechanical
	// interface extraction.
	//
	virtual bool
	SerializeProviderSceneColletion(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> result) = 0;
};
