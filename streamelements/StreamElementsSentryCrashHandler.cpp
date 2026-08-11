#include "StreamElementsSentryCrashHandler.hpp"

#include "StreamElementsUtils.hpp"

#include <util/base.h>
#include <obs.h>

#include <sentry.h>

#include <string>

//
// Scaffolding only. This initializes the SDK and proves the integration links;
// the crash-time work -- the module-of-interest gate, StreamElementsCrashContext,
// the consent dialog and sentry_handle_exception() -- lands in the next commit.
//

// Empty means crash reporting is inert: sentry_init() is skipped entirely rather
// than run against a bad DSN, so a build without the secret behaves like
// STREAMELEMENTS_CRASH_HANDLER=none instead of failing at runtime.
#ifndef SE_SENTRY_DSN
#define SE_SENTRY_DSN ""
#endif

static bool s_initialized = false;

StreamElementsSentryCrashHandler::StreamElementsSentryCrashHandler()
{
	if (s_initialized)
		return;

	const std::string dsn = SE_SENTRY_DSN;

	if (dsn.empty()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: StreamElements: Crash Handler: no Sentry DSN was compiled in; crash reporting is disabled");
		return;
	}

	sentry_options_t *options = sentry_options_new();

	sentry_options_set_dsn(options, dsn.c_str());

	// Matches the tag CI creates for the same build, so a Sentry release lines
	// up with the GitHub release and the CDN manifest version.
	const std::string release = "obs-streamelements-core@" +
				    GetStreamElementsPluginVersionString();
	sentry_options_set_release(options, release.c_str());

	if (sentry_init(options) != 0) {
		blog(LOG_ERROR,
		     "obs-streamelements-core: StreamElements: Crash Handler: sentry_init() failed");
		return;
	}

	s_initialized = true;

	blog(LOG_INFO,
	     "obs-streamelements-core: StreamElements: Crash Handler: Sentry initialized (%s)",
	     release.c_str());
}

void StreamElementsSentryCrashHandler::StopAsyncHangDetection()
{
	// No hang detection yet. sentry-native has its own app-hang tracking
	// (enable_app_hang_tracking) which is worth evaluating against the
	// currently disabled HANG_DETECTION_ENABLED path in the BugSplat handler.
}

StreamElementsSentryCrashHandler::~StreamElementsSentryCrashHandler()
{
	// Deliberately not calling sentry_close(): the BugSplat handler likewise
	// never tears down, on the grounds that shutting the reporter down early
	// loses exceptions thrown during shutdown. See
	// StreamElementsGlobalStateManager::Shutdown().
}
