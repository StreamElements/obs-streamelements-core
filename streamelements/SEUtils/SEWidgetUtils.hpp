#pragma once

//
// Qt desktop integration that does not belong to any one widget.
//
// This file used to hold the menu, action and control-widget deserializers
// as well, which were the last six SEGlobalStateManager::GetInstance()
// reaches under SEUtils/. They are SEAuxiliaryControlsBuilder now: a
// service, injected into the four places that build auxiliary controls.
//

#include <cef-headers.hpp>

void DeserializeRevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);
