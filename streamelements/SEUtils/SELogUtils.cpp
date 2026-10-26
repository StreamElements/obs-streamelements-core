#include "SELogUtils.hpp"

#include <util/base.h>

//
// The only place in this domain that needs libobs, which is the whole point:
// everything upstream of here logs through a declaration that does not.
//
// SE_LOG_* are libobs's own values, so the level passes through untranslated.
//

void SELogVa(int logLevel, const char *format, va_list args)
{
	blogva(logLevel, format, args);
}

void SELog(int logLevel, const char *format, ...)
{
	va_list args;

	va_start(args, format);

	blogva(logLevel, format, args);

	va_end(args);
}
