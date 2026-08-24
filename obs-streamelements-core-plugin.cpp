#include <obs-frontend-api.h>
#include <util/threading.h>
#include <util/platform.h>
#include <util/util.hpp>
#include <util/dstr.hpp>
#include <obs-module.h>
#include <obs-frontend-api.h>
#include <obs.hpp>
#include <obs-config.h>
#include <functional>
#include <sstream>
#include <thread>
#include <mutex>
#include <atomic>
#include <QMainWindow>
#include <QObject>
#include <QEvent>

#include "json11/json11.hpp"
#include "obs-websocket-api/obs-websocket-api.h"
#include "cef-headers.hpp"

#include "streamelements/audio-wrapper-source.h"
#include "streamelements/Version.generated.hpp"

#define ENABLE_PLUGIN 1

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-streamelements-core", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
	return "SE.Live";
}

using namespace std;
using namespace json11;

/* ========================================================================= */

#include <obs-frontend-api.h>
#include "streamelements/StreamElementsGlobalStateManager.hpp"
#include "streamelements/StreamElementsUtils.hpp"

/* ========================================================================= */

static void log_remaining_objects()
{
	StreamElementsVideoCompositionBase::CompositionInfo::
		LogRemainingCompositionInfos();

	obs_enum_sources(
		[](void *, obs_source_t *source) -> bool {
			auto id = obs_source_get_id(source);
			auto name = obs_source_get_name(source);

			blog(LOG_WARNING,
			     "[obs-streamelements-core]: remaining source id '%s', name '%s'",
			     id, name);

			return true;
		},
		nullptr);

	obs_enum_outputs(
		[](void *, obs_output_t *output) -> bool {
			auto id = obs_output_get_id(output);
			auto name = obs_output_get_name(output);

			blog(LOG_WARNING,
			     "[obs-streamelements-core]: remaining output id '%s', name '%s'",
			     id, name);

			return true;
		},
		nullptr);

	obs_enum_encoders(
		[](void *, obs_encoder_t *encoder) -> bool {
			auto id = obs_encoder_get_id(encoder);
			auto name = obs_encoder_get_name(encoder);

			blog(LOG_WARNING,
			     "[obs-streamelements-core]: remaining encoder id '%s', name '%s'",
			     id, name);

			return true;
		},
		nullptr);

	obs_enum_services([](void *, obs_service_t *service) -> bool {
		auto id = obs_service_get_id(service);
		auto name = obs_service_get_name(service);

		blog(LOG_WARNING,
		     "[obs-streamelements-core]: remaining service id '%s', name '%s'",
		     id, name);

		return true;
	}, nullptr);
}

/* ========================================================================= */

MODULE_EXPORT bool obs_module_load(void)
{
#if ENABLE_PLUGIN
	std::string version = GetStreamElementsPluginVersionString();

	blog(LOG_INFO, "[obs-streamelements-core]: Version %s",
	     version.c_str());

	obs_register_source(&audio_wrapper_source);
#endif
	return true;
}

//
// ---------------------------------------------------------------------------
// The OBS update-thread race (CORE-786)
// ---------------------------------------------------------------------------
//
// OBSBasic::OBSInit() spawns the update checker BEFORE it tells us the UI is
// ready, and the two are not ordered with respect to each other:
//
//   OBSBasic.cpp:1307   TimedCheckForUpdates() -> new AutoUpdateThread; start()
//   OBSBasic.cpp:1371   OnFirstLoad()          -> FINISHED_LOADING
//
// AutoUpdateThread then reaches back onto the main thread twice:
//
//   1. queryUpdate() does invokeMethod(this, "queryUpdateSlot",
//      Qt::BlockingQueuedConnection), and `this` is the QThread OBJECT, which
//      lives in the main thread. So the update dialog's exec() -- a nested,
//      modal event loop -- runs on the main thread, dispatched by whichever
//      event pump runs next. That pump is frequently one of ours, inside
//      Initialize().
//
//   2. After launching the updater, run() finishes with
//      invokeMethod(App()->GetMainWindow(), "close"), queued. The next pump --
//      again, often ours -- delivers it, so OBSBasic::close() -> closeEvent()
//      -> OBS_FRONTEND_EVENT_EXIT all run NESTED INSIDE our Initialize().
//
// We are then asked to tear down an object graph we are still building. Every
// crash chased under this issue was a destructor touching something
// Initialize() had just created: docks built at 02:48:17, destroyed at
// 02:48:20, `init done` never reached.
//
// Detection: an event filter on the OBS main window watching QEvent::Close.
// Filters on a widget run before that widget's own closeEvent(), and
// closeEvent() is what fires EXIT -- so the flag is set before teardown
// begins. QEvent::Close is the correct signal: the queued call arrives as a
// private QMetaCallEvent carrying no readable method name, so it cannot be
// told apart from any other invokeMethod on the main window.
//
// The flag is deliberately narrow. A close AFTER Initialize() completed is an
// ordinary shutdown and must behave exactly as it always has, or every normal
// exit would leak. Only a close that beats `init done` is poison.
//
static std::atomic<bool> s_initializeCompleted(false);
static std::atomic<bool> s_closedBeforeInitCompleted(false);
static std::atomic<bool> s_closeWatcherInstalled(false);

