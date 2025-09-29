#include "StreamElementsSharedVideoCompositionManager.hpp"

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>

#include "StreamElementsConfig.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <set>
#include <string>
#include <functional>

static void dispatch_external_event(std::string name, std::string args)
{
	std::string externalEventName =
		name.c_str() + 4; /* remove 'host' prefix */
	externalEventName[0] =
		tolower(externalEventName[0]); /* lower case first letter */

	StreamElementsMessageBus::GetInstance()->NotifyAllExternalEventListeners(
		StreamElementsMessageBus::DEST_ALL_EXTERNAL,
		StreamElementsMessageBus::SOURCE_APPLICATION, "OBS",
		externalEventName,
		CefParseJSON(args, JSON_PARSER_ALLOW_TRAILING_COMMAS));
}

static void dispatch_js_event(std::string name, std::string args)
{
	if (!StreamElementsGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = StreamElementsGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", name, args);
}

static void dispatch_shared_video_compositions_list_changed_event(
	StreamElementsSharedVideoCompositionManager *self)
{
	std::string name = "hostSharedVideoCompositionListChanged";
	std::string args = "null";

	dispatch_js_event(name, args);
	dispatch_external_event(name, args);

	dispatch_js_event("hostSharedVideoCompositionListChanged", "null");
	dispatch_external_event("hostSharedVideoCompositionListChanged",
				"null");
}

static bool IsVideoInUse()
{
	return obs_frontend_streaming_active() ||
	       obs_frontend_recording_active() ||
	       obs_frontend_recording_paused();
}

static void EnumOwnCanvases(std::function<bool(obs_canvas_t*)> callback) {
	auto config = StreamElementsConfig::GetInstance();

	if (!config)
		return;

	std::set<std::string> canvasUUIDs;

	config->GetSharedVideoCompositionIds(canvasUUIDs);

	// Enum canvases

	obs_frontend_canvas_list list = {0};

	obs_frontend_get_canvases(&list);

	for (size_t i = 0; i < list.canvases.num; ++i) {
		auto canvas = list.canvases.array[i];

		auto uuid = obs_canvas_get_uuid(canvas);

		if (canvasUUIDs.count(uuid)) {
			auto result = callback(canvas);

			if (!result)
				break;
		}
	}

	obs_frontend_canvas_list_free(&list);
}

static void AddUUID(std::string uuid) {
	auto config = StreamElementsConfig::GetInstance();

	if (!config)
		return;

	std::set<std::string> canvasUUIDs;

	config->GetSharedVideoCompositionIds(canvasUUIDs);

	if (!canvasUUIDs.count(uuid)) {
		canvasUUIDs.insert(uuid);
	}

	config->SetSharedVideoCompositionIds(canvasUUIDs);
}

static void RemoveUUID(std::string uuid) {
	auto config = StreamElementsConfig::GetInstance();

	if (!config)
		return;

	std::set<std::string> canvasUUIDs;

	config->GetSharedVideoCompositionIds(canvasUUIDs);

	if (canvasUUIDs.count(uuid)) {
		canvasUUIDs.erase(uuid);
	}

	config->SetSharedVideoCompositionIds(canvasUUIDs);
}

static obs_canvas_t *GetCanvasByNameRef(std::string name)
{
	obs_canvas_t *result = nullptr;

	EnumOwnCanvases([&](obs_canvas_t* canvas) -> bool {
		if (name == obs_canvas_get_name(canvas)) {
			result = obs_canvas_get_ref(canvas);

			return false;
		}

		return true;
	});

	return result;
}

static obs_canvas_t *GetCanvasByUUIDRef(std::string uuid)
{
	obs_canvas_t *result = nullptr;

	EnumOwnCanvases([&](obs_canvas_t *canvas) -> bool {
		if (uuid == obs_canvas_get_uuid(canvas)) {
			result = obs_canvas_get_ref(canvas);

			return false;
		}

		return true;
	});

	return result;
}

bool StreamElementsSharedVideoCompositionManager::SerializeCanvas(
	obs_canvas_t *canvas, CefRefPtr<CefDictionaryValue> d)
{
	if (!canvas)
		return false;

	std::string uuid = obs_canvas_get_uuid(canvas);

	d->SetString("name", obs_canvas_get_name(canvas));
	d->SetString("sharedVideoCompositionId", uuid);

	if (m_canvasUUIDToVideoCompositionInfoMap.count(uuid)) {
		d->SetString("videoCompositionId",
			     m_canvasUUIDToVideoCompositionInfoMap[uuid]
				     ->GetComposition()
				     ->GetId());
	} else {
		d->SetNull("videoCompositionId");
	}

	bool isVideoInUse = IsVideoInUse();

	d->SetBool("canChange", !isVideoInUse);
	d->SetBool("canRemove", !isVideoInUse);

	return true;
}

StreamElementsSharedVideoCompositionManager::
	StreamElementsSharedVideoCompositionManager(
		std::shared_ptr<StreamElementsVideoCompositionManager>
			videoCompositionManager)
	: m_videoCompositionManager(videoCompositionManager)
{
	// Sync config UUIDs to available canvases

	std::set<std::string> configUUIDs;
	std::set<std::string> obsUUIDs;

	auto config = StreamElementsConfig::GetInstance();

	if (!config)
		return;

	config->GetSharedVideoCompositionIds(configUUIDs);

	EnumOwnCanvases([&](obs_canvas_t *canvas) -> bool {
		auto uuid = obs_canvas_get_uuid(canvas);

		if (!uuid)
			return true;

		if (!obsUUIDs.count(uuid)) {
			obsUUIDs.insert(uuid);
		}

		return true;
	});

	std::list<std::string> removeUUIDs;

	for (auto uuid : configUUIDs) {
		if (obsUUIDs.count(uuid))
			continue;

		removeUUIDs.emplace_back(uuid);
	}

	for (auto uuid : removeUUIDs) {
		configUUIDs.erase(uuid);
	}

	config->SetSharedVideoCompositionIds(configUUIDs);

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);
}

