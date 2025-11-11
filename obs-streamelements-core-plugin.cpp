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
	blog(LOG_INFO, "[obs-streamelements-core]: Version %lu",
	     STREAMELEMENTS_PLUGIN_VERSION);

	obs_register_source(&audio_wrapper_source);
#endif
	return true;
}

void handle_obs_frontend_event(enum obs_frontend_event event, void *data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	static bool isRunning = true;

	switch (event) {
	case OBS_FRONTEND_EVENT_FINISHED_LOADING:
		isRunning = true;

		blog(LOG_INFO, "[obs-streamelements-core]: initializing");

		// Initialize StreamElements plug-in
		StreamElementsGlobalStateManager::GetInstance()->Initialize(
			static_cast<QMainWindow *>(obs_frontend_get_main_window()));

		blog(LOG_INFO, "[obs-streamelements-core]: init done");
		break;
	case OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN:
	case OBS_FRONTEND_EVENT_EXIT:
		if (!isRunning)
			return;

		isRunning = false;

		obs_frontend_remove_event_callback(handle_obs_frontend_event,
						   nullptr);

		// Shutdown StreamElements plug-in
		blog(LOG_INFO, "[obs-streamelements-core]: shutting down");

		//StreamElementsGlobalStateManager::GetInstance()
		//	->Shutdown();
		StreamElementsGlobalStateManager::Destroy();
		break;
	default:
		break;
	}
}

MODULE_EXPORT void obs_module_post_load(void)
{
#if ENABLE_PLUGIN
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
