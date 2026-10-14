#pragma once

//
// Comparison over the local CefValue model. Pure: the standard library and
// cef-stub, nothing else.
//

#include <cef-headers.hpp>

bool IsCefValueEqual(CefRefPtr<CefValue> a, CefRefPtr<CefValue> b);
