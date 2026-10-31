#include "SEStringUtils.hpp"

#include "../deps/utf8.h"

#include <regex>
#include <iterator>

#ifndef WIN32
#define sprintf_s sprintf
#endif

std::string wstring_to_utf8(const std::wstring wstr)
{
	std::string utf8line;

	if (wstr.empty())
		return utf8line;

#ifdef _MSC_VER
	utf8::utf16to8(wstr.begin(), wstr.end(), std::back_inserter(utf8line));
#else
	utf8::utf32to8(wstr.begin(), wstr.end(), std::back_inserter(utf8line));
#endif
	return utf8line;
}

std::wstring utf8_to_wstring(const std::string str)
{
	std::wstring wide_line;

	if (str.empty())
		return wide_line;

#ifdef _MSC_VER
	utf8::utf8to16(str.begin(), str.end(), std::back_inserter(wide_line));
#else
	utf8::utf8to32(str.begin(), str.end(), std::back_inserter(wide_line));
#endif
	return wide_line;
}

std::string clean_guid_string(std::string input)
{
	return std::regex_replace(input, std::regex("-"), "");
}

bool ParseQueryString(std::string input,
		      std::map<std::string, std::string> &result)
{
	std::string s = input;

	while (s.size()) {
		std::string left;

		size_t offset = s.find('&');
		if (offset != std::string::npos) {
			left = s.substr(0, offset);
			s = s.substr(offset + 1);
		} else {
			left = s;
			s = "";
		}

		std::string right = "";
		offset = left.find('=');

		if (offset != std::string::npos) {
			right = left.substr(offset + 1);
			left = left.substr(0, offset);
		}

		result[left] = right;
	}

	return true;
}

std::string GetIdFromPointer(const void *ptr)
{
	char buf[32];

	sprintf_s(buf, "ptr(%p)", ptr);

	return buf;
}

const void *GetPointerFromId(const char *id)
{
	void *ptr;

	if (sscanf(id, "ptr(%p)", &ptr) > 0) {
		return ptr;
	} else {
		return nullptr;
	}
}
