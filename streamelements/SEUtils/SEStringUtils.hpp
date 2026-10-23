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
#include <vector>
#include <cstdarg>
#include <cstdio>

std::string wstring_to_utf8(const std::wstring wstr);
std::wstring utf8_to_wstring(const std::string str);

//
// Declared rather than left file-static, because two domains use it: the
// system identity id and the temporary file name builder.
//
std::string clean_guid_string(std::string input);

//
// The template parameter pack is not used and is not a mistake worth
// correcting: it is what makes this definition a template, and therefore
// legal in a header included by 50-odd translation units. Call sites never
// name it.
//
// The grow-and-retry path used to be undefined behaviour. vsnprintf consumes
// the va_list it walks, and the retry handed the same one to a second call --
// which also still passed the original 512 rather than the size it had just
// been told it needed. So an output of 512 bytes or more read a consumed list
// and then truncated to 512 anyway. Every call in the plug-in happens to be
// shorter than that, which is why it never showed; SEVideoComposition builds
// encoder names through it and a long enough composition id would have got
// there.
//
// Now the list is copied before the first call, so the retry has an
// untouched one, and the buffer is sized from what vsnprintf asked for. A
// negative return -- an encoding error -- gives an empty string rather than
// reading whatever is in the buffer.
//
template<typename... Args> std::string FormatString(const char *format, ...)
{
	va_list vl;
	va_start(vl, format);

	va_list retry;
	va_copy(retry, vl);

	std::vector<char> buffer(512);

	int nsize = vsnprintf(buffer.data(), buffer.size(), format, vl);

	va_end(vl);

	if (nsize >= (int)buffer.size()) {
		buffer.resize((size_t)nsize + 1); // +1 for the terminator

		nsize = vsnprintf(buffer.data(), buffer.size(), format, retry);
	}

	va_end(retry);

	if (nsize < 0)
		return std::string();

	return std::string(buffer.data(), (size_t)nsize);
}

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result);

//
// Declared here, defined in SESystemInfoUtils.cpp beside its platform pair:
// it picks between CoCreateGuid and libuuid, and that file is where the
// Windows and macOS halves of the system-identity helpers live.
//
std::string CreateGloballyUniqueIdString();

std::string GetIdFromPointer(const void *ptr);
const void *GetPointerFromId(const char *id);
