#include <obs.hpp>
#include "StreamElementsObsSceneManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsConfig.hpp"

#include <util/platform.h>
#include <string.h>

#include <unordered_map>
#include <regex>

#include <QListView>
#include <QDockWidget>
#include <QAbstractItemModel>
#include <QLayout>
#include <QHBoxLayout>
#include <QPushButton>

#include "StreamElementsGlobalStateManager.hpp"

#include "canvas-mutate.hpp"
#include "canvas-scan.hpp"

//#define CP(a) blog(LOG_INFO, "checkpoint %d", a)

// obs_sceneitem_group_add_item() and obs_sceneitem_group_remove_item()
// APIs are fixed in OBS 25 and are broken in previous versions.
//
#if LIBOBS_API_MAJOR_VER >= 25
#define ENABLE_OBS_GROUP_ADD_REMOVE_ITEM 1
#else
#define ENABLE_OBS_GROUP_ADD_REMOVE_ITEM 0
#endif

// obs_scene_get_group deadlocks on full_lock(scene) when inside a scene signal handler
#define ENABLE_GET_GROUP_SCENE_IN_SIGNAL_HANDLER 0

bool s_shutdown = false;

///////////////////////////////////////////////////////////////////////////

static inline const std::string safe_str(const char *s)
{
	if (s == NULL)
		return "(NULL)";
	else
		return s;
}

///////////////////////////////////////////////////////////////////////////

static bool is_active_scene(obs_scene_t *scene)
{
	if (!scene)
		return false;

	obs_source_t *current_scene_source = obs_frontend_get_current_scene();

	if (!current_scene_source)
		return false;

	bool result = false;

	if (scene == obs_scene_from_source(current_scene_source))
		result = true;

	obs_source_release(current_scene_source);

	return result;
}

static bool is_child_of_current_scene(obs_sceneitem_t *sceneitem)
{
	bool result = false;

	obs_source_t *root_scene_source = obs_frontend_get_current_scene();
	obs_scene_t *root_scene = obs_scene_from_source(root_scene_source);

	obs_scene_t *parent_scene = obs_sceneitem_get_scene(sceneitem);
	if (!obs_scene_is_group(parent_scene)) {
		if (root_scene == parent_scene)
			result = true;
	} else {
#if ENABLE_GET_GROUP_SCENE_IN_SIGNAL_HANDLER
		// how to get parent scene?!
		obs_source_t *group_source = obs_scene_get_source(parent_scene);

		const char *group_name = obs_source_get_name(group_source);

		obs_sceneitem_t *group_sceneitem =
			obs_scene_get_group(root_scene, group_name);

		if (group_sceneitem)
			result = true;
#endif
	}

	obs_source_release(root_scene_source);

	return result;
}

static bool is_active_scene(obs_sceneitem_t *sceneitem)
{
	if (s_shutdown)
		return false;

	if (!sceneitem)
		return false;

	return is_child_of_current_scene(sceneitem);
}

static obs_source_t *
get_scene_source_by_id_addref(std::string id, bool getCurrentIfNoId = false)
{
	obs_source_t *result = nullptr;

	if (id.size()) {
		const void *ptr = GetPointerFromId(id.c_str());

		struct obs_frontend_source_list scenes = {};

		obs_frontend_get_scenes(&scenes);

		for (size_t idx = 0; idx < scenes.sources.num; ++idx) {
			obs_source_t *source = scenes.sources.array[idx];

			if (ptr == source) {
				result = source;

				break;
			}
		}

		obs_frontend_source_list_free(&scenes);
	}

	if (result) {
		obs_source_get_ref(result);
	} else if (getCurrentIfNoId) {
		result = obs_frontend_get_current_scene();
	}

	return result;
}

static obs_source_t *
get_scene_source_by_id_addref(CefRefPtr<CefDictionaryValue> root,
			      std::string sceneIdFieldName,
			      bool getCurrentIfNoId = false)
{
	std::string sceneId =
		root->HasKey(sceneIdFieldName) &&
				root->GetType(sceneIdFieldName) == VTYPE_STRING
			? root->GetString(sceneIdFieldName).ToString()
			: "";

	return get_scene_source_by_id_addref(sceneId, getCurrentIfNoId);
}

static obs_source_t *
get_scene_source_by_id_addref(CefRefPtr<CefValue> input,
			      std::string sceneIdFieldName,
			      bool getCurrentIfNoId = false)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return get_scene_source_by_id_addref("", getCurrentIfNoId);
	else
		return get_scene_source_by_id_addref(input->GetDictionary(),
						     sceneIdFieldName,
						     getCurrentIfNoId);
}

///////////////////////////////////////////////////////////////////////////

static void signal_parent_scene(obs_scene_t *parent, const char *command,
				calldata_t *params)
{
	calldata_set_ptr(params, "scene", parent);

	obs_source_t *source = obs_scene_get_source(parent);

	if (!source)
		return;

	signal_handler_t *handler = obs_source_get_signal_handler(source);

	if (!handler)
		return;

	signal_handler_signal(handler, command, params);
}

static void signal_refresh(obs_scene_t *scene)
{
	struct calldata params;
	uint8_t stack[128];

	calldata_init_fixed(&params, stack, sizeof(stack));
	signal_parent_scene(scene, "refresh", &params);
}

void StreamElementsObsSceneManager::RefreshObsSceneItemsList()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_source_t *scene_source = obs_frontend_get_current_scene();
	obs_scene_t* current_scene = obs_scene_from_source(scene_source);
	signal_refresh(current_scene);
	obs_source_release(scene_source);

#if LIBOBS_API_MAJOR_VER < 25
	/*
	 * This is a *HACK* which overrides an oversight in group
	 * management API: when scene item group is added to the scene
	 * items list, UI is not refreshed.
	 *
	 * We invoke ReorderItems() on the SourceTree class in OBS UI
	 * which in turn refreshes the content of the scene items
	 * QListView.
	 *
	 * TODO: TBD: replace this when an official alternative becomes
	 *            available or remove if this becomes obsolete.
	 */

	QDockWidget *sourcesDock =
		(QDockWidget *)m_parent->findChild<QDockWidget *>(
			"sourcesDock");

	if (!sourcesDock)
		return;

	QListView *listView =
		(QListView *)sourcesDock->findChild<QListView *>("sources");

	if (!listView)
		return;

	QtExecSync([listView]() -> void {
#if ENABLE_OBS_GROUP_ADD_REMOVE_ITEM
		QMetaObject::invokeMethod(listView, "SceneChanged",
					  Qt::DirectConnection);
#else
		QMetaObject::invokeMethod(listView, "ReorderItems",
					  Qt::DirectConnection);
#endif
	});

#endif
}

static bool IsSceneItemInfoValid(CefRefPtr<CefValue> input, bool requireClass,
				 bool requireSettings)
{
	if (input->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	if (!root->HasKey("name") || root->GetType("name") != VTYPE_STRING ||
	    root->GetString("name").empty()) {
		return false;
	}

	if (requireClass) {
		if (!root->HasKey("class") ||
		    root->GetType("class") != VTYPE_STRING ||
		    root->GetString("class").empty()) {
			return false;
		}
	}

	if (requireSettings) {
		if (!root->HasKey("settings") ||
		    root->GetType("settings") != VTYPE_DICTIONARY) {
			return false;
		}
	}

	return true;
}

static bool IsBrowserSourceSceneItemInfoValid(CefRefPtr<CefValue> input)
{
	if (!IsSceneItemInfoValid(input, false, true)) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> d =
		input->GetDictionary()->GetDictionary("settings");

	if (!d->HasKey("url") || d->GetType("url") != VTYPE_STRING ||
	    d->GetString("url").empty()) {
		return false;
	}

	if (!d->HasKey("width") || d->GetType("width") != VTYPE_INT ||
	    d->GetInt("width") <= 0) {
		return false;
	}

	if (!d->HasKey("height") || d->GetType("height") != VTYPE_INT ||
	    d->GetInt("height") <= 0) {
		return false;
	}

	return true;
}

///////////////////////////////////////////////////////////////////////


static CefRefPtr<CefValue> SerializeObsSourceSettings(obs_source_t *source)
{
	CefRefPtr<CefValue> result = CefValue::Create();

	if (source) {
		obs_data_t *data = obs_source_get_settings(source);

		if (data) {
			result = SerializeObsData(data);

			obs_data_release(data);
		}
	} else {
		result->SetNull();
	}

	return result;
}

static CefRefPtr<CefDictionaryValue> SerializeVec2(vec2 &vec)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetDouble("x", vec.x);
	d->SetDouble("y", vec.y);

	return d;
}

static double DeserializeDoubleValue(CefRefPtr<CefValue> input,
				     double defaultValue)
{
	if (input->GetType() == VTYPE_DOUBLE)
		return input->GetDouble();

	if (input->GetType() == VTYPE_INT)
		return (double)input->GetInt();

	return defaultValue;
}

static int DeserializeIntValue(CefRefPtr<CefValue> input, int defaultValue)
{
	if (input->GetType() == VTYPE_DOUBLE)
		return int(std::round(input->GetDouble()));

	if (input->GetType() == VTYPE_INT)
		return input->GetInt();

	return defaultValue;
}

static vec2 DeserializeVec2(CefRefPtr<CefValue> input)
{
	vec2 result = {0};

	if (!!input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (d->HasKey("x") && d->HasKey("y")) {
			result.x =
				DeserializeDoubleValue(d->GetValue("x"), 1.0);

			result.y =
				DeserializeDoubleValue(d->GetValue("y"), 1.0);
		}
	}

	return result;
}

static bool DeserializeSceneItemComposition(CefRefPtr<CefValue> input,
					    obs_transform_info &info,
					    obs_sceneitem_crop &crop)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY) {
		return false;
	}

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	if (!root->HasKey("composition") ||
	    root->GetType("composition") != VTYPE_DICTIONARY) {
		return false;
	}

	memset(&info, 0, sizeof(info));
	memset(&crop, 0, sizeof(crop));

	CefRefPtr<CefDictionaryValue> d = root->GetDictionary("composition");

	if (d->HasKey("position"))
		info.pos = DeserializeVec2(d->GetValue("position"));

	if (d->HasKey("scale"))
		info.scale = DeserializeVec2(d->GetValue("scale"));
	else
		info.scale = {1, 1};

	if (d->HasKey("rotationDegrees"))
		info.rot = DeserializeDoubleValue(
			d->GetValue("rotationDegrees"), 0);

	if (d->HasKey("crop") && d->GetType("crop") == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> c = d->GetDictionary("crop");

		auto get = [&](const char *key, double defaultValue = 0) {
			if (!c->HasKey(key))
				return defaultValue;

			if (c->GetType(key) == VTYPE_INT)
				return (double)c->GetInt(key);
			else if (c->GetType(key) == VTYPE_DOUBLE)
				return c->GetDouble(key);
			else
				return defaultValue;
		};

		crop.left = get("left");
		crop.top = get("top");
		crop.right = get("right");
		crop.bottom = get("bottom");
	}

	if (d->HasKey("alignment")) {
		info.alignment =
			GetInt32FromAlignmentId(d->GetString("alignment"));
	} else {
		info.alignment = OBS_ALIGN_LEFT | OBS_ALIGN_TOP;
	}

	if (d->HasKey("boundsType")) {
		std::string v = d->GetString("boundsType");

		if (v == "none")
			info.bounds_type = OBS_BOUNDS_NONE;
		else if (v == "stretch")
			info.bounds_type = OBS_BOUNDS_STRETCH;
		else if (v == "scale_to_inner_rect")
			info.bounds_type = OBS_BOUNDS_SCALE_INNER;
		else if (v == "scale_to_outer_rect")
			info.bounds_type = OBS_BOUNDS_SCALE_OUTER;
		else if (v == "scale_to_width")
			info.bounds_type = OBS_BOUNDS_SCALE_TO_WIDTH;
		else if (v == "scale_to_height")
			info.bounds_type = OBS_BOUNDS_SCALE_TO_HEIGHT;
		else if (v == "max_size_only")
			info.bounds_type = OBS_BOUNDS_MAX_ONLY;
		else
			return false;

		if (info.bounds_type != OBS_BOUNDS_NONE) {
			if (d->HasKey("bounds"))
				info.bounds =
					DeserializeVec2(d->GetValue("bounds"));

			if (d->HasKey("boundsAlignment") &&
			    d->GetType("boundsAlignment") == VTYPE_STRING)
				info.bounds_alignment = GetInt32FromAlignmentId(
					d->GetString("boundsAlignment"));
			else
				info.bounds_alignment = OBS_ALIGN_CENTER;
		}
	}

	return true;
}

