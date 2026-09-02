#include "WyvrnLogger.h"
#include <stdarg.h>

using namespace WyvrnSDK;


void WyvrnLogger::printf(const char* format, ...)
{
#if _DEBUG
	va_list args;
	va_start(args, format);
	::vprintf(format, args);
	va_end(args);
#endif
}

void WyvrnLogger::fprintf(FILE* stream, const char* format, ...)
{
#if _DEBUG
	va_list args;
	va_start(args, format);
	::vfprintf(stream, format, args);
	va_end(args);
#endif
}

void WyvrnLogger::wprintf(const wchar_t* format, ...)
{
#if _DEBUG
	va_list args;
	va_start(args, format);
	::vwprintf(format, args);
	va_end(args);
#endif
}

void WyvrnLogger::fwprintf(FILE* stream, const wchar_t* format, ...)
{
#if _DEBUG
	va_list args;
	va_start(args, format);
	::vfwprintf(stream, format, args);
	va_end(args);
#endif
}