StreamElementsSharedVideoCompositionManager::
~StreamElementsSharedVideoCompositionManager()
{
	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	m_canvasUUIDToVideoCompositionInfoMap.clear();

	m_videoCompositionManager = nullptr;
}

void StreamElementsSharedVideoCompositionManager::DeserializeSharedVideoComposition(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY) {
		return;
	}

	auto d = input->GetDictionary();

	if (!d->HasKey("name") || d->GetType("name") != VTYPE_STRING)
		return;

	std::string name = d->GetString("name");

	std::unique_lock guard(m_mutex);

	OBSCanvasAutoRelease canvas = GetCanvasByNameRef(name);

	if (!canvas) {
		obs_video_info ovi = {0};

		obs_get_video_info(&ovi);

		canvas = obs_frontend_add_canvas(name.c_str(), &ovi, PROGRAM);

		AddUUID(obs_canvas_get_uuid(canvas));
	}

	auto result = CefDictionaryValue::Create();

	if (SerializeCanvas(canvas, result)) {
		output->SetDictionary(result);
	}
}

void StreamElementsSharedVideoCompositionManager::SerializeAllSharedVideoCompositions(
	CefRefPtr<CefValue>& output)
{
	std::unique_lock guard(m_mutex);

	auto list = CefListValue::Create();

	EnumOwnCanvases([&](obs_canvas_t *canvas) -> bool {
		auto d = CefDictionaryValue::Create();

		SerializeCanvas(canvas, d);

		list->SetDictionary(list->GetSize(), d);

		return true;
	});

	output->SetList(list);
}