static CefRefPtr<CefDictionaryValue>
SerializeObsSceneItemCompositionSettings(obs_source_t *source,
					 obs_sceneitem_t *sceneitem)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	if (source) {
		d->SetInt("srcWidth", obs_source_get_width(source));
		d->SetInt("srcHeight", obs_source_get_height(source));
	}

	obs_transform_info info;
	obs_sceneitem_get_info(sceneitem, &info);

	d->SetDictionary("position", SerializeVec2(info.pos));
	d->SetDictionary("scale", SerializeVec2(info.scale));
	d->SetDouble("rotationDegrees", (double)info.rot);

	obs_sceneitem_crop crop;
	obs_sceneitem_get_crop(sceneitem, &crop);

	CefRefPtr<CefDictionaryValue> cropInfo = CefDictionaryValue::Create();

	cropInfo->SetInt("left", crop.left);
	cropInfo->SetInt("top", crop.top);
	cropInfo->SetInt("right", crop.right);
	cropInfo->SetInt("bottom", crop.bottom);

	d->SetDictionary("crop", cropInfo);

	d->SetString("alignment", GetAlignmentIdFromInt32(info.alignment));

	switch (info.bounds_type) {
	case OBS_BOUNDS_NONE: /**< no bounds */
		d->SetString("boundsType", "none");
		break;
	case OBS_BOUNDS_STRETCH: /**< stretch (ignores base scale) */
		d->SetString("boundsType", "stretch");
		break;
	case OBS_BOUNDS_SCALE_INNER: /**< scales to inner rectangle */
		d->SetString("boundsType", "scale_to_inner_rect");
		break;
	case OBS_BOUNDS_SCALE_OUTER: /**< scales to outer rectangle */
		d->SetString("boundsType", "scale_to_outer_rect");
		break;
	case OBS_BOUNDS_SCALE_TO_WIDTH: /**< scales to the width  */
		d->SetString("boundsType", "scale_to_width");
		break;
	case OBS_BOUNDS_SCALE_TO_HEIGHT: /**< scales to the height */
		d->SetString("boundsType", "scale_to_height");
		break;
	case OBS_BOUNDS_MAX_ONLY: /**< no scaling: maximum size only */
		d->SetString("boundsType", "max_size_only");
		break;
	}

	d->SetDictionary("bounds", SerializeVec2(info.bounds));
	d->SetString("boundsAlignment",
		     GetAlignmentIdFromInt32(info.bounds_alignment));

	return d;
}

static void SerializeSourceAndSceneItem(CefRefPtr<CefValue> &result,
					obs_scene_t* root_scene,
					obs_source_t *source,
					obs_sceneitem_t *sceneitem,
					const int order = -1,
					bool serializeDetails = true,
					bool serializeProperties = false,
					StreamElementsVideoCompositionBase *videoComposition = nullptr)
{
	result->SetNull();

	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();

	std::string sceneItemId = GetIdFromPointer(sceneitem);

	if (!videoComposition) {
		auto videoCompositionManager =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager();

		if (!videoCompositionManager.get())
			return;

		videoComposition = videoCompositionManager
				->GetVideoCompositionBySceneItemId(sceneItemId,
								   &root_scene).get();
	}

	root->SetString("id", sceneItemId);

	if (videoComposition) {
		root->SetString("videoCompositionId",
				videoComposition->GetId());
		root->SetString(
			"sceneId",
			GetIdFromPointer(obs_scene_get_source(root_scene)));
	}

	if (source) {
		const char *name = obs_source_get_name(source);
		const char *id = obs_source_get_id(source);

		if (name)
			root->SetString("name", safe_str(name));

		if (id)
			root->SetString("class", safe_str(id));
	}
	root->SetBool("visible", obs_sceneitem_visible(sceneitem));
	root->SetBool("selected", obs_sceneitem_selected(sceneitem));
	root->SetBool("locked", obs_sceneitem_locked(sceneitem));

	if (order >= 0) {
		root->SetInt("order", order);
	}

	root->SetDictionary(
		"composition",
		SerializeObsSceneItemCompositionSettings(source, sceneitem));

	if (serializeDetails) {
		if (!obs_sceneitem_is_group(sceneitem) && source &&
		    strcmp(obs_source_get_id(source), "scene") != 0) {
			/* Not a group and not a scene */
			root->SetValue("settings",
				       SerializeObsSourceSettings(source));
		}

		{
			obs_source_t *parent_scene =
				obs_frontend_get_current_scene();

			if (!!parent_scene) {
				obs_scene_t *scene =
					obs_scene_from_source(parent_scene);

				if (!!scene) {
					obs_sceneitem_t *group =
						obs_sceneitem_get_group(
							scene, sceneitem);
					if (!!group) {
						root->SetString(
							"parentId",
							GetIdFromPointer(
								group));
					}
				}

				obs_source_release(parent_scene);
			}
		}

		if (obs_sceneitem_is_group(sceneitem)) {
			/* Serialize group */
			struct local_context {
				CefRefPtr<CefListValue> list;
				std::vector<OBSSceneItemAutoRelease> groupItems;
			};

			local_context context;

			context.list = CefListValue::Create();
			context.groupItems.clear();

			/* Serialize group items */
			obs_sceneitem_group_enum_items(
				sceneitem,
				[](obs_scene_t *scene,
				   obs_sceneitem_t *sceneitem, void *param) {
					local_context *context =
						(local_context *)param;

					obs_sceneitem_addref(
						sceneitem); /* will be released below */

					context->groupItems.push_back(
						sceneitem);

					// Continue iteration
					return true;
				},
				&context);

			for (auto it = context.groupItems.cbegin();
			     it != context.groupItems.cend(); ++it) {
				obs_source_t *source = obs_sceneitem_get_source(
					*it); // does not increase refcount

				if (!source)
					continue;

				CefRefPtr<CefValue> item = CefValue::Create();

				SerializeSourceAndSceneItem(
					item, root_scene, source, *it,
					context.list->GetSize(),
					serializeDetails, serializeProperties,
					videoComposition);

				context.list->SetValue(context.list->GetSize(),
						       item);
			}

			/* Group items */
			root->SetList("items", context.list);
		} else if (source &&
			   strcmp(obs_source_get_id(source), "scene") == 0) {
			/* Scene source */
		} else {
			/* Not a group and not a scene, handled above */
		}

		#if SE_ENABLE_SCENEITEM_ACTIONS
		root->SetList(
			"actions",
			StreamElementsSceneItemsMonitor::GetSceneItemActions(
				sceneitem)
				->Copy());
		#endif

		#if SE_ENABLE_SCENEITEM_ICONS
		root->SetValue(
			"icon",
			StreamElementsSceneItemsMonitor::GetSceneItemIcon(
				sceneitem)
				->Copy());
		#endif

		#if SE_ENABLE_SCENEITEM_DEFAULT_ACTION 
		root->SetValue("defaultAction",
			       StreamElementsSceneItemsMonitor::
				       GetSceneItemDefaultAction(sceneitem)
					       ->Copy());
		#endif

		root->SetValue("auxiliaryData",
			       StreamElementsSceneItemsMonitor::
				       GetSceneItemAuxiliaryData(sceneitem)
					       ->Copy());

		#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
		root->SetValue(
			"contextMenu",
			StreamElementsSceneItemsMonitor::GetSceneItemContextMenu(
				sceneitem)
				->Copy());
		#endif

		#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
		root->SetValue(
			"uiSettings",
			StreamElementsSceneItemsMonitor::GetSceneItemUISettings(
					sceneitem)
				->Copy());
		#endif
	}

	root->SetList("filters",
		      SerializeObsSourceFilters(source, serializeProperties));

	result->SetDictionary(root);
}

static void SerializeObsScene(obs_source_t *sceneSource, CefRefPtr<CefValue> &result)
{
	result->SetNull();

	auto scene = obs_scene_from_source(sceneSource);

	auto videoCompositionManager =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager();

	if (!videoCompositionManager.get())
		return;

	auto videoComposition = videoCompositionManager->GetVideoCompositionByScene(scene);

	if (!videoComposition.get())
		return;

	videoComposition->SerializeScene(scene, result);
}

static void SerializeObsScene(obs_scene_t *scene, CefRefPtr<CefValue> &result)
{
	SerializeObsScene(obs_scene_get_source(scene), result);
}

///////////////////////////////////////////////////////////////////////

static void dispatch_scene_event(obs_scene_t *scene,
				 std::string currentSceneEventName,
				 std::string otherSceneEventName)
{
	if (s_shutdown)
		return;

	//obs_scene_get_ref(scene);

	//QtPostTask([scene, currentSceneEventName, otherSceneEventName]() {
	CefRefPtr<CefValue> item = CefValue::Create();

	SerializeObsScene(scene, item);

	auto json = CefWriteJSON(item, JSON_WRITER_DEFAULT);

	if (is_active_scene(scene)) {
		DispatchJSEventGlobal(currentSceneEventName, json);
	}

	DispatchJSEventGlobal(
		otherSceneEventName, json);

	//obs_scene_release(scene);
	//});
}

static void dispatch_scene_event(obs_source_t *source,
				 std::string currentSceneEventName,
				 std::string otherSceneEventName)
{
	if (s_shutdown)
		return;

	obs_scene_t *scene = obs_scene_from_source(source);

	dispatch_scene_event(scene, currentSceneEventName, otherSceneEventName);
}

static void dispatch_scene_event(void *my_data, calldata_t *cd,
				 std::string currentSceneEventName,
				 std::string otherSceneEventName)
{
	if (s_shutdown)
		return;

	obs_scene_t *scene = (obs_scene_t *)calldata_ptr(cd, "scene");

	if (!scene) {
		obs_source_t *source =
			(obs_source_t *)calldata_ptr(cd, "source");

		if (!obs_source_is_scene(source))
			return;

		scene = obs_scene_from_source(source);
	}

	if (!scene)
		return;

	dispatch_scene_event(scene, currentSceneEventName, otherSceneEventName);
}

static void dispatch_scene_update(obs_scene_t* scene,
				  bool shouldDelay,
				  SESignalHandlerData* signalHandlerData)
{
	if (s_shutdown)
		return;

	if (!scene)
		return;

	if (!signalHandlerData)
		return;

	if (shouldDelay) {
		auto sceneRef = obs_scene_get_ref(scene);

		std::thread thread([=]() {
			dispatch_scene_event(sceneRef,
					     "hostActiveSceneItemListChanged",
					     "hostSceneItemListChanged");

			obs_scene_release(sceneRef);
		});

		thread.detach();
	} else {
		dispatch_scene_event(scene, "hostActiveSceneItemListChanged",
				     "hostSceneItemListChanged");
	}
}

