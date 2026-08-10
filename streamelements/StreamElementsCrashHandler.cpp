#include "StreamElementsCrashHandler.hpp"

#include <util/base.h>

#ifdef SE_CRASH_HANDLER_BUGSPLAT
#include "StreamElementsBugSplatCrashHandler.hpp"
#endif

//
// Resolved entirely at compile time: CMake puts only the selected backend's
// sources into the build, so the branch not taken here refers to a class that
// is not linked -- and must therefore stay behind the preprocessor rather than
// behind a runtime condition.
//
StreamElementsCrashHandler *StreamElementsCrashHandler::Create()
{
#ifdef SE_CRASH_HANDLER_BUGSPLAT
	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: backend is BugSplat");

	return new StreamElementsBugSplatCrashHandler();
#else
	blog(LOG_WARNING,
	     "obs-streamelements-core: StreamElements: Crash Handler: compiled out; crashes will not be reported");

	return nullptr;
#endif
}
