#pragma once

//
// Logging, declared without libobs.
//
// SELog forwards to blog. The reason it exists rather than the call sites
// using blog directly is that blog is declared in libobs's util/base.h, and
// a translation unit that wants to log therefore needs libobs on its include
// path -- which is what keeps the API handler sources out of tests/, where
// there is no obs tree at all.
//
// Mirroring libobs's declarations in a header of our own does not work: the
// LOG_* levels live in an anonymous enum, so in the plug-in build -- where
// both headers reach the same translation unit through SEUtils.hpp -- the
// enumerators would be declared twice and that is a hard error. Hence a
// separate name rather than the same one.
//
// The levels are libobs's values, so SELogUtils.cpp is a one-line forward and
// nothing has to translate between two scales. For a test this is a link
// seam: define SELog, capture what the code under test logs, and assert on
// it -- which turns an error path that is otherwise only reachable in
// production into an ordinary assertion.
//
// Adopted file by file, as each one is made testable. Everything else in the
// plug-in still calls blog, and should, until it has a reason not to.
//

#include <cstdarg>

enum {
	SE_LOG_ERROR = 100,
	SE_LOG_WARNING = 200,
	SE_LOG_INFO = 300,
	SE_LOG_DEBUG = 400,
};

//
// Checked like printf where the compiler can -- these calls carry a lot of
// arguments, and a mismatched format string is a crash rather than a warning.
//
#if !defined(_MSC_VER)
#define SE_PRINTFATTR(f, a) __attribute__((__format__(__printf__, f, a)))
#else
#define SE_PRINTFATTR(f, a)
#endif

SE_PRINTFATTR(2, 3)
void SELog(int logLevel, const char *format, ...);

void SELogVa(int logLevel, const char *format, va_list args);

#undef SE_PRINTFATTR