static void dispatch_scene_update(void *my_data, calldata_t *cd,
					  bool shouldDelay)
{
	if (s_shutdown)
		return;

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData) {
		obs_scene_t *scene = (obs_scene_t *)calldata_ptr(cd, "scene");

		if (!scene)
			return;

		dispatch_scene_update(scene, shouldDelay, nullptr);
	} else {
		OBSSceneAutoRelease scene =
			signalHandlerData->GetRootSceneRef();

		if (!scene)
			return;

		dispatch_scene_update(scene, shouldDelay, signalHandlerData);
	}
}

static void dispatch_scene_update(void* my_data, calldata_t* cd) {
	dispatch_scene_update(my_data, cd, false);
}

static void dispatch_sceneitem_event(void *my_data, obs_sceneitem_t *sceneitem,
				     std::string eventName,
				     bool serializeDetails = true)
{
	if (s_shutdown)
		return;

	if (!sceneitem)
		return;

	if (sceneitem) {
		//obs_sceneitem_addref(sceneitem);

		//QtPostTask([sceneitem, eventName, serializeDetails]() {
		CefRefPtr<CefValue> item = CefValue::Create();

		obs_source_t *sceneitem_source =
			obs_sceneitem_get_source(sceneitem);

		// this can deadlock due to full_lock(obs_scene) in obs_sceneitem_get_group
		SerializeSourceAndSceneItem(
			item, obs_sceneitem_get_scene(sceneitem),
			sceneitem_source, sceneitem, -1, serializeDetails,
			false,
			my_data ? ((SESignalHandlerData *)my_data)
					  ->m_videoCompositionBase
				: nullptr);

		std::string json =
			CefWriteJSON(item, JSON_WRITER_DEFAULT).ToString();

		DispatchJSEventGlobal(eventName, json);

		//obs_sceneitem_release(sceneitem);
		//});
	} else {
		DispatchJSEventGlobal(eventName, "null");
	}
}

static void dispatch_sceneitem_event(void *my_data, obs_sceneitem_t *sceneitem,
				     std::string currentSceneEventName,
				     std::string otherSceneEventName,
				     bool serializeDetails = true)
{
	if (s_shutdown)
		return;

	if (!sceneitem)
		return;

	//obs_sceneitem_addref(sceneitem);

	//QtPostTask([sceneitem, currentSceneEventName, otherSceneEventName,
	//	    serializeDetails]() {
	if (is_active_scene(sceneitem)) {
		dispatch_sceneitem_event(my_data, sceneitem,
					 currentSceneEventName,
					 serializeDetails);
	}

	//obs_sceneitem_release(sceneitem);

	dispatch_sceneitem_event(my_data, sceneitem, otherSceneEventName,
				 serializeDetails);
	//});
}

static void dispatch_sceneitem_event(void *my_data, calldata_t *cd,
				     std::string currentSceneEventName,
				     std::string otherSceneEventName,
				     bool serializeDetails = true, bool shouldDelay = false)
{
	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	if (shouldDelay) {
		obs_sceneitem_addref(sceneitem);

		
		signalHandlerData->Lock();

		std::thread thread([=]() -> void {
			dispatch_sceneitem_event(my_data, sceneitem,
						 currentSceneEventName,
						 otherSceneEventName,
						 serializeDetails);

			obs_sceneitem_release(sceneitem);

			signalHandlerData->Unlock();
		});

		thread.detach();
	} else {
		dispatch_sceneitem_event(my_data, sceneitem,
					 currentSceneEventName,
					 otherSceneEventName, serializeDetails);
	}
}

static void dispatch_source_event(void *my_data, calldata_t *cd,
				  std::string currentSceneEventName,
				  std::string otherSceneEventName)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (!source)
		return;

	obs_source_t *scene_source =
		obs_frontend_get_current_scene(); // adds ref

	if (!scene_source)
		return;

	// Get scene handle
	obs_scene_t *scene = obs_scene_from_source(
		scene_source); // does not increment refcount

	if (!scene) {
		obs_source_release(scene_source);

		return;
	}

	//obs_source_get_ref(source);

	//QtPostTask([scene, scene_source, source, currentSceneEventName,
	//	    otherSceneEventName]() {
	// For each scene item
	ObsSceneEnumAllItems(scene, [&](obs_sceneitem_t *sceneitem) {
		obs_source_t *sceneitem_source = obs_sceneitem_get_source(
			sceneitem); // does not increase refcount

		if (sceneitem_source == source) {
			dispatch_sceneitem_event(my_data, sceneitem,
						 currentSceneEventName,
						 otherSceneEventName, false);
		}

		return true;
	});

	//obs_source_release(source);
	obs_source_release(scene_source);
	//});
}

static void handle_scene_rename(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_scene_event(my_data, cd, "hostActiveSceneRenamed",
			      "hostSceneRenamed");
}

static void handle_scene_remove(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_scene_event(
		my_data, cd, "hostActiveSceneRemoved",
			      "hostSceneRemoved");
}

static void handle_scene_item_transform(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemTransformed",
				 "hostSceneItemTransformed", false, true);
	dispatch_scene_update(my_data, cd, true);
}

static void handle_scene_item_select(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	bool enabled =
		StreamElementsSceneItemsMonitor::GetSceneItemUISettingsEnabled(
			sceneitem);

	if (enabled) {
		dispatch_sceneitem_event(my_data, cd,
					 "hostActiveSceneItemSelected",
					 "hostSceneItemSelected", false);
		dispatch_scene_update(my_data, cd, true);
	} else {
		obs_sceneitem_select(sceneitem, false);
	}

	if (my_data) {
		auto sceneManager =
			((SESignalHandlerData *)my_data)->m_obsSceneManager;

		if (sceneManager)
			sceneManager->Update();
	}
}

static void handle_scene_item_deselect(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemUnselected",
				 "hostSceneItemUnselected", false);
	dispatch_scene_update(my_data, cd, true);

	if (my_data) {
		auto sceneManager =
			((SESignalHandlerData *)my_data)->m_obsSceneManager;

		if (sceneManager)
			sceneManager->Update();
	}
}

static void handle_scene_item_reorder(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_scene_event(my_data, cd, "hostActiveSceneItemsOrderChanged",
			     "hostSceneItemOrderChanged");

	dispatch_scene_update(my_data, cd, true);

	if (my_data) {
		auto sceneManager =
			((SESignalHandlerData *)my_data)->m_obsSceneManager;

		if (sceneManager)
			sceneManager->Update();
	}
}

static void handle_scene_item_source_update_props(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_source_event(my_data, cd,
			      "hostActiveSceneItemPropertiesChanged",
			      "hostSceneItemPropertiesChanged");
	dispatch_scene_update(my_data, cd, true);
}

static void handle_scene_item_source_update_settings(void *my_data,
						     calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_source_event(my_data, cd, "hostActiveSceneItemSettingsChanged",
			      "hostSceneItemSettingsChanged");
	dispatch_scene_update(my_data, cd, true);
}

static void handle_scene_item_source_rename(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	dispatch_source_event(my_data, cd, "hostActiveSceneItemRenamed",
			      "hostSceneItemRenamed");
	dispatch_scene_update(my_data, cd, true);
}

static void handle_scene_item_source_filter_update_props(void *my_data,
							 calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	OBSSceneAutoRelease scene = signalHandlerData->GetRootSceneRef();

	if (!scene)
		return;

	dispatch_scene_update(scene, true, signalHandlerData);
}

static void handle_scene_item_source_filter_update_settings(void *my_data,
							    calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	OBSSceneAutoRelease scene = signalHandlerData->GetRootSceneRef();

	if (!scene)
		return;

	dispatch_scene_update(scene, true, signalHandlerData);
}

static void handle_scene_item_source_filter_rename(void *my_data,
						   calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	OBSSceneAutoRelease scene = signalHandlerData->GetRootSceneRef();

	if (!scene)
		return;

	dispatch_scene_update(scene, true, signalHandlerData);
}

static void connect_filter_source_signals(obs_source_t *filter,
					  SESignalHandlerData *data)
{
	auto handler = obs_source_get_signal_handler(filter);

	if (!handler)
		return;

	signal_handler_connect(handler, "update",
			       handle_scene_item_source_filter_update_settings,
			       data);

	signal_handler_connect(handler, "update_properties",
			       handle_scene_item_source_filter_update_props,
			       data);

	signal_handler_connect(handler, "rename",
			       handle_scene_item_source_filter_rename, data);
}

static void disconnect_filter_source_signals(obs_source_t *filter,
					     SESignalHandlerData *data)
{
	auto handler = obs_source_get_signal_handler(filter);

	if (!handler)
		return;

	signal_handler_disconnect(
		handler, "update",
		handle_scene_item_source_filter_update_settings, data);

	signal_handler_disconnect(handler, "update_properties",
				  handle_scene_item_source_filter_update_props,
				  data);

	signal_handler_disconnect(handler, "rename",
				  handle_scene_item_source_filter_rename, data);
}

static void handle_scene_item_source_filter_add(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	// obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");
	obs_source_t *filter = (obs_source_t *)calldata_ptr(cd, "filter");

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	connect_filter_source_signals(filter, signalHandlerData);

	OBSSceneAutoRelease scene = signalHandlerData->GetRootSceneRef();

	if (!scene)
		return;

	dispatch_scene_update(scene, true, signalHandlerData);
}

static void handle_scene_item_source_filter_remove(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (obs_source_removed(source)) {
		//
		// Do not process filter_removed signals for removed sources.
		// 
		// This applies to the case where OBS has removed it's scenes
		// during a scene collection change, and removed non-referenced
		// sources as a consequence.
		//
		return;
	}

	obs_source_t *filter = (obs_source_t *)calldata_ptr(cd, "filter");

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	disconnect_filter_source_signals(filter, signalHandlerData);

	OBSSceneAutoRelease scene = signalHandlerData->GetRootSceneRef();

	if (!scene)
		return;

	dispatch_scene_update(scene, true, signalHandlerData);
}

static void handle_scene_item_source_filter_reorder(void *my_data,
						   calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	// obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	OBSSceneAutoRelease scene = signalHandlerData->GetRootSceneRef();

	if (!scene)
		return;

	dispatch_scene_update(scene, true, signalHandlerData);
}

static void add_filter_signals(obs_sceneitem_t *sceneitem, SESignalHandlerData* data)
{
	auto source = obs_sceneitem_get_source(sceneitem);

	if (!source)
		return;

	auto handler = obs_source_get_signal_handler(source);

	if (!handler)
		return;

	signal_handler_connect(handler, "filter_add",
			       handle_scene_item_source_filter_add, data);
	signal_handler_connect(handler, "filter_remove",
			       handle_scene_item_source_filter_remove, data);
	signal_handler_connect(handler, "reorder_filters",
			       handle_scene_item_source_filter_reorder, data);

	obs_source_enum_filters(
		source,
		[](obs_source_t *source, obs_source_t *filter, void *data) {
			connect_filter_source_signals(
				filter,
				static_cast<SESignalHandlerData *>(data));
		},
		data);
}

