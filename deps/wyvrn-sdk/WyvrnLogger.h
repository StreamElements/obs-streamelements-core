#pragma once

#include <stdio.h>

namespace WyvrnSDK
{
	class WyvrnLogger
	{
	public:
		static void printf(const char* format, ...);
		static void fprintf(FILE* stream, const char* format, ...);

		static void wprintf(const wchar_t* format, ...);
		static void fwprintf(FILE* stream, const wchar_t* format, ...);
	};
}