static void NoteObsCloseRequested(const char *source)
{
	if (s_initializeCompleted.load())
		return; // ordinary shutdown

	if (s_closedBeforeInitCompleted.exchange(true))
		return;

	blog(LOG_WARNING,
	     "[obs-streamelements-core]: OBS was asked to close before initialization completed (%s); UI teardown will be suppressed",
	     source);
}

// Declared in StreamElementsUtils.hpp; called from every UI teardown site.
bool SEIsUiTeardownSafe()
{
	return !s_closedBeforeInitCompleted.load();
}

void SENoteInitializeCompleted()
{
	s_initializeCompleted.store(true);
}

class StreamElementsObsCloseWatcher : public QObject {
public:
	StreamElementsObsCloseWatcher(QObject *parent) : QObject(parent) {}

protected:
	bool eventFilter(QObject *watched, QEvent *event) override
	{
		if (event->type() == QEvent::Close)
			NoteObsCloseRequested("QEvent::Close");

		// Observe only. Never consume: OBS must close exactly as it
		// would have without us.
		return QObject::eventFilter(watched, event);
	}
};

static void InstallObsCloseWatcher()
{
	if (s_closeWatcherInstalled.load())
		return;

	auto mainWindow =
		static_cast<QMainWindow *>(obs_frontend_get_main_window());

	if (!mainWindow)
		return; // not up yet; the caller tries again later

	s_closeWatcherInstalled.store(true);

	// Parented to the main window so it dies with it.
	mainWindow->installEventFilter(
		new StreamElementsObsCloseWatcher(mainWindow));

	blog(LOG_INFO,
	     "[obs-streamelements-core]: watching the OBS main window for close");
}

void handle_obs_frontend_event(enum obs_frontend_event event, void *data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	static bool isRunning = true;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		isRunning = true;

		// Belt and braces: obs_module_post_load() usually gets this,
		// but the main window certainly exists by now.
		InstallObsCloseWatcher();

		blog(LOG_INFO, "[obs-streamelements-core]: initializing");

		// Initialize StreamElements plug-in
		StreamElementsGlobalStateManager::GetInstance()->Initialize(
			static_cast<QMainWindow *>(
				obs_frontend_get_main_window()));

		blog(LOG_INFO, "[obs-streamelements-core]: init done");
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		if (!isRunning)
			return;

		isRunning = false;

		// Second, independent trigger: a quit path that never sends
		// QEvent::Close still reaches here, and EXIT arriving before
		// `init done` means the same thing.
		NoteObsCloseRequested("OBS_FRONTEND_EVENT_EXIT");

		obs_frontend_remove_event_callback(handle_obs_frontend_event,
						   nullptr);

		if (!SEIsUiTeardownSafe()) {
			blog(LOG_WARNING,
			     "[obs-streamelements-core]: leaking plug-in state rather than tearing down a half-built object graph");

			// Running the destructors is what corrupts the widget
			// tree, so do not run them at all. The process is
			// exiting and about to be replaced by the updater; a
			// leak is strictly better than a crash.
			StreamElementsGlobalStateManager::Leak();
			break;
		}

		// Shutdown StreamElements plug-in
		blog(LOG_INFO, "[obs-streamelements-core]: shutting down");

		StreamElementsGlobalStateManager::Destroy();
		break;
	default:
		break;
	}
}

MODULE_EXPORT void obs_module_post_load(void)
{
#if ENABLE_PLUGIN
	// Earliest point the main window may exist. The update thread can in
	// principle prompt before FINISHED_LOADING, so install as early as
	// possible; the handler installs again if this was too early.
	InstallObsCloseWatcher();

	obs_frontend_add_event_callback(handle_obs_frontend_event, nullptr);

	/*
	auto vendor = obs_websocket_register_vendor("obs-streamelements-core");
	if (!vendor)
		return;

	auto emit_event_request_cb = [](obs_data_t *request_data, obs_data_t *,
					void *) {
		const char *event_name =
			obs_data_get_string(request_data, "event_name");
		if (!event_name)
			return;

		OBSDataAutoRelease event_data =
			obs_data_get_obj(request_data, "event_data");
		const char *event_data_string =
			event_data ? obs_data_get_json(event_data) : "{}";
	};

	if (!obs_websocket_vendor_register_request(
		    vendor, "emit_event", emit_event_request_cb, nullptr))
		blog(LOG_WARNING,
		     "[obs-streamelements-core]: Failed to register obs-websocket request emit_event");
	*/
#endif
}

MODULE_EXPORT void obs_module_unload(void)
{
#if ENABLE_PLUGIN
	log_remaining_objects();

	blog(LOG_INFO, "[obs-streamelements-core]: shutdown complete");

	SETRACE_DUMP();
#endif
}