static void remove_filter_signals(obs_sceneitem_t *sceneitem,
				  SESignalHandlerData* data)
{
	auto source = obs_sceneitem_get_source(sceneitem);

	if (!source)
		return;

	auto handler = obs_source_get_signal_handler(source);

	if (!handler)
		return;

	signal_handler_disconnect(handler, "filter_add",
				  handle_scene_item_source_filter_add, data);
	signal_handler_disconnect(handler, "filter_remove",
				  handle_scene_item_source_filter_remove, data);
	signal_handler_disconnect(handler, "reorder_filters",
				  handle_scene_item_source_filter_reorder, data);

	obs_source_enum_filters(
		source,
		[](obs_source_t *source, obs_source_t *filter, void *data) {
			disconnect_filter_source_signals(
				filter,
				static_cast<SESignalHandlerData *>(data));
		},
		data);
}

static void handle_scene_item_add(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	if (!sceneitem)
		return;

	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemAdded",
				 "hostSceneItemAdded", false);
	dispatch_scene_update(my_data, cd, true);

	auto source = obs_sceneitem_get_source(sceneitem);

	if (source) {
		add_source_signals(source,
				   static_cast<SESignalHandlerData *>(my_data));
	}

	add_filter_signals(sceneitem,
			   static_cast<SESignalHandlerData *>(my_data));

	if (!obs_sceneitem_is_group(sceneitem))
		return;

	obs_scene_t *group_scene = obs_sceneitem_group_get_scene(sceneitem);

	add_scene_signals(group_scene,
			  static_cast<SESignalHandlerData *>(my_data));

	if (my_data) {
		auto sceneManager =
			static_cast<SESignalHandlerData *>(my_data)->m_obsSceneManager;

		if (sceneManager)
			sceneManager->Update();
	}
}

static void process_scene_item_remove(obs_sceneitem_t *sceneitem,
				      SESignalHandlerData *signalHandlerData,
				      bool dispatchEvents)
{
	if (!sceneitem)
		return;

	if (!signalHandlerData)
		return;

	remove_filter_signals(sceneitem, signalHandlerData);

	auto source = obs_sceneitem_get_source(sceneitem);

	if (source) {
		remove_source_signals(source, signalHandlerData);
	}

	if (dispatchEvents) {
		OBSSceneAutoRelease sceneRef = signalHandlerData->GetRootSceneRef();

		dispatch_sceneitem_event(signalHandlerData, sceneitem,
					 "hostActiveSceneItemRemoved",
					 "hostSceneItemRemoved", false);
		dispatch_scene_update(sceneRef, true, signalHandlerData);
	}

	if (!obs_sceneitem_is_group(sceneitem))
		return;

	obs_scene_t *group_scene = obs_sceneitem_group_get_scene(sceneitem);

	remove_scene_signals(group_scene, signalHandlerData);

	if (signalHandlerData) {
		auto sceneManager = signalHandlerData->m_obsSceneManager;

		if (sceneManager)
			sceneManager->Update();
	}
}

static void handle_scene_item_remove(void *my_data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	if (!sceneitem)
		return;

	auto signalHandlerData = static_cast<SESignalHandlerData *>(my_data);

	if (!signalHandlerData)
		return;

	process_scene_item_remove(sceneitem, signalHandlerData, true);
}

void remove_source_signals(obs_source_t *source, SESignalHandlerData *data)
{
	if (!source)
		return;

	auto handler = obs_source_get_signal_handler(source);

	signal_handler_disconnect(handler, "update",
				  handle_scene_item_source_update_settings,
				  data);

	signal_handler_disconnect(handler, "update_properties",
				  handle_scene_item_source_update_props, data);

	signal_handler_disconnect(handler, "rename",
				  handle_scene_item_source_rename, data);
}

void add_source_signals(obs_source_t *source, SESignalHandlerData *data)
{
	auto handler = obs_source_get_signal_handler(source);

	signal_handler_connect(handler, "update",
			       handle_scene_item_source_update_settings, data);

	signal_handler_connect(handler, "update_properties",
			       handle_scene_item_source_update_props, data);

	signal_handler_connect(handler, "rename",
			       handle_scene_item_source_rename, data);
}

void remove_scene_signals(obs_scene_t *scene, SESignalHandlerData *data)
{
	if (!scene)
		return;

	obs_source_t *source = obs_scene_get_source(scene);

	if (!source)
		return;

	auto signalHandlerData = data->GetSceneRef(scene);

	scanSceneItems(
		scene,
		[&](obs_sceneitem_t *item,
		    obs_sceneitem_t *parent_item) -> bool {
			if (obs_sceneitem_is_group(item)) {
				scanGroupSceneItems(
					item,
					[signalHandlerData,
					 scene](obs_sceneitem_t *item,
						obs_sceneitem_t *parent_item)
						-> bool {
						process_scene_item_remove(
							item, signalHandlerData, false);

						return true;
					},
					false);
			}

			process_scene_item_remove(item, signalHandlerData,
						  false);

			return true;
		},
		false);

	auto handler = obs_source_get_signal_handler(source);

	signal_handler_disconnect(handler, "item_add", handle_scene_item_add,
				  signalHandlerData);
	signal_handler_disconnect(handler, "item_remove",
				  handle_scene_item_remove, signalHandlerData);
	signal_handler_disconnect(handler, "reorder", handle_scene_item_reorder,
				  signalHandlerData);
	signal_handler_disconnect(handler, "item_visible",
				  dispatch_scene_update, signalHandlerData);
	signal_handler_disconnect(handler, "item_locked", dispatch_scene_update,
				  signalHandlerData);
	signal_handler_disconnect(handler, "item_select",
				  handle_scene_item_select, signalHandlerData);
	signal_handler_disconnect(handler, "item_deselect",
				  handle_scene_item_deselect,
				  signalHandlerData);
	signal_handler_disconnect(handler, "item_transform",
				  handle_scene_item_transform,
				  signalHandlerData);

	signal_handler_disconnect(handler, "rename", handle_scene_rename,
				  signalHandlerData);
	signal_handler_disconnect(handler, "remove", handle_scene_remove,
				  signalHandlerData);

	remove_source_signals(source, signalHandlerData);

	data->RemoveSceneRef(scene);
}

void remove_scene_signals(obs_source_t *sceneSource, SESignalHandlerData *data)
{
	if (!sceneSource)
		return;

	if (obs_source_is_group(sceneSource))
		return;

	obs_scene_t *scene = obs_scene_from_source(sceneSource);

	remove_scene_signals(scene, data);
}

void add_scene_signals(obs_scene_t *scene, SESignalHandlerData *data)
{
	if (!scene)
		return;

	obs_source_t *source = obs_scene_get_source(scene);

	if (!source)
		return;

	auto signalHandlerData = data->AddSceneRef(scene);

	auto handler = obs_source_get_signal_handler(source);

	add_source_signals(source, signalHandlerData);

	signal_handler_connect(handler, "item_add", handle_scene_item_add,
			       signalHandlerData);
	signal_handler_connect(handler, "item_remove", handle_scene_item_remove,
			       signalHandlerData);
	signal_handler_connect(handler, "reorder", handle_scene_item_reorder,
			       signalHandlerData);
	signal_handler_connect(handler, "item_visible", dispatch_scene_update,
			       signalHandlerData);
	signal_handler_connect(handler, "item_locked", dispatch_scene_update,
			       signalHandlerData);
	signal_handler_connect(handler, "item_select", handle_scene_item_select,
			       signalHandlerData);
	signal_handler_connect(handler, "item_deselect",
			       handle_scene_item_deselect, signalHandlerData);
	signal_handler_connect(handler, "item_transform",
			       handle_scene_item_transform, signalHandlerData);

	signal_handler_connect(handler, "rename", handle_scene_rename,
			       signalHandlerData);
	signal_handler_connect(handler, "remove", handle_scene_remove,
			       signalHandlerData);

	scanSceneItems(scene,
		       [&](obs_sceneitem_t *item,
			   obs_sceneitem_t *parent_item) -> bool {
			       calldata_t *cd = calldata_create();
			       calldata_set_ptr(cd, "item", item);

			       handle_scene_item_add(signalHandlerData, cd);
			       calldata_destroy(cd);

			       return true;
		       }, true);
}

void add_scene_signals(obs_source_t *sceneSource, SESignalHandlerData *data)
{
	if (!sceneSource)
		return;

	if (obs_source_is_group(sceneSource))
		return;

	obs_scene_t *scene = obs_scene_from_source(sceneSource);

	add_scene_signals(scene, data);
}

void StreamElementsObsSceneManager::handle_obs_frontend_event(
	enum obs_frontend_event event, void *data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	if (event == OBS_FRONTEND_EVENT_EXIT || event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		s_shutdown = true;
	}

	StreamElementsObsSceneManager *self =
		(StreamElementsObsSceneManager *)data;

	if (event == OBS_FRONTEND_EVENT_SCENE_COLLECTION_CLEANUP ||
	    event == OBS_FRONTEND_EVENT_SCRIPTING_SHUTDOWN) {
		self->CleanObsSceneSignals();
	}

	if (event != OBS_FRONTEND_EVENT_SCENE_CHANGED &&
	    event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
		return;

	if (self->m_sceneItemsMonitor)
		self->m_sceneItemsMonitor->Update();

	if (self->m_scenesWidgetManager)
		self->m_scenesWidgetManager->Update();
}

static void handle_source_create(void *data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (!source)
		return;

	add_scene_signals(source, (SESignalHandlerData *)data);
}

static void handle_source_remove(void *data, calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (!source)
		return;

	remove_scene_signals(source, (SESignalHandlerData *)data);
}

///////////////////////////////////////////////////////////////////////

static std::shared_ptr<StreamElementsVideoCompositionBase>
GetVideoComposition(CefRefPtr<CefValue> input)
{
	auto videoCompositionManager =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager();

	if (!videoCompositionManager.get())
		return std::shared_ptr<StreamElementsVideoCompositionBase>(
			nullptr);

	auto result = videoCompositionManager->GetObsNativeVideoComposition();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return result;

	auto root = input->GetDictionary();

	if (!root->HasKey("videoCompositionId") ||
	    root->GetType("videoCompositionId") != VTYPE_STRING)
		return result;

	std::string videoCompositionId = root->GetString("videoCompositionId");

	result = videoCompositionManager->GetVideoCompositionById(
		videoCompositionId);

	return result;
}

///////////////////////////////////////////////////////////////////////

StreamElementsObsSceneManager::StreamElementsObsSceneManager(QMainWindow *parent)
	: m_parent(parent)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	m_sceneItemsMonitor = new StreamElementsSceneItemsMonitor(m_parent);
	m_scenesWidgetManager =
		new StreamElementsScenesListWidgetManager(m_parent);

	m_signalHandlerData = new SESignalHandlerData(
		this, StreamElementsGlobalStateManager::GetInstance()
			      ->GetVideoCompositionManager()
			      ->GetObsNativeVideoComposition()
			      .get());

	obs_frontend_add_event_callback(handle_obs_frontend_event, this);

	auto handler = obs_get_signal_handler();

	signal_handler_connect(handler, "source_create", handle_source_create,
			       m_signalHandlerData);

	signal_handler_connect(handler, "source_remove", handle_source_remove,
			       m_signalHandlerData);

	// Add signals to existing scenes
	obs_enum_scenes(
		[](void * data, obs_source_t *scene_source) -> bool {
			auto scene = obs_scene_from_source(scene_source);

			add_scene_signals(
				scene_source,
				static_cast<SESignalHandlerData *>(data));

			return true;
		},
		m_signalHandlerData);

	CefRefPtr<CefValue> dummy1 = CefValue::Create();
	DeserializeSceneItemsAuxiliaryActions(
		CefParseJSON(StreamElementsConfig::GetInstance()
				     ->GetSceneItemsAuxActionsConfig(),
			     JSON_PARSER_ALLOW_TRAILING_COMMAS),
		dummy1);

	CefRefPtr<CefValue> dummy2 = CefValue::Create();
	DeserializeScenesAuxiliaryActions(
		CefParseJSON(StreamElementsConfig::GetInstance()
				     ->GetScenesAuxActionsConfig(),
			     JSON_PARSER_ALLOW_TRAILING_COMMAS),
		dummy2);
}

