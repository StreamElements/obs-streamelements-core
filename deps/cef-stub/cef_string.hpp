//
//  cef_string.hpp
//  cef-drop-in-stub
//
//  Created by Ilya Melamed on 24/03/2022.
//

#pragma once

#ifndef cef_string_h
#define cef_string_h

#include <string>

class CefString {
public:
    CefString() {}
    CefString(const CefString& src): m_string(src.m_string) {}
    CefString(const std::string& src): m_string(src) {}
    CefString(const char* src): m_string(src) {}
    CefString(const std::wstring& src) {
        InitFromWString(src);
    }
    CefString(const wchar_t* src): CefString(std::wstring(src)) {}

    ~CefString() {}
    
    const char* c_str() const { return m_string.c_str(); }
    size_t length() const { return m_string.size(); }
    size_t size() const { return m_string.size(); }
    bool empty() const { return size() == 0; }
    
    int compare(const CefString& str) const {
        if (empty() && str.empty())
          return 0;
        if (empty())
          return -1;
        if (str.empty())
          return 1;
        return m_string.compare(str.m_string);
    }
    
    void clear() { m_string.clear(); }
    void swap(CefString& str) {
        std::string tmp = str.m_string;
        str.m_string = m_string;
        m_string = tmp;
    }
    
    bool IsOwner() const { return true; }

    std::string ToString() const { return m_string; }
    
    std::wstring ToWString() const;
    
    ///
    // Comparison operator overloads.
    ///
    bool operator<(const CefString& str) const { return (compare(str) < 0); }
    bool operator<=(const CefString& str) const {
      return (compare(str) <= 0);
    }
    bool operator>(const CefString& str) const { return (compare(str) > 0); }
    bool operator>=(const CefString& str) const {
      return (compare(str) >= 0);
    }
    bool operator==(const CefString& str) const {
      return (compare(str) == 0);
    }
    bool operator!=(const CefString& str) const {
      return (compare(str) != 0);
    }

    ///
    // Assignment operator overloads.
    ///
    CefString& operator=(const CefString& str) {
        m_string = str.m_string;
        return *this;
    }
    operator std::string() const { return ToString(); }
    CefString& operator=(const std::string& str) {
        m_string = str;
        return *this;
    }
    CefString& operator=(const char* str) {
        m_string = str;
        return *this;
    }
    operator std::wstring() const { return ToWString(); }
    CefString& operator=(const std::wstring& str) {
        InitFromWString(str);
        return *this;
    }
    CefString& operator=(const wchar_t* str) {
        InitFromWString(std::wstring(str));
        return *this;
    }

    
private:
    std::string m_string;

    void InitFromWString(const std::wstring& src);
};

#endif /* cef_string_h */