void StreamElementsSharedVideoCompositionManager::
	RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::unique_lock guard(m_mutex);

	output->SetBool(false);

	if (IsVideoInUse())
		return;

	if (!input.get() || input->GetType() != VTYPE_LIST)
		return;

	std::set<std::string> uuids;

	auto list = input->GetList();

	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return;

		std::string uuid = list->GetString(i);

		if (!uuids.count(uuid))
			uuids.insert(uuid);
	}

	for (auto uuid : uuids) {
		OBSCanvasAutoRelease canvas = GetCanvasByUUIDRef(uuid);

		if (!canvas)
			continue;

		obs_frontend_remove_canvas(canvas);

		RemoveUUID(uuid);

		if (m_canvasUUIDToVideoCompositionInfoMap.count(uuid)) {
			m_canvasUUIDToVideoCompositionInfoMap.erase(uuid);
		}

		output->SetBool(true);
	}
}

void StreamElementsSharedVideoCompositionManager::
	ConnectVideoCompositionToSharedVideoComposition(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue>& output)
{
	output->SetNull();

	if (!input || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	if (!d->HasKey("sharedVideoCompositionId") ||
	    d->GetType("sharedVideoCompositionId") != VTYPE_STRING)
		return;

	if (!d->HasKey("videoCompositionId") ||
	    d->GetType("videoCompositionId") != VTYPE_STRING)
		return;

	std::string sharedVideoCompositionId =
		d->GetString("sharedVideoCompositionId");

	std::string videoCompositionId = d->GetString("videoCompositionId");

	auto videoComposition =
		m_videoCompositionManager->GetVideoCompositionById(
			videoCompositionId);

	if (!videoComposition)
		return;

	auto videoCompositionInfo = videoComposition->GetCompositionInfo(
		this, "StreamElementsSharedVideoCompositionManager");

	std::unique_lock guard(m_mutex);

	if (IsVideoInUse())
		return;

	OBSCanvasAutoRelease canvas =
		GetCanvasByUUIDRef(sharedVideoCompositionId);

	if (!canvas)
		return;

	obs_video_info ovi = {0};

	videoComposition->GetVideoInfo(&ovi);

	if (!obs_canvas_reset_video(canvas, &ovi))
		return;

	m_canvasUUIDToVideoCompositionInfoMap[sharedVideoCompositionId] =
		videoCompositionInfo;

	OBSSourceAutoRelease videoCompositionRootSource =
		videoComposition->GetCompositionRootSourceRef();

	obs_canvas_set_channel(canvas, 0, videoCompositionRootSource);

	auto result = CefDictionaryValue::Create();

	SerializeCanvas(canvas, result);

	output->SetDictionary(result);

	dispatch_shared_video_compositions_list_changed_event(this);
}

void StreamElementsSharedVideoCompositionManager::
	DisconnectVideoCompositionsFromSharedVideoCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::unique_lock guard(m_mutex);
\
	output->SetBool(false);

	if (IsVideoInUse())
		return;

	if (!input.get() || input->GetType() != VTYPE_LIST)
		return;

	std::set<std::string> uuids;

	auto list = input->GetList();

	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return;

		std::string uuid = list->GetString(i);

		if (!uuids.count(uuid))
			uuids.insert(uuid);
	}

	for (auto uuid : uuids) {
		OBSCanvasAutoRelease canvas = GetCanvasByUUIDRef(uuid);

		if (canvas) {
			obs_canvas_set_channel(canvas, 0, nullptr);
		}

		if (m_canvasUUIDToVideoCompositionInfoMap.count(uuid) > 0) {
			m_canvasUUIDToVideoCompositionInfoMap.erase(uuid);
		}
	}

	output->SetBool(true);

	dispatch_shared_video_compositions_list_changed_event(this);
}

void StreamElementsSharedVideoCompositionManager::handle_obs_frontend_event(
	enum obs_frontend_event event, void *data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	StreamElementsSharedVideoCompositionManager *self =
		static_cast<StreamElementsSharedVideoCompositionManager *>(
			data);

	switch (event) {
	case OBS_FRONTEND_EVENT_CANVAS_ADDED:
	case OBS_FRONTEND_EVENT_CANVAS_REMOVED:
	case OBS_FRONTEND_EVENT_PROFILE_CHANGED:
	case OBS_FRONTEND_EVENT_SCENE_LIST_CHANGED:
		dispatch_shared_video_compositions_list_changed_event(self);
		break;
	default:
		break;
	}
}