void StreamElementsObsSceneManager::CleanObsSceneSignals()
{
	std::vector<obs_scene_t *> scenes;
	m_signalHandlerData->GetScenes(scenes);

	for (auto scene : scenes) {
		auto signalHandlerData = m_signalHandlerData->GetSceneRef(scene);

		remove_scene_signals(scene, m_signalHandlerData);
	}

	m_signalHandlerData->Wait();
}

StreamElementsObsSceneManager::~StreamElementsObsSceneManager()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (m_scenesWidgetManager) {
		delete m_scenesWidgetManager;
		m_scenesWidgetManager = nullptr;
	}

	if (m_sceneItemsMonitor) {
		delete m_sceneItemsMonitor;
		m_sceneItemsMonitor = nullptr;
	}

	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);

	if (m_signalHandlerData) {
		auto handler = obs_get_signal_handler();

		signal_handler_disconnect(handler, "source_create",
					  handle_source_create,
					  m_signalHandlerData);

		signal_handler_disconnect(handler, "source_remove",
					  handle_source_remove,
					  m_signalHandlerData);

		// Remove signals from existing scenes
		CleanObsSceneSignals();

		m_signalHandlerData->Wait();
		m_signalHandlerData->Release();
		m_signalHandlerData = nullptr;
	}
}

void StreamElementsObsSceneManager::ObsAddSourceInternal(
	obs_source_t *inputParentScene, obs_sceneitem_t *parentGroup,
	const char *sourceId, const char *sourceName,
	obs_data_t *sourceSettings, obs_data_t *sourceHotkeyData,
	bool preferExistingSource, const char *existingSourceId,
	obs_source_t **output_source,
	obs_sceneitem_t **output_sceneitem,
	bool *isExistingSource)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);
	
	OBSSourceAutoRelease parentScene = nullptr;

	if (inputParentScene == NULL) {
		parentScene = obs_frontend_get_current_scene();
	} else {
		parentScene = obs_source_get_ref(inputParentScene);
	}

	OBSSourceAutoRelease source = NULL;

	if (existingSourceId && !source) {
		// Look up existing source by ID

		struct enum_sources_args {
			std::string id;
			obs_source_t *result;
		};

		enum_sources_args enum_args = {};
		enum_args.id = existingSourceId;
		enum_args.result = NULL;

		auto process = [](void *arg, obs_source_t *iterator) {
			enum_sources_args *args = (enum_sources_args *)arg;

			if (GetIdFromPointer((void *)iterator) == args->id &&
			    !args->result) {
				args->result = obs_source_get_ref(iterator);

				return false;
			}

			return true;
		};

		obs_enum_sources(process, &enum_args);
		obs_enum_scenes(process, &enum_args);

		source = enum_args.result;
	}

	if (preferExistingSource && !source) {
		// Try locating existing source of the same type for reuse
		//
		// This is especially relevant for video capture sources
		//

		struct enum_sources_args {
			const char *id;
			obs_source_t *result;
		};

		enum_sources_args enum_args = {};
		enum_args.id = sourceId;
		enum_args.result = NULL;

		obs_enum_sources(
			[](void *arg, obs_source_t *iterator) {
				enum_sources_args *args =
					(enum_sources_args *)arg;

				const char *id = obs_source_get_id(iterator);

				if (strcmp(id, args->id) == 0) {
					args->result =
						obs_source_get_ref(iterator);

					return false;
				}

				return true;
			},
			&enum_args);

		source = enum_args.result;
	}

	if (source == NULL) {
		// Not reusing an existing source, create a new one
		source = obs_source_create(sourceId, sourceName, NULL,
					   sourceHotkeyData);

		obs_source_update(source, sourceSettings);
	} else {
		*isExistingSource = true;
	}

	/*
	if (strcmp(sourceId, "game_capture") != 0) {
		// Wait for dimensions: some sources like video capture source do not
		// get their dimensions immediately: they are initializing asynchronously
		// and are not aware of the source dimensions until they do.
		//
		// We'll do this for maximum 15 seconds and give up.
		for (int i = 0; i < 150 && obs_source_get_width(source) == 0;
		     ++i) {
			os_sleep_ms(100);
		}
	}
	*/

	// Does not increment refcount. No obs_scene_release() call is necessary.
	obs_scene_t *scene = obs_scene_from_source(parentScene);

	struct atomic_update_args {
		obs_source_t *source;
		obs_sceneitem_t *sceneitem;
		obs_sceneitem_t *group;
	};

	atomic_update_args args = {};

	args.source = source;
	args.sceneitem = NULL;
	args.group = parentGroup;

	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *scene) {
			atomic_update_args *args = (atomic_update_args *)data;

			args->sceneitem = obs_scene_add(scene, args->source);

			if (args->sceneitem) {
				obs_sceneitem_addref(args->sceneitem);

				if (!!args->group &&
				    obs_sceneitem_is_group(args->group)) {
					// TODO: TBD: this call causes a crash on shutdown of OBS
					//            discuss with Jim and eliminate from upcoming OBS 25
					obs_sceneitem_group_add_item(
						args->group, args->sceneitem);
				} else {
					args->group = nullptr;
				}

				obs_sceneitem_set_visible(args->sceneitem,
							  true);
			}
		},
		&args);

	if (args.sceneitem) {
		if (!!args.group) {
			RefreshObsSceneItemsList();
		}

		if (output_sceneitem != NULL) {
			*output_sceneitem = args.sceneitem;
		} else {
			obs_sceneitem_release(
				args.sceneitem); // was allocated in atomic_scene_update
		}

		if (output_source != NULL) {
			*output_source = obs_source_get_ref(source);
		}
	}
}

static bool isSourceNameUnique(obs_source_t* source, std::string name) {
	if (source) {
		OBSSourceAutoRelease existing =
			obs_get_source_by_name(name.c_str());

		if (existing && existing == source) {
			return true;
		} else if (existing) {
			return false;
		}
	}

	auto videoComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager()
			->GetVideoCompositionBySceneName(name);

	if (videoComposition.get())
		return false;

	videoComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager()
			->GetVideoCompositionBySceneItemName(name, nullptr);

	if (videoComposition.get())
		return false;

	return true;
}

std::string
StreamElementsObsSceneManager::ObsSetUniqueSourceName(obs_source_t *source,
						      std::string name)
{
	if (!isSourceNameUnique(source, name)) {
		std::string unique = ObsGetUniqueSourceName(name);

		obs_source_set_name(source, unique.c_str());

		return unique;
	} else {
		obs_source_set_name(source, name.c_str());

		return name;
	}
}

std::string
StreamElementsObsSceneManager::ObsGetUniqueSourceName(std::string name)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string result(name);

	int sequence = 0;

	while (!isSourceNameUnique(nullptr, result)) {
		++sequence;

		char buf[32];
		sprintf(buf, "%d", sequence);
		result = name + " ";
		result += buf;
	}

	return result;
}

void StreamElementsObsSceneManager::DeserializeObsBrowserSource(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsBrowserSourceSceneItemInfoValid(input)) {
		return;
	}

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	// Get parameter values

	std::string unique_name =
		ObsGetUniqueSourceName(root->GetString("name").ToString());
	std::string source_class = "browser_source";

	std::string groupId = root->HasKey("parentId") &&
					      root->GetType("parentId") ==
						      VTYPE_STRING
				      ? root->GetString("parentId").ToString()
				      : "";

	std::string sceneId = root->HasKey("sceneId") &&
					      root->GetType("sceneId") ==
						      VTYPE_STRING
				      ? root->GetString("sceneId").ToString()
				      : "";

	OBSDataAutoRelease settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeObsData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		// Add browser source

		OBSSceneAutoRelease parent_scene =
			videoComposition->GetSceneByIdRef(sceneId);

		obs_source_t *parent_scene_source = obs_scene_get_source(parent_scene); // no addref

		obs_source_t *source = nullptr;
		obs_sceneitem_t *sceneitem = nullptr;

		bool isExistingSource = false;

		ObsAddSourceInternal(
			parent_scene_source,
			videoComposition->GetSceneItemById(groupId),
			source_class.c_str(), unique_name.c_str(), settings,
			nullptr, false, nullptr, &source, &sceneitem,
			&isExistingSource);

		if (sceneitem) {
			obs_transform_info info;
			obs_sceneitem_crop crop;

			if (DeserializeSceneItemComposition(input, info,
							    crop)) {
				obs_sceneitem_set_info(sceneitem, &info);
				obs_sceneitem_set_crop(sceneitem, &crop);
			}

			DeserializeAuxiliaryObsSceneItemProperties(
				videoComposition, sceneitem, root);

			if (root->HasKey("filters") && !isExistingSource) {
				if (!DeserializeObsSourceFilters(
					    source,
					    root->GetValue("filters"))) {
					blog(LOG_ERROR,
					     "[obs-streamelements-core]: DeserializeObsBrowserSource: failed parsing filters for source '%s': %s",
					     obs_source_get_name(source),
					     CefWriteJSON(
						     root->GetValue("filters"),
						     JSON_WRITER_DEFAULT)
						     .ToString()
						     .c_str());
				}
			}

			// Result
			SerializeSourceAndSceneItem(
				output, obs_sceneitem_get_scene(sceneitem),
				source, sceneitem, true, false,
				videoComposition.get());

			obs_sceneitem_release(sceneitem);
		}

		obs_source_release(source);
	}
}

