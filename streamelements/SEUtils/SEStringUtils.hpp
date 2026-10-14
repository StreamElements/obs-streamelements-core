#pragma once

//
// Pure string, encoding and identifier helpers. Nothing here touches libobs,
// Qt, CEF or a singleton, which is the point: this header can be included on
// its own, where SEUtils.hpp cannot -- that one drags in obs.h, obs-module.h,
// curl and eight Qt headers, so no test can include it at all.
//
// Split out of SEUtils.hpp, which now includes this file, so every one of the
// 52 existing callers is unaffected.
//

#include <string>
#include <map>
#include <cstdarg>
#include <cstdio>

std::string wstring_to_utf8(const std::wstring wstr);
std::wstring utf8_to_wstring(const std::string str);

//
// Declared rather than left file-static, because two domains use it: the
// system identity id and the temporary file name builder.
//
std::string clean_guid_string(std::string input);

template<typename... Args> std::string FormatString(const char *format, ...)
{
	int size = 512;
	char *buffer = 0;
	buffer = new char[size];
	va_list vl;
	va_start(vl, format);
	int nsize = vsnprintf(buffer, size, format, vl);
	if (size <= nsize) { //fail delete buffer and try again
		delete[] buffer;
		buffer = 0;
		buffer = new char[nsize + 1]; //+1 for /0
		nsize = vsnprintf(buffer, size, format, vl);
	}
	std::string ret(buffer);
	va_end(vl);
	delete[] buffer;
	return ret;
}

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result);

std::string GetIdFromPointer(const void *ptr);
const void *GetPointerFromId(const char *id);
