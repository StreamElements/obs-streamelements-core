//
//  cef_string.cpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#include "cef_string.hpp"

#include "../utf8.h"

std::wstring CefString::ToWString() const {
    std::wstring wide_line;

    if (m_string.empty()) return wide_line;

#ifdef _MSC_VER
    utf8::utf8to16(m_string.begin(), m_string.end(), std::back_inserter(wide_line));
#else
    utf8::utf8to32(m_string.begin(), m_string.end(), std::back_inserter(wide_line));
#endif
    return wide_line;
}

void CefString::InitFromWString(const std::wstring& src) {
    m_string.clear();

    if (src.empty()) return;

#ifdef _MSC_VER
    utf8::utf16to8(src.begin(), src.end(), std::back_inserter(m_string));
#else
    utf8::utf32to8(src.begin(), src.end(), std::back_inserter(m_string));
#endif
}