void StreamElementsObsSceneManager::DeserializeObsGameCaptureSource(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, false, false))
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	// Get parameter values

	std::string unique_name =
		ObsGetUniqueSourceName(root->GetString("name").ToString());
	std::string source_class = "game_capture";

	std::string groupId = root->HasKey("parentId") &&
					      root->GetType("parentId") ==
						      VTYPE_STRING
				      ? root->GetString("parentId").ToString()
				      : "";

	std::string sceneId = root->HasKey("sceneId") &&
					      root->GetType("sceneId") ==
						      VTYPE_STRING
				      ? root->GetString("sceneId").ToString()
				      : "";

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeObsData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		// Add game capture source

		OBSSceneAutoRelease parent_scene =
			videoComposition->GetSceneByIdRef(sceneId);
		obs_source_t *parent_scene_source = obs_scene_get_source(
			parent_scene);

		obs_source_t *source = nullptr;
		obs_sceneitem_t *sceneitem = nullptr;

		bool isExistingSource = false;

		ObsAddSourceInternal(
			parent_scene_source,
			videoComposition->GetSceneItemById(groupId),
			source_class.c_str(), unique_name.c_str(), settings,
			nullptr, false, nullptr, &source, &sceneitem,
			&isExistingSource);

		//if (parent_scene) {
		//	obs_source_release(parent_scene);
		//}

		if (sceneitem) {
			obs_transform_info info;
			obs_sceneitem_crop crop;

			if (DeserializeSceneItemComposition(input, info,
							    crop)) {
				obs_sceneitem_set_info(sceneitem, &info);
				obs_sceneitem_set_crop(sceneitem, &crop);
			}

			DeserializeAuxiliaryObsSceneItemProperties(
				videoComposition, sceneitem, root);

			if (root->HasKey("filters") && !isExistingSource) {
				if (!DeserializeObsSourceFilters(
					    source,
					    root->GetValue("filters"))) {
					blog(LOG_ERROR,
					     "[obs-streamelements-core]: DeserializeObsGameCaptureSource: failed parsing filters for source '%s': %s",
					     obs_source_get_name(source),
					     CefWriteJSON(
						     root->GetValue("filters"),
						     JSON_WRITER_DEFAULT)
						     .ToString()
						     .c_str());
				}
			}

			// Result
			SerializeSourceAndSceneItem(
				output, obs_sceneitem_get_scene(sceneitem),
				source, sceneitem, true, false,
				videoComposition.get());

			obs_sceneitem_release(sceneitem);
		}

		obs_source_release(source);
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsVideoCaptureSource(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
#ifdef _WIN32
	const char *VIDEO_DEVICE_ID = "video_device_id";

	const char *sourceId = "dshow_input";
#else // APPLE / LINUX
	const char *VIDEO_DEVICE_ID = "device";

	const char *sourceId = "av_capture_input";
#endif

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, false, false))
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	// Get parameter values

	std::string unique_name =
		ObsGetUniqueSourceName(root->GetString("name").ToString());

	std::string source_class = sourceId;

	std::string groupId = root->HasKey("parentId") &&
					      root->GetType("parentId") ==
						      VTYPE_STRING
				      ? root->GetString("parentId").ToString()
				      : "";

	std::string sceneId = root->HasKey("sceneId") &&
					      root->GetType("sceneId") ==
						      VTYPE_STRING
				      ? root->GetString("sceneId").ToString()
				      : "";

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeObsData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		if (!obs_data_has_user_value(settings,
					     "synchronous_activate")) {
			/* Async activate for DirectShow freezes the capture source */
			obs_data_set_bool(settings, "synchronous_activate",
					  true);
		}

		if (!obs_data_has_user_value(settings, VIDEO_DEVICE_ID)) {
			/* No device id supplied by user, select a default value */

			obs_properties_t *props =
				obs_get_source_properties(sourceId);

			if (!!props) {
				// Set first available video_device_id value
				obs_property_t *prop_video_device_id =
					obs_properties_get(props,
							   VIDEO_DEVICE_ID);

				if (prop_video_device_id) {
					size_t count_video_device_id =
						obs_property_list_item_count(
							prop_video_device_id);

					if (count_video_device_id > 0) {
#ifdef _WIN32
						const size_t idx = 0;
#else
						const size_t idx =
							count_video_device_id -
							1;
#endif
						obs_data_set_string(
							settings,
							VIDEO_DEVICE_ID,
							obs_property_list_item_string(
								prop_video_device_id,
								idx));
					}
				}

				obs_properties_destroy(props);
			}
		}

		if (obs_data_has_user_value(settings, VIDEO_DEVICE_ID)) {
			// Add game capture source

			OBSSceneAutoRelease parent_scene =
				videoComposition->GetSceneByIdRef(sceneId);
			obs_source_t *parent_scene_source = obs_scene_get_source(
				parent_scene);

			obs_source_t *source = nullptr;
			obs_sceneitem_t *sceneitem = nullptr;

			bool isExistingSource = false;

			ObsAddSourceInternal(
				parent_scene_source, videoComposition->GetSceneItemById(groupId),
				source_class.c_str(), unique_name.c_str(),
				settings, nullptr, true, nullptr, &source,
				&sceneitem, &isExistingSource);

			//if (parent_scene) {
			//	obs_source_release(parent_scene);
			//}

			if (sceneitem) {
				obs_transform_info info;
				obs_sceneitem_crop crop;

				if (DeserializeSceneItemComposition(input, info,
								    crop)) {
					obs_sceneitem_set_info(sceneitem,
							       &info);
					obs_sceneitem_set_crop(sceneitem,
							       &crop);
				}

				DeserializeAuxiliaryObsSceneItemProperties(
					videoComposition, sceneitem, root);

				if (root->HasKey("filters") &&
				    !isExistingSource) {
					if (!DeserializeObsSourceFilters(
						    source,
						    root->GetValue("filters"))) {
						blog(LOG_ERROR,
						     "[obs-streamelements-core]: DeserializeObsVideoCaptureSource: failed parsing filters for source '%s': %s",
						     obs_source_get_name(
							     source),
						     CefWriteJSON(
							     root->GetValue(
								     "filters"),
							     JSON_WRITER_DEFAULT)
							     .ToString()
							     .c_str());
					}
				}

				// Result
				SerializeSourceAndSceneItem(
					output,
					obs_sceneitem_get_scene(sceneitem),
					source, sceneitem, true, false,
					videoComposition.get());

				obs_sceneitem_release(sceneitem);
			}

			obs_source_release(source);
		}
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsNativeSource(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, true, true))
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	// Get parameter values

	std::string unique_name =
		ObsGetUniqueSourceName(root->GetString("name").ToString());

	std::string source_class = root->GetString("class").ToString();

	std::string groupId = root->HasKey("parentId") &&
					      root->GetType("parentId") ==
						      VTYPE_STRING
				      ? root->GetString("parentId").ToString()
				      : "";

	std::string sceneId = root->HasKey("sceneId") &&
					      root->GetType("sceneId") ==
						      VTYPE_STRING
				      ? root->GetString("sceneId").ToString()
				      : "";

	bool preferExistingSourceReference =
		root->HasKey("preferExistingSourceReference") &&
				root->GetType(
					"preferExistingSourceReference") ==
					VTYPE_BOOL
			? root->GetBool("preferExistingSourceReference")
			: false;

	std::string existingSourceId =
		root->HasKey("existingSourceId") &&
				root->GetType("existingSourceId") ==
					VTYPE_STRING
			? root->GetString("existingSourceId")
			: "";

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeObsData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		// Add game capture source

		OBSSceneAutoRelease parent_scene =
			videoComposition->GetSceneByIdRef(sceneId);
		obs_source_t *parent_scene_source = obs_scene_get_source(
			parent_scene);

		obs_source_t *source = nullptr;
		obs_sceneitem_t *sceneitem = nullptr;

		bool isExistingSource = false;

		ObsAddSourceInternal(
			parent_scene_source,
			videoComposition->GetSceneItemById(groupId),
			source_class.c_str(), unique_name.c_str(), settings,
			nullptr, preferExistingSourceReference,
			existingSourceId.size() ? existingSourceId.c_str()
						: nullptr,
			&source, &sceneitem, &isExistingSource);

		//if (parent_scene) {
		//	obs_source_release(parent_scene);
		//}

		if (sceneitem) {
			obs_transform_info info;
			obs_sceneitem_crop crop;

			if (DeserializeSceneItemComposition(input, info,
							    crop)) {
				obs_sceneitem_set_info(sceneitem, &info);
				obs_sceneitem_set_crop(sceneitem, &crop);
			}

			DeserializeAuxiliaryObsSceneItemProperties(
				videoComposition, sceneitem, root);

			if (root->HasKey("filters") && !isExistingSource) {
				if (!DeserializeObsSourceFilters(
					    source,
					    root->GetValue("filters"))) {
					blog(LOG_ERROR,
					     "[obs-streamelements-core]: DeserializeObsNativeSource: failed parsing filters for source '%s': %s",
					     obs_source_get_name(source),
					     CefWriteJSON(
						     root->GetValue("filters"),
						     JSON_WRITER_DEFAULT)
						     .ToString()
						     .c_str());
				}
			}

			// Result
			SerializeSourceAndSceneItem(
				output, obs_sceneitem_get_scene(sceneitem),
				source, sceneitem, true, false,
				videoComposition.get());

			obs_sceneitem_release(sceneitem);
		}

		obs_source_release(source);
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsSceneItemGroup(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, false, false))
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	struct atomic_update_args {
		std::string unique_name;
		obs_sceneitem_t *sceneitem;
	};

	atomic_update_args args;

	args.unique_name =
		ObsGetUniqueSourceName(root->GetString("name").ToString());

	std::string sceneId = root->HasKey("sceneId") &&
					      root->GetType("sceneId") ==
						      VTYPE_STRING
				      ? root->GetString("sceneId").ToString()
				      : "";

	OBSSceneAutoRelease scene =
		videoComposition->GetSceneByIdRef(sceneId);

	if (scene) {
		obs_scene_atomic_update(
			scene,
			[](void *data, obs_scene_t *scene) {
				atomic_update_args *args =
					(atomic_update_args *)data;

				args->sceneitem = obs_scene_add_group(
					scene, args->unique_name.c_str());

				if (args->sceneitem) {
					obs_sceneitem_addref(args->sceneitem);

					obs_sceneitem_set_visible(
						args->sceneitem, true);
				}
			},
			&args);
	}

	if (args.sceneitem) {
		obs_transform_info info;
		obs_sceneitem_crop crop;

		if (DeserializeSceneItemComposition(input, info, crop)) {
			obs_sceneitem_set_info(args.sceneitem, &info);
			obs_sceneitem_set_crop(args.sceneitem, &crop);
		}

		DeserializeAuxiliaryObsSceneItemProperties(
			videoComposition, args.sceneitem, root);

		// Result
		SerializeSourceAndSceneItem(
			output, obs_sceneitem_get_scene(args.sceneitem),
			obs_sceneitem_get_source(args.sceneitem),
			args.sceneitem, true, false, videoComposition.get());

		//obs_sceneitem_release(args.sceneitem); // obs_scene_add_group() does not return an incremented reference
	}

	RefreshObsSceneItemsList();
}

void StreamElementsObsSceneManager::SerializeObsSceneItems(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output,
	bool serializeProperties)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	OBSSceneAutoRelease scene = nullptr;

	if (input.get() && input->GetType() == VTYPE_DICTIONARY) {
		auto root = input->GetDictionary();

		if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
			return;

		// Get scene handle
		scene = videoComposition->GetSceneByIdRef(root->GetString("id"));
	} else {
		scene = videoComposition->GetCurrentSceneRef();
	}

	if (scene) {
		struct local_context {
			CefRefPtr<CefListValue> list;
			decltype(videoComposition) videoComposition;
			std::vector<OBSSceneItemAutoRelease> sceneItems;
		};

		local_context context;

		context.list = CefListValue::Create();
		context.videoComposition = videoComposition;	

		// For each scene item
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
			   void *param) {
				local_context *context = (local_context *)param;

				obs_sceneitem_addref(sceneitem); // Will be auto-released

				context->sceneItems.push_back(sceneitem);

				// Continue iteration
				return true;
			},
			&context);

		for (auto it = context.sceneItems.cbegin();
		     it != context.sceneItems.cend(); ++it) {
			obs_source_t *source = obs_sceneitem_get_source(
				*it); // does not increase refcount

			CefRefPtr<CefValue> item = CefValue::Create();

			SerializeSourceAndSceneItem(
				item, scene, source, *it,
				context.list->GetSize(), true,
				serializeProperties,
				context.videoComposition.get());

			context.list->SetValue(context.list->GetSize(), item);
		}

		output->SetList(context.list);
	}
}

void StreamElementsObsSceneManager::SerializeObsCurrentScene(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	OBSSceneAutoRelease scene = videoComposition->GetCurrentSceneRef();

	if (scene) {
		videoComposition->SerializeScene(scene, output);
	} else {
		output->SetNull();
	}
}

void StreamElementsObsSceneManager::SerializeObsScenes(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefListValue> list = CefListValue::Create();

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	StreamElementsVideoCompositionBase::scenes_t scenes;
	videoComposition->GetAllScenes(scenes);

	for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
		/* Get the scene (a scene is a source) */
		CefRefPtr<CefValue> item = CefValue::Create();

		videoComposition->SerializeScene(*it, item);

		list->SetValue(list->GetSize(), item);
	}

	output->SetList(list);
}

