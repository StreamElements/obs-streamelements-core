//
//  cef_process_message.hpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 15/06/2022.
//

#pragma once

#ifndef cef_process_message_h
#define cef_process_message_h

#include <memory>
#include <vector>

#include "cef_value.hpp"

#define CefRefPtr std::shared_ptr

class CefProcessMessage {
public:
	static CefRefPtr<CefProcessMessage> Create(CefString name);

	virtual CefRefPtr<CefProcessMessage> Copy() = 0;
	virtual CefRefPtr<CefListValue> GetArgumentList() = 0;
	virtual CefString GetName() = 0;
	virtual bool IsReadOnly() = 0;
	virtual bool IsValid() = 0;
};

#endif /* cef_procesS_message_h */
