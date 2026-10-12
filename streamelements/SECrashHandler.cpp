#include "SECrashHandler.hpp"

#include <util/base.h>

#ifdef SE_CRASH_HANDLER_BUGSPLAT
#include "SEBugSplatCrashHandler.hpp"
#endif

#ifdef SE_CRASH_HANDLER_SENTRY
#include "SESentryCrashHandler.hpp"
#endif

//
// Resolved entirely at compile time: CMake puts only the selected backend's
// sources into the build, so the branch not taken here refers to a class that
// is not linked -- and must therefore stay behind the preprocessor rather than
// behind a runtime condition.
//
SECrashHandler *SECrashHandler::Create()
{
#ifdef SE_CRASH_HANDLER_BUGSPLAT
	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: backend is BugSplat");

	return new SEBugSplatCrashHandler();
#elif defined(SE_CRASH_HANDLER_SENTRY)
	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: backend is Sentry");

	return new SESentryCrashHandler();
#else
	blog(LOG_WARNING,
	     "obs-streamelements-core: StreamElements: Crash Handler: compiled out; crashes will not be reported");

	return nullptr;
#endif
}