std::string
StreamElementsObsSceneManager::ObsGetUniqueSceneCollectionName(std::string name)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string result(name);

	char **names = obs_frontend_get_scene_collections();

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique) {
		isUnique = true;

		for (size_t idx = 0; names[idx] && isUnique; ++idx) {
			if (strcasecmp(result.c_str(), names[idx]) == 0)
				isUnique = false;
		}

		if (!isUnique) {
			++sequence;

			char buf[32];
			sprintf(buf, "%d", sequence);
			result = name + " ";
			result += buf;
		}
	}

	bfree(names);

	return result;
}

void StreamElementsObsSceneManager::DeserializeObsScene(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("name"))
		return;

	bool activate = true;

	if (d->HasKey("active") && d->GetType("active") == VTYPE_BOOL &&
	    !d->GetBool("active"))
		activate = false;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	obs_scene_t *scene = videoComposition->AddScene(d->GetString("name"));

	if (activate) {
		videoComposition->SetCurrentScene(scene);
	}

	SerializeObsScene(scene, output);

	obs_scene_release(scene);
}

void StreamElementsObsSceneManager::SetCurrentObsSceneById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	if (!id.size())
		return;

	auto videoCompositionManager =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager();

	if (!videoCompositionManager.get())
		return;

	auto videoComposition =
		videoCompositionManager->GetVideoCompositionBySceneId(id);

	if (!videoComposition.get())
		return;

	OBSSceneAutoRelease scene = videoComposition->GetSceneByIdRef(id);

	if (!scene)
		return;

	if (!videoComposition->SetCurrentScene(scene))
		return;

	videoComposition->SerializeScene(scene, output);
}

void StreamElementsObsSceneManager::RemoveObsScenesByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list = input->GetList();

	if (!list->GetSize())
		return;

	std::unordered_map<const void *, std::string> pointer_to_id_map;

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_STRING)
			continue;

		std::string id = list->GetString(index);

		auto videoComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetVideoCompositionBySceneId(id);

		if (!videoComposition.get())
			continue;

		OBSSceneAutoRelease scene = videoComposition->GetSceneByIdRef(id);

		if (!scene)
			continue;

		if (videoComposition->SafeRemoveScene(scene)) {
			output->SetBool(true);
		}
	}
}

void StreamElementsObsSceneManager::SetObsScenePropertiesById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id"))
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	std::string id = d->GetString("id");
	OBSSceneAutoRelease scene = videoComposition->GetSceneByIdRef(id);
	if (!scene)
		return;

	bool result = false;

	obs_source_t *source = obs_scene_get_source(scene);

	if (d->HasKey("name")) {
		ObsSetUniqueSourceName(
			source,
			d->GetString("name").ToString());

		result = true;
	}

	#if SE_ENABLE_SCENE_ICONS
	if (d->HasKey("icon")) {
		m_scenesWidgetManager->SetSceneIcon(
			scene, d->GetValue("icon")->Copy());
	}
	#endif

	#if SE_ENABLE_SCENE_DEFAULT_ACTION
	if (d->HasKey("defaultAction")) {
		m_scenesWidgetManager->SetSceneDefaultAction(
			scene,
			d->GetValue("defaultAction")->Copy());
	}
	#endif

	#if SE_ENABLE_SCENE_CONTEXT_MENU
	if (d->HasKey("contextMenu")) {
		m_scenesWidgetManager->SetSceneContextMenu(
			scene,
			d->GetValue("contextMenu")->Copy());
	}
	#endif

	if (d->HasKey("auxiliaryData")) {
		m_scenesWidgetManager->SetSceneAuxiliaryData(
			source,
			d->GetValue("auxiliaryData")->Copy());
	}

	#if SE_ENABLE_SCENE_ICONS
	// This one is required, otherwise icon is not always reset
	QApplication::processEvents();
	#endif

	output->SetBool(result);
}

void StreamElementsObsSceneManager::RemoveObsSceneItemsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (!input.get() || input->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list = input->GetList();

	if (!list->GetSize())
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::list<obs_sceneitem_t *> scene_items_to_remove;

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_STRING)
			continue;

		std::string id = list->GetString(index);

		auto item =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetSceneItemById(id, true);

		if (item) {
			scene_items_to_remove.push_back(item);
		}
	}

	output->SetBool(true);

	for (obs_sceneitem_t *sceneitem : scene_items_to_remove) {
		/* Remove the scene item */
		obs_sceneitem_remove(sceneitem);
		obs_sceneitem_release(sceneitem);
	}
}

void StreamElementsObsSceneManager::DeserializeAuxiliaryObsSceneItemProperties(
	std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition,
	obs_sceneitem_t *sceneitem, CefRefPtr<CefDictionaryValue> d)
{
	#if SE_ENABLE_SCENEITEM_ACTIONS
	if (d->HasKey("actions")) {
		if (d->GetType("actions") == VTYPE_LIST) {
			CefRefPtr<CefListValue> actionsList =
				d->GetList("actions");

			m_sceneItemsMonitor->SetSceneItemActions(
				sceneitem, actionsList);
		} else {
			CefRefPtr<CefListValue> emptyList =
				CefListValue::Create();

			m_sceneItemsMonitor->SetSceneItemActions(
				sceneitem, emptyList);
		}
	}
	#endif

	#if SE_ENABLE_SCENEITEM_ICONS
	if (d->HasKey("icon")) {
		m_sceneItemsMonitor->SetSceneItemIcon(
			sceneitem, d->GetValue("icon")->Copy());
	}
	#endif

	#if SE_ENABLE_SCENEITEM_DEFAULT_ACTION
	if (d->HasKey("defaultAction")) {
		m_sceneItemsMonitor->SetSceneItemDefaultAction(
			sceneitem,
			d->GetValue("defaultAction")->Copy());
	}
	#endif

	if (d->HasKey("auxiliaryData")) {
		m_sceneItemsMonitor->SetSceneItemAuxiliaryData(
			sceneitem,
			d->GetValue("auxiliaryData")->Copy());
	}

	#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
	if (d->HasKey("contextMenu")) {
		m_sceneItemsMonitor->SetSceneItemContextMenu(
			sceneitem, d->GetValue("contextMenu")->Copy());
	}
	#endif

	#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
	if (d->HasKey("uiSettings")) {
		m_sceneItemsMonitor->SetSceneItemUISettings(
			sceneitem, d->GetValue("uiSettings")->Copy());
	}
	#endif

#if ENABLE_OBS_GROUP_ADD_REMOVE_ITEM
	std::string groupId = d->HasKey("parentId") && d->GetType("parentId") ==
							       VTYPE_STRING
				      ? d->GetString("parentId").ToString()
				      : "";

	// It seems that the implementation of
	// `obs_sceneitem_group_remove_item` is broken in
	// OBS versions older than 25.0.0, so this piece of
	// code just does not work.
	{
		struct atomic_update_args {
			obs_sceneitem_t *group;
			obs_sceneitem_t *current_group;
			obs_sceneitem_t *sceneitem;
		};

		atomic_update_args args;
		args.sceneitem = sceneitem;
		args.group = videoComposition->GetSceneItemById(groupId);
		args.current_group =
			obs_sceneitem_get_group(obs_sceneitem_get_scene(sceneitem), sceneitem);

		if (args.group != args.current_group) {
			obs_scene_atomic_update(
				obs_sceneitem_get_scene(sceneitem),
				[](void *data, obs_scene_t *scene) {
					atomic_update_args *args =
						(atomic_update_args *)data;

					obs_sceneitem_t *original_sceneitem =
						args->sceneitem;

					if (args->group) {
						obs_sceneitem_group_add_item(
							args->group,
							args->sceneitem);
					} else {
						obs_sceneitem_group_remove_item(
							args->current_group,
							args->sceneitem);
					}
				},
				&args);

			RefreshObsSceneItemsList();
		}
	}
#endif

	if (d->HasKey("order") && d->GetType("order") == VTYPE_INT) {
		int order = d->GetInt("order");

		auto scene = obs_sceneitem_get_scene(sceneitem);

		int itemsCount = 0;
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *, obs_sceneitem_t *,
			   void *data) -> bool {
				auto itemsCountPtr = static_cast<int *>(data);

				++(*itemsCountPtr);

				return (*itemsCountPtr) < 2;
			},
			&itemsCount);

		if (itemsCount > 1) {
			// We need the IF case due to a bug in OBS whcih should be fixed by this PR:
			// https://github.com/obsproject/obs-studio/pull/11985
			//
			// TL;DR: When setting non-zero order on the first and only scene item in a scene
			//        libobs crashes
			//
			obs_sceneitem_set_order_position(sceneitem, order);
		}
	}

	if (d->HasKey("visible") && d->GetType("visible") == VTYPE_BOOL) {
		obs_sceneitem_set_visible(sceneitem, d->GetBool("visible"));
	}

	if (d->HasKey("selected") && d->GetType("selected") == VTYPE_BOOL) {
		obs_sceneitem_select(sceneitem, d->GetBool("selected"));
	}

	if (d->HasKey("locked") && d->GetType("locked") == VTYPE_BOOL) {
		obs_sceneitem_set_locked(sceneitem, d->GetBool("locked"));
	}
}

void StreamElementsObsSceneManager::SetObsSceneItemPropertiesById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
		return;

	std::string id = d->GetString("id");

	obs_sceneitem_t *sceneitem = nullptr;

	sceneitem = videoComposition->GetSceneItemById(id);

	if (!sceneitem)
		return;

	bool result = false;

	obs_source_t *source = obs_sceneitem_get_source(
		sceneitem); // does not increment refcount

	if (d->HasKey("name")) {
		ObsSetUniqueSourceName(
			source,
			d->GetString("name").ToString());

		result = true;
	}

	if (d->HasKey("settings")) {
		obs_data_t *settings = obs_data_create();

		bool parsed = DeserializeObsData(d->GetValue("settings"),
						settings);

		if (parsed) {
			obs_source_update(source, settings);

			result = true;
		}

		obs_data_release(settings);
	}

	obs_transform_info info;
	obs_sceneitem_crop crop;

	if (DeserializeSceneItemComposition(input, info, crop)) {
		obs_sceneitem_set_info(sceneitem, &info);
		obs_sceneitem_set_crop(sceneitem, &crop);
	}

	DeserializeAuxiliaryObsSceneItemProperties(
		videoComposition, sceneitem, d);

	if (d->HasKey("filters")) {
		if (!DeserializeObsSourceFilters(source,
						 d->GetValue("filters"))) {
			blog(LOG_ERROR,
			     "[obs-streamelements-core]: SetObsSceneItemPropertiesById: failed parsing filters for source '%s': %s",
			     obs_source_get_name(source),
			     CefWriteJSON(d->GetValue("filters"),
					  JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}
	}

	// Result
	SerializeSourceAndSceneItem(output,
					obs_sceneitem_get_scene(sceneitem),
					source, sceneitem, true, false,
					videoComposition.get());
}

void StreamElementsObsSceneManager::GetObsSceneItemPropertiesById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
		return;

	std::string id = d->GetString("id");

	obs_sceneitem_t *sceneitem = nullptr;

	sceneitem = videoComposition->GetSceneItemById(id);

	if (!sceneitem)
		return;

	bool result = false;

	obs_source_t *source = obs_sceneitem_get_source(
		sceneitem); // does not increment refcount

	// Result
	SerializeSourceAndSceneItem(output, obs_sceneitem_get_scene(sceneitem),
				    source, sceneitem, true, true,
				    videoComposition.get());
}

