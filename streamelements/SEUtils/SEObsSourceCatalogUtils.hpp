#pragma once

//
// The source catalog: what OBS can create, and what it already has.
//
// This enumerates source *types* and live sources. A scene consumes both
// but owns neither, and the same enumeration is wanted in places that have
// nothing to do with scenes, so it is neither SEObsSceneUtils' concern nor
// SEObsDataUtils' -- that one serializes data, settings and properties.
//
// Deliberately obs-free, headers included. There is one function per shape
// the host API asks for, and the OBS_SOURCE_* vocabulary stays on the other
// side of this header: no caller names a flag mask or a source type. That
// is what lets SEApiMessageHandler.cpp compile without libobs, because a
// free function is a link seam -- a test includes this header, links its
// own definitions and never needs obs.h (CORE-757).
//

#include <cef-headers.hpp>

//
// What OBS can create. A type is listed when it can produce any of the
// kinds of output asked for.
//
void SerializeAvailableInputSourceTypes(CefRefPtr<CefValue> &output);
void SerializeAvailableVideoInputSourceTypes(CefRefPtr<CefValue> &output);
void SerializeAvailableAudioInputSourceTypes(CefRefPtr<CefValue> &output);

void SerializeAvailableFilterSourceTypes(CefRefPtr<CefValue> &output);
void SerializeAvailableVideoFilterSourceTypes(CefRefPtr<CefValue> &output);
void SerializeAvailableAudioFilterSourceTypes(CefRefPtr<CefValue> &output);

//
// What exists in this OBS session already, scenes included.
//
void SerializeExistingInputSources(CefRefPtr<CefValue> &output);
void SerializeExistingVideoInputSources(CefRefPtr<CefValue> &output);
void SerializeExistingAudioInputSources(CefRefPtr<CefValue> &output);

//
// One source type's properties, from a {class, settings} dictionary. It
// creates a private source, reads its property list, and throws the source
// away.
//
void SerializeObsSourceProperties(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output);

//
// Every obs module loaded into this process.
//
void SerializeLoadedObsModules(CefRefPtr<CefValue> &output);
