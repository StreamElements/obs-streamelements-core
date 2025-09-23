#include "StreamElementsSharedVideoCompositionManager.hpp"

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>

#include "StreamElementsConfig.hpp"

#include <set>
#include <string>
#include <functional>

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

static bool SerializeCanvas(obs_canvas_t* canvas, CefRefPtr<CefDictionaryValue> d) {
	if (!canvas)
		return false;

	d->SetString("name", obs_canvas_get_name(canvas));
	d->SetString("id", obs_canvas_get_uuid(canvas));

	bool isVideoInUse = IsVideoInUse();

	d->SetBool("canChange", !isVideoInUse);
	d->SetBool("canRemove", !isVideoInUse);

	return true;
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
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	auto list = CefListValue::Create();

	EnumOwnCanvases([&](obs_canvas_t *canvas) -> bool {
		auto d = CefDictionaryValue::Create();

		SerializeCanvas(canvas, d);

		list->SetDictionary(list->GetSize(), d);

		return true;
	});

	output->SetList(list);
}

void RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
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

		output->SetBool(true);
	}
}