void StreamElementsObsSceneManager::SerializeInputSourceClasses(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefListValue> list = CefListValue::Create();

	const char *id;
	for (size_t index = 0; obs_enum_input_types(index, &id); ++index) {
		list->SetString(list->GetSize(), id);
	}

	output->SetList(list);
}

void StreamElementsObsSceneManager::SerializeSourceClassProperties(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("class") || d->GetType("class") != VTYPE_STRING)
		return;

	std::string id = d->GetString("class").ToString();

	if (!id.size())
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	// We need all of this create-destroy dance since some 3rd party source types just do not support obs_get_source_properties :(
	obs_data_t *settings = obs_data_create();
	auto source = obs_source_create_private(
		id.c_str(), CreateGloballyUniqueIdString().c_str(), settings);
	obs_properties_t *props = obs_source_properties(source);
	obs_source_release(source);
	obs_data_release(settings);

	if (!props)
		return;

	if (d->HasKey("settings")) {
		obs_data_t *settings = obs_data_create();

		if (DeserializeObsData(d->GetValue("settings"), settings)) {
			obs_properties_apply_settings(props, settings);
		}

		obs_data_release(settings);
	}

	SerializeObsProperties(props, output);

	obs_properties_destroy(props);
}

void StreamElementsObsSceneManager::UngroupObsSceneItemsByGroupId(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_sceneitem_t *group = videoComposition->GetSceneItemById(
		d->GetString("id").ToString().c_str());

	if (!group)
		return;

	if (!obs_sceneitem_is_group(group))
		return;

	obs_sceneitem_group_ungroup(group);

	RefreshObsSceneItemsList(); // TODO: If not OBS native videoComposition, check if this should be called at all

	output->SetBool(true);
}

void StreamElementsObsSceneManager::SerializeObsSceneCollections(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefListValue> list = CefListValue::Create();

	std::map<std::string, std::string> items;
	ReadListOfObsSceneCollections(items);

	for (auto item : items) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", item.first);
		d->SetString("name", item.second);

		list->SetDictionary(list->GetSize(), d);
	}

	output->SetList(list);
}

void StreamElementsObsSceneManager::SerializeObsCurrentSceneCollection(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	char *name = obs_frontend_get_current_scene_collection();

	std::map<std::string, std::string> items;
	ReadListOfObsSceneCollections(items);

	for (auto item : items) {
		if (item.second == name) {
			CefRefPtr<CefDictionaryValue> d =
				CefDictionaryValue::Create();

			d->SetString("id", item.first);
			d->SetString("name", item.second);

			output->SetDictionary(d);

			break;
		}
	}

	bfree(name);
}

void StreamElementsObsSceneManager::DeserializeObsSceneCollection(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("name") || d->GetType("name") != VTYPE_STRING)
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string name = ObsGetUniqueSceneCollectionName(
		d->GetString("name").ToString());

	obs_frontend_add_scene_collection(name.c_str());

	SerializeObsCurrentSceneCollection(output);
}

void StreamElementsObsSceneManager::DeserializeObsCurrentSceneCollectionById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString().ToString();

	std::string actualId = "";

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::map<std::string, std::string> items;
	ReadListOfObsSceneCollections(items);

	for (auto item : items) {
		if (strcasecmp(item.second.c_str(), id.c_str()) == 0) {
			actualId = item.second;

			break;
		} else if (strcasecmp(item.first.c_str(), id.c_str()) == 0) {
			actualId = item.second;

			break;
		}
	}

	if (actualId.size()) {
		obs_frontend_set_current_scene_collection(actualId.c_str());

		SerializeObsCurrentSceneCollection(output);
	}
}

void StreamElementsObsSceneManager::InvokeCurrentSceneItemDefaultActionById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_sceneitem_t *item =
		videoComposition->GetSceneItemById(input->GetString().ToString().c_str());

	output->SetBool(
		m_sceneItemsMonitor->InvokeCurrentSceneItemDefaultAction(item));
}

void StreamElementsObsSceneManager::InvokeCurrentSceneItemDefaultContextMenuById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	auto videoComposition = GetVideoComposition(input);
	if (!videoComposition.get())
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_sceneitem_t *item =
		videoComposition->GetSceneItemById(input->GetString().ToString().c_str());

	output->SetBool(
		m_sceneItemsMonitor->InvokeCurrentSceneItemDefaultContextMenu(
			item));
}

void StreamElementsObsSceneManager::DeserializeSceneItemsAuxiliaryActions(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (!m_sceneItemsMonitor)
		return;

	output->SetBool(
		m_sceneItemsMonitor->DeserializeSceneItemsAuxiliaryActions(
			input));

	CefRefPtr<CefValue> m_actions = CefValue::Create();
	m_sceneItemsMonitor->SerializeSceneItemsAuxiliaryActions(m_actions);

	StreamElementsConfig::GetInstance()->SetSceneItemsAuxActionsConfig(
		CefWriteJSON(m_actions, JSON_WRITER_DEFAULT));

	StreamElementsConfig::GetInstance()->SaveConfig();
}

void StreamElementsObsSceneManager::SerializeSceneItemsAuxiliaryActions(
	CefRefPtr<CefValue> &output)
{
	if (!m_sceneItemsMonitor)
		return;

	m_sceneItemsMonitor->SerializeSceneItemsAuxiliaryActions(output);
}

void StreamElementsObsSceneManager::DeserializeScenesAuxiliaryActions(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (!m_scenesWidgetManager)
		return;

	output->SetBool(
		m_scenesWidgetManager->DeserializeScenesAuxiliaryActions(
			input));

	CefRefPtr<CefValue> m_actions = CefValue::Create();
	m_scenesWidgetManager->SerializeScenesAuxiliaryActions(m_actions);

	StreamElementsConfig::GetInstance()->SetScenesAuxActionsConfig(
		CefWriteJSON(m_actions, JSON_WRITER_DEFAULT));

	StreamElementsConfig::GetInstance()->SaveConfig();
}

void StreamElementsObsSceneManager::SerializeScenesAuxiliaryActions(
	CefRefPtr<CefValue> &output)
{
	if (!m_scenesWidgetManager)
		return;

	m_scenesWidgetManager->SerializeScenesAuxiliaryActions(output);
}

void StreamElementsObsSceneManager::OpenSceneItemPropertiesById(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	auto sceneItem = StreamElementsGlobalStateManager::GetInstance()
		->GetVideoCompositionManager()
		->GetSceneItemById(id);

	if (!sceneItem)
		return;

	obs_frontend_open_source_properties(
		obs_sceneitem_get_source(sceneItem));

	output->SetBool(true);
}

void StreamElementsObsSceneManager::OpenSceneItemFiltersById(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	auto sceneItem = StreamElementsGlobalStateManager::GetInstance()
				 ->GetVideoCompositionManager()
				 ->GetSceneItemById(id);

	if (!sceneItem)
		return;

	obs_frontend_open_source_filters(obs_sceneitem_get_source(sceneItem));

	output->SetBool(true);
}

void StreamElementsObsSceneManager::OpenSceneItemInteractionById(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	auto sceneItem = StreamElementsGlobalStateManager::GetInstance()
				 ->GetVideoCompositionManager()
				 ->GetSceneItemById(id);

	if (!sceneItem)
		return;

	auto source = obs_sceneitem_get_source(sceneItem);

	if (obs_source_get_output_flags(source) & OBS_SOURCE_INTERACTION) {
		obs_frontend_open_source_interaction(source);

		output->SetBool(true);
	}
}

void StreamElementsObsSceneManager::OpenSceneItemTransformEditorById(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	auto sceneItem = StreamElementsGlobalStateManager::GetInstance()
				 ->GetVideoCompositionManager()
				 ->GetSceneItemById(id);

	if (!sceneItem)
		return;

	obs_frontend_open_sceneitem_edit_transform(sceneItem);

	output->SetBool(true);
}

void StreamElementsObsSceneManager::SerializeSceneItemViewportRotation(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	std::string id = root->GetString("id");

	auto videoComposition = GetVideoComposition(input);

	if (!videoComposition)
		return;

	auto sceneItem = videoComposition->GetSceneItemById(id);

	if (!sceneItem)
		return;

	auto scene = obs_sceneitem_get_scene(sceneItem);

	if (!scene)
		return;

	auto d = CefDictionaryValue::Create();

	d->SetString("id", GetIdFromPointer(sceneItem));
	d->SetString("videoCompositionId", videoComposition->GetId());
	d->SetDouble("viewportRotationDegrees",
		     getSceneItemViewportRotationDegrees(scene, sceneItem));

	output->SetDictionary(d);
}

void StreamElementsObsSceneManager::DeserializeSceneItemViewportRotation(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	if (!root->HasKey("viewportRotationDegrees") ||
	    (root->GetType("viewportRotationDegrees") != VTYPE_INT &&
	     root->GetType("viewportRotationDegrees") != VTYPE_DOUBLE))
		return;

	std::string id = root->GetString("id");

	auto videoComposition = GetVideoComposition(input);

	if (!videoComposition)
		return;

	auto sceneItem = videoComposition->GetSceneItemById(id);

	if (!sceneItem)
		return;

	auto scene = obs_sceneitem_get_scene(sceneItem);

	if (!scene)
		return;

	auto newAngle = DeserializeIntValue(
		root->GetValue("viewportRotationDegrees"), 0);

	setSceneItemViewportRotationDegrees(scene, sceneItem, newAngle);

	SerializeSceneItemViewportRotation(input, output);
}

void StreamElementsObsSceneManager::SerializeSceneItemViewportBoundingRectangle(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	std::string id = root->GetString("id");

	auto videoComposition = GetVideoComposition(input);

	if (!videoComposition)
		return;

	auto sceneItem = videoComposition->GetSceneItemById(id);

	if (!sceneItem)
		return;

	auto scene = obs_sceneitem_get_scene(sceneItem);

	if (!scene)
		return;

	auto d = CefDictionaryValue::Create();

	d->SetString("id", GetIdFromPointer(sceneItem));
	d->SetString("videoCompositionId", videoComposition->GetId());

	vec2 tl, br;
	getSceneItemViewportBoundingBoxCoords(scene, sceneItem, &tl, &br);

	d->SetDouble("left", tl.x);
	d->SetDouble("top", tl.y);
	d->SetDouble("right", br.x);
	d->SetDouble("bottom", br.y);

	output->SetDictionary(d);
}

void StreamElementsObsSceneManager::DeserializeSceneItemViewportPosition(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	auto root = input->GetDictionary();

	if (!root->HasKey("id") || root->GetType("id") != VTYPE_STRING)
		return;

	std::string id = root->GetString("id");

	auto videoComposition = GetVideoComposition(input);

	if (!videoComposition)
		return;

	auto sceneItem = videoComposition->GetSceneItemById(id);

	if (!sceneItem)
		return;

	auto scene = obs_sceneitem_get_scene(sceneItem);

	if (!scene)
		return;

	vec2 tl, br;
	getSceneItemViewportBoundingBoxCoords(scene, sceneItem, &tl, &br);

	if (root->HasKey("left"))
		tl.x = DeserializeDoubleValue(root->GetValue("left"), tl.x);
	if (root->HasKey("top"))
		tl.y = DeserializeDoubleValue(root->GetValue("top"), tl.y);

	setSceneItemViewportBoundingBoxPosition(scene, sceneItem, tl.x, tl.y);

	SerializeSceneItemViewportBoundingRectangle(input, output);
}
