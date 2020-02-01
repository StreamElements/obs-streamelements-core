#include "StreamElementsObsSceneManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsCefClient.hpp"
#include "StreamElementsConfig.hpp"

#include <util/platform.h>

#include <unordered_map>
#include <regex>

#include <QListView>
#include <QDockWidget>
#include <QAbstractItemModel>
#include <QLayout>
#include <QHBoxLayout>
#include <QPushButton>

// TODO: TBD: When obs_sceneitem_group_add_item() and obs_sceneitem_group_remove_item()
//            are fixed, change this to 1
#define ENABLE_OBS_GROUP_ADD_REMOVE_ITEM 0

static bool s_shutdown = false;

static void sceneitem_addref(obs_sceneitem_t *sceneitem)
{
	obs_sceneitem_addref(sceneitem);

	obs_source_t *source = obs_sceneitem_get_source(sceneitem);
	if (source)
		obs_source_addref(source);
}

static void sceneitem_release(obs_sceneitem_t *sceneitem)
{
	obs_source_t *source = obs_sceneitem_get_source(sceneitem);

	if (source)
		obs_source_release(source);

	obs_sceneitem_release(sceneitem);
}

void StreamElementsObsSceneManager::RefreshObsSceneItemsList()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

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

	return;

	/*
	 * ReorderItems() refreshes the sources list only if source count differs
	 * from previous source count.
	 *
	 * To make sure source list is refreshed, we add an empty group, refresh,
	 * and the remove that group and refresh again.
	 */

	obs_source_t *currentScene = obs_frontend_get_current_scene();

	obs_scene_t *scene = obs_scene_from_source(
		currentScene); // does not increment refcount

	struct atomic_update_args {
		std::string unique_name;
		obs_source_t *source = nullptr;
		obs_sceneitem_t *sceneitem = nullptr;
	};

	atomic_update_args args;

	args.unique_name = ObsGetUniqueSourceName("____REFRESH____");

	args.sceneitem = obs_scene_add_group(scene, args.unique_name.c_str());

	QtExecSync([listView]() -> void {
		QMetaObject::invokeMethod(listView, "ReorderItems",
					  Qt::DirectConnection);
	});

	QApplication::sendPostedEvents();

	obs_sceneitem_remove(args.sceneitem);

	QtExecSync([listView]() -> void {
		QMetaObject::invokeMethod(listView, "ReorderItems",
					  Qt::DirectConnection);
	});

	QApplication::sendPostedEvents();

	obs_source_release(currentScene);
}

static obs_sceneitem_t *FindSceneItemById(std::string id)
{
	if (!id.size())
		return nullptr;

	struct local_context {
		const void *searchPtr = nullptr;
		obs_sceneitem_t *sceneitem = nullptr;
	};

	local_context context;

	context.searchPtr = GetPointerFromId(id.c_str());

	if (!context.searchPtr)
		return nullptr;

	obs_source_t *currentScene = obs_frontend_get_current_scene();

	obs_scene_t *scene = obs_scene_from_source(
		currentScene); // does not increment refcount

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
		   void *param) {
			local_context *context = (local_context *)param;

			//obs_source_t *source = obs_sceneitem_get_source(
			//	sceneitem); // does not increment refcount

			if (context->searchPtr == sceneitem) {
				context->sceneitem = sceneitem;

				/* Found what we're looking for, stop iteration */
				return false;
			} else if (obs_sceneitem_is_group(sceneitem)) {
				/* Serialize group */

				/* Serialize group items */
				obs_sceneitem_group_enum_items(
					sceneitem,
					[](obs_scene_t *scene,
					   obs_sceneitem_t *sceneitem,
					   void *param) {
						local_context *context =
							(local_context *)param;

						if (context->searchPtr ==
						    sceneitem) {
							context->sceneitem =
								sceneitem;
						}

						/* Continue or stop iteration */
						return !context->sceneitem;
					},
					context);
			}

			/* Continue or stop iteration */
			return !context->sceneitem;
		},
		&context);

	obs_source_release(currentScene);

	return context.sceneitem;
}

static bool IsSceneItemInfoValid(CefRefPtr<CefValue> &input, bool requireClass,
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

static bool IsBrowserSourceSceneItemInfoValid(CefRefPtr<CefValue> &input)
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

static CefRefPtr<CefValue> SerializeData(obs_data_t *data)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (obs_data_item_t *item = obs_data_first(data); item;
	     obs_data_item_next(&item)) {
		enum obs_data_type type = obs_data_item_gettype(item);
		const char *name = obs_data_item_get_name(item);

		if (type == OBS_DATA_STRING)
			d->SetString(name, obs_data_item_get_string(item));
		else if (type == OBS_DATA_NUMBER) {
			if (obs_data_item_numtype(item) == OBS_DATA_NUM_INT) {
				d->SetInt(name, obs_data_item_get_int(item));
			} else if (obs_data_item_numtype(item) ==
				   OBS_DATA_NUM_DOUBLE) {
				d->SetDouble(name,
					     obs_data_item_get_double(item));
			}
		} else if (type == OBS_DATA_BOOLEAN)
			d->SetBool(name, obs_data_item_get_bool(item));
		else if (type == OBS_DATA_OBJECT) {
			obs_data_t *obj = obs_data_item_get_obj(item);
			d->SetValue(name, SerializeData(obj));
			obs_data_release(obj);
		} else if (type == OBS_DATA_ARRAY) {
			CefRefPtr<CefListValue> list = CefListValue::Create();

			obs_data_array_t *array = obs_data_item_get_array(item);
			size_t count = obs_data_array_count(array);

			for (size_t idx = 0; idx < count; idx++) {
				obs_data_t *sub_item =
					obs_data_array_item(array, idx);

				list->SetValue(list->GetSize(),
					       SerializeData(sub_item));

				obs_data_release(sub_item);
			}

			obs_data_array_release(array);
		}
	}

	CefRefPtr<CefValue> result = CefValue::Create();
	result->SetDictionary(d);

	/*
	return CefParseJSON(obs_data_get_json(data),
			JSON_PARSER_ALLOW_TRAILING_COMMAS);
	*/

	return result;
}

static bool DeserializeData(CefRefPtr<CefValue> input, obs_data_t *data)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	CefDictionaryValue::KeyList keys;

	if (!d->GetKeys(keys))
		return false;

	for (std::string key : keys) {
		CefRefPtr<CefValue> v = d->GetValue(key);

		if (v->GetType() == VTYPE_STRING)
			obs_data_set_string(data, key.c_str(),
					    v->GetString().ToString().c_str());
		else if (v->GetType() == VTYPE_INT)
			obs_data_set_int(data, key.c_str(), v->GetInt());
		else if (v->GetType() == VTYPE_DOUBLE)
			obs_data_set_double(data, key.c_str(), v->GetDouble());
		else if (v->GetType() == VTYPE_BOOL)
			obs_data_set_bool(data, key.c_str(), v->GetBool());
		else if (v->GetType() == VTYPE_DICTIONARY) {
			obs_data_t *obj = obs_data_create_from_json(
				CefWriteJSON(v, JSON_WRITER_DEFAULT)
					.ToString()
					.c_str());

			obs_data_set_obj(data, key.c_str(), obj);

			obs_data_release(obj);
		} else if (v->GetType() == VTYPE_LIST) {
			CefRefPtr<CefListValue> list = v->GetList();

			obs_data_array_t *array = obs_data_array_create();

			for (size_t index = 0; index < list->GetSize();
			     ++index) {
				obs_data_t *obj = obs_data_create_from_json(
					CefWriteJSON(v, JSON_WRITER_DEFAULT)
						.ToString()
						.c_str());

				obs_data_array_push_back(array, obj);

				obs_data_release(obj);
			}

			obs_data_set_array(data, key.c_str(), array);

			obs_data_array_release(array);
		} else {
			/* Unexpected data type */
			return false;
		}
	}

	return true;
}

static CefRefPtr<CefValue> SerializeObsSourceSettings(obs_source_t *source)
{
	CefRefPtr<CefValue> result = CefValue::Create();

	if (source) {
		obs_data_t *data = obs_source_get_settings(source);

		result = SerializeData(data);

		obs_data_release(data);
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

static vec2 DeserializeVec2(CefRefPtr<CefValue> &input)
{
	vec2 result = {0};

	if (!!input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (d->HasKey("x") && d->HasKey("y")) {
			result.x = d->GetDouble("x");
			result.y = d->GetDouble("y");
		}
	}

	return result;
}

static uint32_t GetInt32FromAlignmentId(std::string alignment)
{
	uint32_t result = 0;

	if (std::regex_search(alignment, std::regex("left")))
		result |= OBS_ALIGN_LEFT;

	if (std::regex_search(alignment, std::regex("right")))
		result |= OBS_ALIGN_RIGHT;

	if (std::regex_search(alignment, std::regex("top")))
		result |= OBS_ALIGN_TOP;

	if (std::regex_search(alignment, std::regex("bottom")))
		result |= OBS_ALIGN_BOTTOM;

	return result;
}

static std::string GetAlignmentIdFromInt32(uint32_t a)
{
	std::string h = "center";
	std::string v = "center";

	if (a & OBS_ALIGN_LEFT) {
		h = "left";
	} else if (a & OBS_ALIGN_RIGHT) {
		h = "right";
	}

	if (a & OBS_ALIGN_TOP) {
		v = "top";
	} else if (a & OBS_ALIGN_BOTTOM) {
		v = "bottom";
	}

	if (h == v) {
		return "center";
	} else {
		return v + "_" + h;
	}
}

static bool DeserializeSceneItemComposition(CefRefPtr<CefValue> &input,
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
		info.rot = d->GetDouble("rotationDegrees");

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
					obs_source_t *source,
					obs_sceneitem_t *sceneitem,
					const int order = -1)
{
	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();

	root->SetString("id", GetIdFromPointer(sceneitem));
	if (source) {
		const char *name = obs_source_get_name(source);
		const char *id = obs_source_get_id(source);

		if (name)
			root->SetString("name", name);

		if (id)
			root->SetString("class", id);
	}
	root->SetBool("visible", obs_sceneitem_visible(sceneitem));
	root->SetBool("selected", obs_sceneitem_selected(sceneitem));
	root->SetBool("locked", obs_sceneitem_locked(sceneitem));

	if (order >= 0) {
		root->SetInt("order", order);
	}

	{
		obs_source_t *parent_scene = obs_frontend_get_current_scene();
		obs_scene_t *scene = obs_scene_from_source(parent_scene);

		obs_sceneitem_t *group =
			obs_sceneitem_get_group(scene, sceneitem);
		if (!!group) {
			root->SetString("parentId", GetIdFromPointer(group));
		}

		obs_source_release(parent_scene);
	}

	root->SetDictionary(
		"composition",
		SerializeObsSceneItemCompositionSettings(source, sceneitem));

	if (obs_sceneitem_is_group(sceneitem)) {
		/* Serialize group */
		struct local_context {
			CefRefPtr<CefListValue> list;
			std::vector<obs_sceneitem_t *> groupItems;
		};

		local_context context;

		context.list = CefListValue::Create();
		context.groupItems.clear();

		/* Serialize group items */
		obs_sceneitem_group_enum_items(
			sceneitem,
			[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
			   void *param) {
				local_context *context = (local_context *)param;

				obs_sceneitem_addref(
					sceneitem); /* will be released below */

				context->groupItems.push_back(sceneitem);

				// Continue iteration
				return true;
			},
			&context);

		for (auto group_sceneitem : context.groupItems) {
			obs_source_t *source = obs_sceneitem_get_source(
				group_sceneitem); // does not increase refcount

			CefRefPtr<CefValue> item = CefValue::Create();

			SerializeSourceAndSceneItem(item, source,
						    group_sceneitem,
						    context.list->GetSize());

			context.list->SetValue(context.list->GetSize(), item);

			obs_sceneitem_release(
				group_sceneitem); /* addref above in obs_sceneitem_group_enum_items callback */
		}

		/* Group items */
		root->SetList("items", context.list);
	} else if (source && strcmp(obs_source_get_id(source), "scene") == 0) {
		/* Scene source */
	} else {
		/* Not a group and not a scene */
		root->SetValue("settings", SerializeObsSourceSettings(source));
	}

	root->SetList(
		"actions",
		StreamElementsSceneItemsMonitor::GetSceneItemActions(sceneitem)
			->Copy());

	root->SetValue(
		"icon",
		StreamElementsSceneItemsMonitor::GetSceneItemIcon(sceneitem)
			->Copy());

	root->SetValue(
		"defaultAction",
		StreamElementsSceneItemsMonitor::GetSceneItemDefaultAction(
			sceneitem)
			->Copy());

	root->SetValue(
		"auxiliaryData",
		StreamElementsSceneItemsMonitor::GetSceneItemAuxiliaryData(
			sceneitem)
			->Copy());

	root->SetValue("contextMenu",
		       StreamElementsSceneItemsMonitor::GetSceneItemContextMenu(
			       sceneitem)
			       ->Copy());

	result->SetDictionary(root);
}

///////////////////////////////////////////////////////////////////////

static obs_source_t *g_current_scene = nullptr;

static void dispatch_scene_update(void *my_data, calldata_t *cd)
{
	if (s_shutdown)
		return;

	StreamElementsCefClient::DispatchJSEvent(
		"hostActiveSceneItemListChanged", "null");
}

static void dispatch_sceneitem_event(obs_sceneitem_t *sceneitem,
				     std::string eventName)
{
	if (s_shutdown)
		return;

	if (!sceneitem)
		return;

	sceneitem_addref(sceneitem);

	if (sceneitem) {
		CefRefPtr<CefValue> item = CefValue::Create();

		obs_source_t *sceneitem_source =
			obs_sceneitem_get_source(sceneitem);

		SerializeSourceAndSceneItem(item, sceneitem_source, sceneitem);

		std::string json =
			CefWriteJSON(item, JSON_WRITER_DEFAULT).ToString();

		StreamElementsCefClient::DispatchJSEvent(eventName, json);
	} else {
		StreamElementsCefClient::DispatchJSEvent(eventName, "null");
	}

	sceneitem_release(sceneitem);
}

static void dispatch_sceneitem_event(void *my_data, calldata_t *cd,
				     std::string eventName)
{
	if (s_shutdown)
		return;

	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	dispatch_sceneitem_event(sceneitem, eventName);
}

static void dispatch_source_event(void *my_data, calldata_t *cd,
				  std::string eventName)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (!source)
		return;

	obs_source_t *scene_source =
		obs_frontend_get_current_scene(); // adds ref

	// Get scene handle
	obs_scene_t *scene = obs_scene_from_source(
		scene_source); // does not increment refcount

	if (!scene) {
		obs_source_release(scene_source);

		return;
	}

	obs_source_addref(source);

	// For each scene item
	ObsSceneEnumAllItems(scene, [source,
				     eventName](obs_sceneitem_t *sceneitem) {
		obs_source_t *sceneitem_source = obs_sceneitem_get_source(
			sceneitem); // does not increase refcount

		if (sceneitem_source == source) {
			dispatch_sceneitem_event(sceneitem, eventName);
		}

		return true;
	});

	obs_source_release(scene_source);
	obs_source_release(source);
}

static void add_scene_signals(obs_source_t *scene, void *data);

static void handle_scene_item_transform(void *my_data, calldata_t *cd)
{
	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemTransformed");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_select(void *my_data, calldata_t *cd)
{
	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemSelected");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_deselect(void *my_data, calldata_t *cd)
{
	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemUnselected");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_remove(void *my_data, calldata_t *cd)
{
	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	if (!sceneitem)
		return;

	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemRemoved");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_reorder(void *my_data, calldata_t *cd)
{
	if (s_shutdown)
		return;

	StreamElementsCefClient::DispatchJSEvent(
		"hostActiveSceneItemsOrderChanged", "null");

	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_source_update_props(void *my_data, calldata_t *cd)
{
	dispatch_source_event(my_data, cd,
			      "hostActiveSceneItemPropertiesChanged");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_source_update_settings(void *my_data,
						     calldata_t *cd)
{
	dispatch_source_event(my_data, cd,
			      "hostActiveSceneItemSettingsChanged");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_source_rename(void *my_data, calldata_t *cd)
{
	dispatch_source_event(my_data, cd, "hostActiveSceneItemRenamed");
	dispatch_scene_update(my_data, cd);
}

static void handle_scene_item_add(void *my_data, calldata_t *cd)
{
	if (s_shutdown)
		return;

	obs_sceneitem_t *sceneitem =
		(obs_sceneitem_t *)calldata_ptr(cd, "item");

	if (!sceneitem)
		return;

	dispatch_sceneitem_event(my_data, cd, "hostActiveSceneItemAdded");
	dispatch_scene_update(my_data, cd);

	if (!obs_sceneitem_is_group(sceneitem))
		return;

	obs_scene_t *group_scene = obs_sceneitem_group_get_scene(sceneitem);

	obs_source_t *group_scene_source = obs_scene_get_source(group_scene);

	add_scene_signals(group_scene_source, my_data);
}

static void remove_source_signals(obs_source_t *source, void *data)
{
	if (s_shutdown)
		return;

	if (!source)
		return;

	auto handler = obs_source_get_signal_handler(source);

	signal_handler_disconnect(handler, "update_properties",
				  handle_scene_item_source_update_props, data);

	signal_handler_disconnect(handler, "streamelements_update_settings",
				  handle_scene_item_source_update_settings,
				  data);

	signal_handler_disconnect(handler, "rename",
				  handle_scene_item_source_rename, data);
}

static void add_source_signals(obs_source_t *source, void *data)
{
	if (s_shutdown)
		return;

	auto handler = obs_source_get_signal_handler(source);

	signal_handler_connect(handler, "update_properties",
			       handle_scene_item_source_update_props, data);

	signal_handler_connect(handler, "streamelements_update_settings",
			       handle_scene_item_source_update_settings, data);

	signal_handler_connect(handler, "rename",
			       handle_scene_item_source_rename, data);
}

static void remove_scene_signals(obs_source_t *sceneSource, void *data)
{
	if (s_shutdown)
		return;

	if (!sceneSource)
		return;

	obs_scene_t *scene = obs_scene_from_source(sceneSource);

	if (!scene)
		return;

	obs_enter_graphics();
	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *scene) {
			obs_source_t *source = obs_scene_get_source(scene);

			if (!source)
				return;

			auto handler = obs_source_get_signal_handler(source);

			signal_handler_disconnect(handler, "item_add",
						  handle_scene_item_add, data);
			signal_handler_disconnect(handler, "item_remove",
						  handle_scene_item_remove,
						  data);
			signal_handler_disconnect(handler, "reorder",
						  handle_scene_item_reorder,
						  data);
			signal_handler_disconnect(handler, "item_visible",
						  dispatch_scene_update, data);
			signal_handler_disconnect(handler, "item_locked",
						  dispatch_scene_update, data);
			signal_handler_disconnect(handler, "item_select",
						  handle_scene_item_select,
						  data);
			signal_handler_disconnect(handler, "item_deselect",
						  handle_scene_item_deselect,
						  data);
			signal_handler_disconnect(handler, "item_transform",
						  handle_scene_item_transform,
						  data);

			obs_scene_enum_items(
				scene,
				[](obs_scene_t *scene,
				   obs_sceneitem_t *sceneitem, void *data) {
					if (obs_sceneitem_is_group(sceneitem)) {
						/* group */
						obs_scene_t *group_scene =
							obs_sceneitem_group_get_scene(
								sceneitem);

						obs_source_t *group_scene_source =
							obs_scene_get_source(
								group_scene);

						remove_scene_signals(
							group_scene_source,
							data);
					}

					/* Continue iteration */
					return true;
				},
				data);
		},
		data);
	obs_leave_graphics();
}

static void add_scene_signals(obs_source_t *sceneSource, void *data)
{
	if (s_shutdown)
		return;

	if (!sceneSource)
		return;

	obs_scene_t *scene = obs_scene_from_source(sceneSource);

	if (!scene)
		return;

	obs_enter_graphics();
	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *scene) {
			obs_source_t *source = obs_scene_get_source(scene);

			if (!source)
				return;

			auto handler = obs_source_get_signal_handler(source);

			signal_handler_connect(handler, "item_add",
					       handle_scene_item_add, data);
			signal_handler_connect(handler, "item_remove",
					       handle_scene_item_remove, data);
			signal_handler_connect(handler, "reorder",
					       handle_scene_item_reorder, data);
			signal_handler_connect(handler, "item_visible",
					       dispatch_scene_update, data);
			signal_handler_connect(handler, "item_locked",
					       dispatch_scene_update, data);
			signal_handler_connect(handler, "item_select",
					       handle_scene_item_select, data);
			signal_handler_connect(handler, "item_deselect",
					       handle_scene_item_deselect,
					       data);
			signal_handler_connect(handler, "item_transform",
					       handle_scene_item_transform,
					       data);

			obs_scene_enum_items(
				scene,
				[](obs_scene_t *scene,
				   obs_sceneitem_t *sceneitem, void *data) {
					if (obs_sceneitem_is_group(sceneitem)) {
						/* group */
						obs_scene_t *group_scene =
							obs_sceneitem_group_get_scene(
								sceneitem);

						obs_source_t *group_scene_source =
							obs_scene_get_source(
								group_scene);

						add_scene_signals(
							group_scene_source,
							data);
					}

					/* Continue iteration */
					return true;
				},
				data);
		},
		data);
	obs_leave_graphics();
}

static void remove_current_scene_signals(void *data)
{
	if (s_shutdown)
		return;

	if (!g_current_scene)
		return;

	remove_scene_signals(g_current_scene, data);
}

static void add_current_scene_signals(void *data)
{
	if (s_shutdown)
		return;

	if (!g_current_scene)
		return;

	add_scene_signals(g_current_scene, data);
}

void StreamElementsObsSceneManager::handle_obs_frontend_event(
	enum obs_frontend_event event, void *data)
{
	if (event == OBS_FRONTEND_EVENT_EXIT) {
		s_shutdown = true;

		return;
	}

	if (event != OBS_FRONTEND_EVENT_SCENE_CHANGED &&
	    event != OBS_FRONTEND_EVENT_SCENE_COLLECTION_CHANGED)
		return;

	StreamElementsObsSceneManager *self =
		(StreamElementsObsSceneManager *)data;

	obs_enter_graphics();

	obs_source_t *source = obs_frontend_get_current_scene(); // adds ref

	if (source && g_current_scene != source) {
		remove_current_scene_signals(data);

		if (g_current_scene)
			obs_source_release(
				g_current_scene); // release saved current scene ref

		g_current_scene = source; // replace ref with new scene

		add_current_scene_signals(data);
	} else {
		obs_source_release(source);
	}

	obs_leave_graphics();

	if (self->m_sceneItemsMonitor)
		self->m_sceneItemsMonitor->Update();

	if (self->m_scenesWidgetManager)
		self->m_scenesWidgetManager->Update();
}

static void handle_source_create(void *data, calldata_t *cd)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (!source)
		return;

	add_source_signals(source, data);
}

static void handle_source_remove(void *data, calldata_t *cd)
{
	obs_source_t *source = (obs_source_t *)calldata_ptr(cd, "source");

	if (!source)
		return;

	remove_source_signals(source, data);
}

///////////////////////////////////////////////////////////////////////

StreamElementsObsSceneManager::StreamElementsObsSceneManager(QMainWindow *parent)
	: m_parent(parent)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	m_sceneItemsMonitor = new StreamElementsSceneItemsMonitor(m_parent);
	m_scenesWidgetManager =
		new StreamElementsScenesListWidgetManager(m_parent);

	obs_enter_graphics();
	obs_frontend_add_event_callback(handle_obs_frontend_event, this);
	obs_leave_graphics();

	auto handler = obs_get_signal_handler();

	signal_handler_connect(handler, "source_create", handle_source_create,
			       this);

	signal_handler_connect(handler, "source_remove", handle_source_remove,
			       this);

	DeserializeSceneItemsAuxiliaryActions(
		CefParseJSON(StreamElementsConfig::GetInstance()
				     ->GetSceneItemsAuxActionsConfig(),
			     JSON_PARSER_ALLOW_TRAILING_COMMAS),
		CefValue::Create());

	DeserializeScenesAuxiliaryActions(
		CefParseJSON(StreamElementsConfig::GetInstance()
				     ->GetScenesAuxActionsConfig(),
			     JSON_PARSER_ALLOW_TRAILING_COMMAS),
		CefValue::Create());
}

StreamElementsObsSceneManager::~StreamElementsObsSceneManager()
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	delete m_sceneItemsMonitor;

	// remove_current_scene_signals(this); // this line throws an exception on OBS shutdown

	obs_enter_graphics();
	obs_frontend_remove_event_callback(handle_obs_frontend_event, this);
	obs_leave_graphics();

	auto handler = obs_get_signal_handler();

	signal_handler_disconnect(handler, "source_create",
				  handle_source_create, this);

	signal_handler_disconnect(handler, "source_remove",
				  handle_source_remove, this);
}

void StreamElementsObsSceneManager::ObsAddSourceInternal(
	obs_source_t *parentScene, obs_sceneitem_t *parentGroup,
	const char *sourceId, const char *sourceName,
	obs_data_t *sourceSettings, obs_data_t *sourceHotkeyData,
	bool preferExistingSource, obs_source_t **output_source,
	obs_sceneitem_t **output_sceneitem)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	bool releaseParentScene = false;

	if (parentScene == NULL) {
		parentScene = obs_frontend_get_current_scene();

		releaseParentScene = true;
	}

	obs_source_t *source = NULL;

	if (preferExistingSource) {
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

		QApplication::processEvents();

		obs_source_update(source, sourceSettings);
	}

	// Wait for dimensions: some sources like video capture source do not
	// get their dimensions immediately: they are initializing asynchronously
	// and are not aware of the source dimensions until they do.
	//
	// We'll do this for maximum 15 seconds and give up.
	for (int i = 0; i < 150 && obs_source_get_width(source) == 0; ++i) {
		os_sleep_ms(100);
	}

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

	obs_enter_graphics();
	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *scene) {
			atomic_update_args *args = (atomic_update_args *)data;

			args->sceneitem = obs_scene_add(scene, args->source);

			if (args->sceneitem) {
				obs_sceneitem_addref(args->sceneitem);

				if (!!args->group &&
				    obs_sceneitem_is_group(args->group)) {
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
	obs_leave_graphics();

	if (args.sceneitem) {
		if (!!args.group) {
			RefreshObsSceneItemsList();
		}

		if (output_sceneitem != NULL) {
			obs_sceneitem_addref(args.sceneitem);

			*output_sceneitem = args.sceneitem;
		}

		obs_sceneitem_release(
			args.sceneitem); // was allocated in atomic_scene_update
	}

	if (output_source != NULL) {
		*output_source = source;
	} else {
		obs_source_release(source);
	}

	if (releaseParentScene) {
		obs_source_release(parentScene);
	}
}

std::string
StreamElementsObsSceneManager::ObsGetUniqueSourceName(std::string name)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string result(name);

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique) {
		isUnique = true;

		obs_source_t *source = obs_get_source_by_name(result.c_str());
		if (source != NULL) {
			obs_source_release(source);

			isUnique = false;
		}

		if (!isUnique) {
			++sequence;

			char buf[32];
			result = name + " ";
			result += itoa(sequence, buf, 10);
		}
	}

	return result;
}

void StreamElementsObsSceneManager::DeserializeObsBrowserSource(
	CefRefPtr<CefValue> &input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsBrowserSourceSceneItemInfoValid(input)) {
		return;
	}

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

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		// Add browser source

		obs_source_t *parent_scene =
			nullptr; // user default: current scene

		obs_source_t *source;
		obs_sceneitem_t *sceneitem;

		ObsAddSourceInternal(parent_scene, FindSceneItemById(groupId),
				     source_class.c_str(), unique_name.c_str(),
				     settings, nullptr, false, &source,
				     &sceneitem);

		if (sceneitem) {
			obs_transform_info info;
			obs_sceneitem_crop crop;

			if (DeserializeSceneItemComposition(input, info,
							    crop)) {
				obs_sceneitem_set_info(sceneitem, &info);
				obs_sceneitem_set_crop(sceneitem, &crop);
			}

			// Result
			SerializeSourceAndSceneItem(output, source, sceneitem);

			obs_sceneitem_release(sceneitem);
		}

		obs_source_release(source);
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsGameCaptureSource(
	CefRefPtr<CefValue> &input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, false, false))
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

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		// Add game capture source

		obs_source_t *parent_scene =
			nullptr; // user default: current scene

		obs_source_t *source;
		obs_sceneitem_t *sceneitem;

		ObsAddSourceInternal(parent_scene, FindSceneItemById(groupId),
				     source_class.c_str(), unique_name.c_str(),
				     settings, nullptr, false, &source,
				     &sceneitem);

		if (sceneitem) {
			obs_transform_info info;
			obs_sceneitem_crop crop;

			if (DeserializeSceneItemComposition(input, info,
							    crop)) {
				obs_sceneitem_set_info(sceneitem, &info);
				obs_sceneitem_set_crop(sceneitem, &crop);
			}

			// Result
			SerializeSourceAndSceneItem(output, source, sceneitem);

			obs_sceneitem_release(sceneitem);
		}

		obs_source_release(source);
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsVideoCaptureSource(
	CefRefPtr<CefValue> &input, CefRefPtr<CefValue> &output)
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

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeData(root->GetValue("settings"), settings)
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

			obs_source_t *parent_scene =
				nullptr; // user default: current scene

			obs_source_t *source;
			obs_sceneitem_t *sceneitem;

			ObsAddSourceInternal(
				parent_scene, FindSceneItemById(groupId),
				source_class.c_str(), unique_name.c_str(),
				settings, nullptr, true, &source, &sceneitem);

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

				// Result
				SerializeSourceAndSceneItem(output, source,
							    sceneitem);

				obs_sceneitem_release(sceneitem);
			}

			obs_source_release(source);
		}
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsNativeSource(
	CefRefPtr<CefValue> &input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, true, true))
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

	bool preferExistingSourceReference =
		root->HasKey("preferExistingSourceReference")
			? root->GetBool("preferExistingSourceReference")
			: false;

	obs_data_t *settings = obs_data_create();

	bool parsed =
		root->HasKey("settings")
			? DeserializeData(root->GetValue("settings"), settings)
			: true;

	if (parsed) {
		// Add game capture source

		obs_source_t *parent_scene =
			nullptr; // user default: current scene

		obs_source_t *source;
		obs_sceneitem_t *sceneitem;

		ObsAddSourceInternal(parent_scene, FindSceneItemById(groupId),
				     source_class.c_str(), unique_name.c_str(),
				     settings, nullptr,
				     preferExistingSourceReference, &source,
				     &sceneitem);

		if (sceneitem) {
			obs_transform_info info;
			obs_sceneitem_crop crop;

			if (DeserializeSceneItemComposition(input, info,
							    crop)) {
				obs_sceneitem_set_info(sceneitem, &info);
				obs_sceneitem_set_crop(sceneitem, &crop);
			}

			// Result
			SerializeSourceAndSceneItem(output, source, sceneitem);

			obs_sceneitem_release(sceneitem);
		}

		obs_source_release(source);
	}

	obs_data_release(settings);
}

void StreamElementsObsSceneManager::DeserializeObsSceneItemGroup(
	CefRefPtr<CefValue> &input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	if (!IsSceneItemInfoValid(input, false, false))
		return;
	CefRefPtr<CefDictionaryValue> root = input->GetDictionary();

	struct atomic_update_args {
		std::string unique_name;
		obs_sceneitem_t *sceneitem;
	};

	atomic_update_args args;

	args.unique_name =
		ObsGetUniqueSourceName(root->GetString("name").ToString());

	obs_source_t *parent_scene = obs_frontend_get_current_scene();
	obs_scene_t *scene = obs_scene_from_source(parent_scene);

	obs_enter_graphics();
	obs_scene_atomic_update(
		scene,
		[](void *data, obs_scene_t *scene) {
			atomic_update_args *args = (atomic_update_args *)data;

			args->sceneitem = obs_scene_add_group(
				scene, args->unique_name.c_str());

			if (args->sceneitem) {
				obs_sceneitem_addref(args->sceneitem);

				obs_sceneitem_set_visible(args->sceneitem,
							  true);
			}
		},
		&args);
	obs_leave_graphics();

	if (args.sceneitem) {
		obs_transform_info info;
		obs_sceneitem_crop crop;

		if (DeserializeSceneItemComposition(input, info, crop)) {
			obs_sceneitem_set_info(args.sceneitem, &info);
			obs_sceneitem_set_crop(args.sceneitem, &crop);
		}

		// Result
		SerializeSourceAndSceneItem(
			output, obs_sceneitem_get_source(args.sceneitem),
			args.sceneitem);

		//obs_sceneitem_release(args.sceneitem); // obs_scene_add_group() does not return an incremented reference
	}

	obs_source_release(parent_scene);

	RefreshObsSceneItemsList();
}

void StreamElementsObsSceneManager::SerializeObsCurrentSceneItems(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	output->SetNull();

	// Get list of scenes
	obs_source_t *sceneSource = obs_frontend_get_current_scene();

	if (!sceneSource) {
		return;
	}

	// Get scene handle
	obs_scene_t *scene = obs_scene_from_source(
		sceneSource); // does not increment refcount

	if (scene) {
		struct local_context {
			CefRefPtr<CefListValue> list;
		};

		local_context context;

		context.list = CefListValue::Create();

		// For each scene item
		obs_scene_enum_items(
			scene,
			[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
			   void *param) {
				local_context *context = (local_context *)param;

				obs_source_t *source = obs_sceneitem_get_source(
					sceneitem); // does not increase refcount

				CefRefPtr<CefValue> item = CefValue::Create();

				SerializeSourceAndSceneItem(
					item, source, sceneitem,
					context->list->GetSize());

				context->list->SetValue(
					context->list->GetSize(), item);

				// Continue iteration
				return true;
			},
			&context);

		output->SetList(context.list);
	}

	obs_source_release(sceneSource);
}

static void SerializeObsScene(obs_source_t *scene, CefRefPtr<CefValue> &result)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetString("id", GetIdFromPointer(scene));
	d->SetString("name", obs_source_get_name(scene));

	d->SetValue("icon",
		    StreamElementsScenesListWidgetManager::GetSceneIcon(scene)
			    ->Copy());

	d->SetValue(
		"auxiliaryData",
		StreamElementsScenesListWidgetManager::GetSceneAuxiliaryData(
			scene)
			->Copy());

	d->SetValue(
		"defaultAction",
		StreamElementsScenesListWidgetManager::GetSceneDefaultAction(
			scene)
			->Copy());

	d->SetValue("contextMenu",
		    StreamElementsScenesListWidgetManager::GetSceneContextMenu(
			    scene)
			    ->Copy());

	result->SetDictionary(d);
}

static void SerializeObsScene(obs_scene_t *scene, CefRefPtr<CefValue> &result)
{
	SerializeObsScene(obs_scene_get_source(scene), result);
}

void StreamElementsObsSceneManager::SerializeObsCurrentScene(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_source_t *scene = obs_frontend_get_current_scene();

	if (scene) {
		SerializeObsScene(scene, output);

		obs_source_release(scene);
	}
}

void StreamElementsObsSceneManager::SerializeObsScenes(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	CefRefPtr<CefListValue> list = CefListValue::Create();

	struct obs_frontend_source_list scenes = {};

	obs_frontend_get_scenes(&scenes);

	for (size_t idx = 0; idx < scenes.sources.num; ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *scene = scenes.sources.array[idx];

		CefRefPtr<CefValue> item = CefValue::Create();

		SerializeObsScene(scene, item);

		list->SetValue(list->GetSize(), item);
	}

	obs_frontend_source_list_free(&scenes);

	output->SetList(list);
}

std::string
StreamElementsObsSceneManager::ObsGetUniqueSceneName(std::string name)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string result(name);

	struct obs_frontend_source_list scenes = {};

	obs_frontend_get_scenes(&scenes);

	auto hasSceneName = [&](std::string name) -> bool {
		for (size_t idx = 0; idx < scenes.sources.num; ++idx) {
			/* Get the scene (a scene is a source) */
			obs_source_t *scene = scenes.sources.array[idx];

			if (stricmp(name.c_str(), obs_source_get_name(scene)) ==
			    0)
				return true;
		}

		return false;
	};

	int sequence = 0;
	bool isUnique = false;

	while (!isUnique) {
		isUnique = true;

		if (hasSceneName(result)) {
			isUnique = false;
		}

		if (!isUnique) {
			++sequence;

			char buf[32];
			result = name + " ";
			result += itoa(sequence, buf, 10);
		}
	}

	obs_frontend_source_list_free(&scenes);

	return result;
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
			if (stricmp(result.c_str(), names[idx]) == 0)
				isUnique = false;
		}

		if (!isUnique) {
			++sequence;

			char buf[32];
			result = name + " ";
			result += itoa(sequence, buf, 10);
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

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	std::string name = ObsGetUniqueSceneName(d->GetString("name"));

	obs_scene_t *scene = obs_scene_create(name.c_str());
	QApplication::processEvents();

	SerializeObsScene(scene, output);

	obs_frontend_set_current_scene(obs_scene_get_source(scene));
	QApplication::processEvents();

	obs_scene_release(scene);
}

void StreamElementsObsSceneManager::SetCurrentObsSceneById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	if (!id.size())
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	const void *sceneIdPointer = GetPointerFromId(id.c_str());

	if (!sceneIdPointer)
		return;

	struct obs_frontend_source_list scenes = {};

	obs_frontend_get_scenes(&scenes);

	for (size_t idx = 0; idx < scenes.sources.num; ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *scene = scenes.sources.array[idx];

		if (sceneIdPointer == (void *)scene) {
			obs_frontend_set_current_scene(scene);
			QApplication::processEvents();

			SerializeObsScene(scene, output);

			break;
		}
	}

	obs_frontend_source_list_free(&scenes);
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
		const void *ptr = GetPointerFromId(id.c_str());

		if (!ptr)
			continue;

		pointer_to_id_map[ptr] = id;
	}

	if (pointer_to_id_map.empty())
		return;

	output->SetBool(true);

	struct obs_frontend_source_list scenes = {};

	obs_frontend_get_scenes(&scenes);

	size_t removedCount = 0;

	for (size_t idx = 0;
	     idx < scenes.sources.num && removedCount + 1 < scenes.sources.num;
	     ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *scene = scenes.sources.array[idx];

		if (pointer_to_id_map.count(scene)) {
			obs_source_remove(scene);

			++removedCount;
		}
	}

	obs_frontend_source_list_free(&scenes);

	QApplication::processEvents();
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

	std::string id = d->GetString("id");
	const void *ptr = GetPointerFromId(id.c_str());

	struct obs_frontend_source_list scenes = {};

	obs_frontend_get_scenes(&scenes);

	bool result = false;

	for (size_t idx = 0; idx < scenes.sources.num; ++idx) {
		/* Get the scene (a scene is a source) */
		obs_source_t *scene = scenes.sources.array[idx];

		if (scene == ptr) {
			if (d->HasKey("name")) {
				obs_source_set_name(
					scene,
					d->GetString("name").ToString().c_str());

				result = true;
			}

			if (d->HasKey("icon")) {
				m_scenesWidgetManager->SetSceneIcon(
					scene, d->GetValue("icon")->Copy());
			}

			if (d->HasKey("defaultAction")) {
				m_scenesWidgetManager->SetSceneDefaultAction(
					scene,
					d->GetValue("defaultAction")->Copy());
			}

			if (d->HasKey("contextMenu")) {
				m_scenesWidgetManager->SetSceneContextMenu(
					scene,
					d->GetValue("contextMenu")->Copy());
			}

			if (d->HasKey("auxiliaryData")) {
				m_scenesWidgetManager->SetSceneAuxiliaryData(
					scene,
					d->GetValue("auxiliaryData")->Copy());
			}
		}
	}

	obs_frontend_source_list_free(&scenes);

	// QApplication::processEvents();

	output->SetBool(result);
}

void StreamElementsObsSceneManager::RemoveObsCurrentSceneItemsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (!input.get() || input->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list = input->GetList();

	if (!list->GetSize())
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	struct local_context {
		std::unordered_map<const void *, std::string> ptr_to_id_map;
		std::list<obs_sceneitem_t *> scene_items_to_remove;
	};

	local_context context;

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_STRING)
			continue;

		std::string id = list->GetString(index);
		const void *ptr = GetPointerFromId(id.c_str());

		if (!ptr)
			continue;

		context.ptr_to_id_map[ptr] = id;
	}

	if (context.ptr_to_id_map.empty())
		return;

	output->SetBool(true);

	obs_source_t *currentScene = obs_frontend_get_current_scene();

	obs_scene_t *scene = obs_scene_from_source(
		currentScene); // does not increment refcount

	obs_scene_enum_items(
		scene,
		[](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
		   void *param) {
			local_context *context = (local_context *)param;

			//obs_source_t *source = obs_sceneitem_get_source(
			//	sceneitem); // does not increment refcount

			if (context->ptr_to_id_map.count(sceneitem)) {
				context->scene_items_to_remove.push_back(
					sceneitem);
			}

			/* Continue iteration */
			return true;
		},
		&context);

	for (obs_sceneitem_t *sceneitem : context.scene_items_to_remove) {
		/* Remove the scene item */
		obs_sceneitem_remove(sceneitem);

		// QApplication::processEvents();
	}

	obs_source_release(currentScene);

	// QApplication::processEvents();
}

void StreamElementsObsSceneManager::SetObsCurrentSceneItemPropertiesById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
		return;

	std::string id = d->GetString("id");
	const void *ptr = GetPointerFromId(id.c_str());

	if (!ptr)
		return;

	std::string groupId = d->HasKey("parentId") && d->GetType("parentId") ==
							       VTYPE_STRING
				      ? d->GetString("parentId").ToString()
				      : "";

	struct local_context {
		const void *ptr = nullptr;
		obs_sceneitem_t *sceneitem = nullptr;
	};

	local_context context = {0};

	context.ptr = ptr;

	obs_source_t *currentScene = obs_frontend_get_current_scene();

	obs_scene_t *scene = obs_scene_from_source(
		currentScene); // does not increment refcount

	context.sceneitem = FindSceneItemById(id);

	if (!!context.sceneitem) {
		bool result = false;

		obs_source_t *source = obs_sceneitem_get_source(
			context.sceneitem); // does not increment refcount

		if (d->HasKey("visible") &&
		    d->GetType("visible") == VTYPE_BOOL) {
			obs_sceneitem_set_visible(context.sceneitem,
						  d->GetBool("visible"));
		}

		if (d->HasKey("name")) {
			obs_source_set_name(
				source,
				d->GetString("name").ToString().c_str());

			result = true;
		}

		if (d->HasKey("settings")) {
			obs_data_t *settings = obs_data_create();

			bool parsed = DeserializeData(d->GetValue("settings"),
						      settings);

			if (parsed) {
				obs_source_update(source, settings);

				result = true;
			}

			obs_data_release(settings);
		}

		if (d->HasKey("actions")) {
			if (d->GetType("actions") == VTYPE_LIST) {
				CefRefPtr<CefListValue> actionsList =
					d->GetList("actions");

				m_sceneItemsMonitor->SetSceneItemActions(
					context.sceneitem, actionsList);
			} else {
				CefRefPtr<CefListValue> emptyList =
					CefListValue::Create();

				m_sceneItemsMonitor->SetSceneItemActions(
					context.sceneitem, emptyList);
			}
		}

		if (d->HasKey("icon")) {
			m_sceneItemsMonitor->SetSceneItemIcon(
				context.sceneitem, d->GetValue("icon")->Copy());
		}

		if (d->HasKey("defaultAction")) {
			m_sceneItemsMonitor->SetSceneItemDefaultAction(
				context.sceneitem,
				d->GetValue("defaultAction")->Copy());
		}

		if (d->HasKey("auxiliaryData")) {
			m_sceneItemsMonitor->SetSceneItemAuxiliaryData(
				context.sceneitem,
				d->GetValue("auxiliaryData")->Copy());
		}

		if (d->HasKey("contextMenu")) {
			m_sceneItemsMonitor->SetSceneItemContextMenu(
				context.sceneitem,
				d->GetValue("contextMenu")->Copy());
		}

		obs_transform_info info;
		obs_sceneitem_crop crop;

		if (DeserializeSceneItemComposition(input, info, crop)) {
			obs_sceneitem_set_info(context.sceneitem, &info);
			obs_sceneitem_set_crop(context.sceneitem, &crop);
		}

		if (d->HasKey("selected") &&
		    d->GetType("selected") == VTYPE_BOOL) {
			obs_sceneitem_select(context.sceneitem,
					     d->GetBool("selected"));
		}

		if (d->HasKey("locked") && d->GetType("locked") == VTYPE_BOOL) {
			obs_sceneitem_set_locked(context.sceneitem,
						 d->GetBool("locked"));
		}

		if (d->HasKey("order") && d->GetType("order") == VTYPE_INT) {
			int order = d->GetInt("order");

			auto update = [&]() -> void {
				if (!obs_sceneitem_get_scene(context.sceneitem))
					return;

				obs_sceneitem_set_order_position(
					context.sceneitem, order);
			};

			using update_t = decltype(update);

			obs_enter_graphics();
			obs_scene_atomic_update(
				scene,
				[](void *data, obs_scene_t *) {
					(*reinterpret_cast<update_t *>(data))();
				},
				&update);
			obs_leave_graphics();
		}

#if ENABLE_OBS_GROUP_ADD_REMOVE_ITEM
		// This piece of code just does not work. It seems that
		// the implementation of `obs_sceneitem_group_remove_item`
		// is broken.
		//
		// TODO: TBD: implement when a fix is available
		{
			struct atomic_update_args {
				obs_sceneitem_t *group;
				obs_sceneitem_t *current_group;
				obs_sceneitem_t *sceneitem;
			};

			atomic_update_args args;
			args.sceneitem = context.sceneitem;
			args.group = FindSceneItemById(groupId);
			args.current_group = obs_sceneitem_get_group(
				scene, context.sceneitem);

			if (args.group != args.current_group) {
				obs_enter_graphics();

				obs_scene_atomic_update(
					scene,
					[](void *data, obs_scene_t *scene) {
						atomic_update_args *args =
							(atomic_update_args *)
								data;

						obs_sceneitem_t
							*original_sceneitem =
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

				obs_leave_graphics();

				RefreshObsSceneItemsList();
			}
		}
#endif

		// Result
		SerializeSourceAndSceneItem(output, source, context.sceneitem);
	}

	obs_source_release(currentScene);
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

static bool SerializeProp(obs_property_t *prop, CefRefPtr<CefValue> &output)
{
	if (!prop)
		return false;

	if (!obs_property_visible(prop))
		return false;

	auto safe_str = [](const char *input) -> std::string {
		if (!input)
			return "";
		else
			return input;
	};

	CefRefPtr<CefDictionaryValue> root = CefDictionaryValue::Create();

	obs_property_type type_id = obs_property_get_type(prop);
	std::string dataType = "";
	std::string controlType = "";
	std::string controlMode = "";
	std::string valueFormat = "";
	std::string propDefault = "";

	switch (type_id) {
	case OBS_PROPERTY_BOOL:
		dataType = "bool";
		controlType = "checkbox";
		controlMode = "";
		break;

	case OBS_PROPERTY_INT:
		dataType = "integer";
		controlType = "number";

		switch (obs_property_int_type(prop)) {
		case OBS_NUMBER_SCROLLER:
			controlMode = "scroller";
			break;
		case OBS_NUMBER_SLIDER:
			controlMode = "slider";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_FLOAT:
		dataType = "float";
		controlType = "number";

		switch (obs_property_float_type(prop)) {
		case OBS_NUMBER_SCROLLER:
			controlMode = "scroller";
			break;
		case OBS_NUMBER_SLIDER:
			controlMode = "slider";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_TEXT:
		dataType = "string";
		controlType = "text";
		controlMode = "";

		switch (obs_property_text_type(prop)) {
		case OBS_TEXT_DEFAULT:
			controlMode = "text";
			break;
		case OBS_TEXT_PASSWORD:
			controlMode = "password";
			break;
		case OBS_TEXT_MULTILINE:
			controlMode = "textarea";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_PATH:
		dataType = "string";
		controlType = "path";

		switch (obs_property_path_type(prop)) {
		case OBS_PATH_FILE:
			controlMode = "open";
			break;
		case OBS_PATH_FILE_SAVE:
			controlMode = "save";
			break;
		case OBS_PATH_DIRECTORY:
			controlMode = "folder";
			break;
		default:
			return false;
		}

		valueFormat = safe_str(obs_property_path_filter(prop));
		propDefault = safe_str(obs_property_path_default_path(prop));
		break;

	case OBS_PROPERTY_LIST:
		controlType = "select";

		switch (obs_property_list_type(prop)) {
		case OBS_COMBO_TYPE_EDITABLE:
			controlMode = "dynamic";
			break;
		case OBS_COMBO_TYPE_LIST:
			controlMode = "static";
			break;
		default:
			return false;
		}

		switch (obs_property_list_format(prop)) {
		case OBS_COMBO_FORMAT_INT:
			dataType = "integer";
			break;
		case OBS_COMBO_FORMAT_FLOAT:
			dataType = "float";
			break;
		case OBS_COMBO_FORMAT_STRING:
			dataType = "string";
			break;
		default:
			return false;
		}
		break;

	case OBS_PROPERTY_COLOR:
		dataType = "string";
		controlType = "text";
		controlMode = "color";
		break;

	case OBS_PROPERTY_BUTTON:
		return false;

	case OBS_PROPERTY_FONT:
		dataType = "font";
		controlType = "font";
		controlMode = "";
		break;

	case OBS_PROPERTY_EDITABLE_LIST:
		dataType = "array";
		controlType = "list";
		controlMode = "dynamic";
		valueFormat = safe_str(obs_property_editable_list_filter(prop));
		propDefault =
			safe_str(obs_property_editable_list_default_path(prop));
		break;

	case OBS_PROPERTY_FRAME_RATE:
		dataType = "frame_rate";
		controlType = "frame_rate";
		controlMode = "";
		break;

	case OBS_PROPERTY_GROUP:
		dataType = "group";
		controlType = "group";

		switch (obs_property_group_type(prop)) {
		case OBS_GROUP_NORMAL:
			controlMode = "normal";
			break;

		case OBS_GROUP_CHECKABLE:
			controlMode = "checkable";
			break;

		default:
			return false;
		}
		break;

	default:
		return false;
	}

	root->SetString("name", obs_property_name(prop));
	root->SetString("label", safe_str(obs_property_description(prop)));
	root->SetString("description",
			safe_str(obs_property_long_description(prop)));
	root->SetString("dataType", dataType);
	root->SetString("controlType", controlType);

	if (controlMode.size())
		root->SetString("controlMode", controlMode);

	if (propDefault.size())
		root->SetString("defaultValue", propDefault);

	if (valueFormat.size())
		root->SetString("valueFormat", valueFormat);

	if (type_id == OBS_PROPERTY_LIST) {
		const size_t count = obs_property_list_item_count(prop);
		const obs_combo_format comboFormat =
			obs_property_list_format(prop);

		CefRefPtr<CefListValue> items = CefListValue::Create();

		for (size_t index = 0; index < count; ++index) {
			CefRefPtr<CefDictionaryValue> item =
				CefDictionaryValue::Create();

			item->SetString("name",
					safe_str(obs_property_list_item_name(
						prop, index)));

			item->SetBool("enabled",
				      !obs_property_list_item_disabled(prop,
								       index));

			switch (comboFormat) {
			case OBS_COMBO_FORMAT_INT:
				item->SetInt("value",
					     obs_property_list_item_int(prop,
									index));
				break;
			case OBS_COMBO_FORMAT_FLOAT:
				item->SetDouble("value",
						obs_property_list_item_float(
							prop, index));
				break;
			case OBS_COMBO_FORMAT_STRING:
				item->SetString(
					"value",
					safe_str(obs_property_list_item_string(
						prop, index)));
				break;
			default:
				continue;
			}

			items->SetDictionary(items->GetSize(), item);
		}

		root->SetList("items", items);
	} else if (type_id == OBS_PROPERTY_EDITABLE_LIST) {
		/* Nothing to do */
	} else if (type_id == OBS_GROUP_NORMAL) {
		obs_properties_t *props = obs_property_group_content(
			prop); // Does not increment refcount

		if (props) {
			CefRefPtr<CefListValue> items = CefListValue::Create();

			obs_property_t *itemProp = obs_properties_first(props);

			do {
				if (!itemProp)
					break;

				CefRefPtr<CefValue> item = CefValue::Create();

				if (SerializeProp(itemProp, item)) {
					items->SetValue(items->GetSize(), item);
				}
			} while (obs_property_next(&prop));

			root->SetList("items", items);
		}
	}

	output->SetDictionary(root);

	return true;
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

	obs_properties_t *props = obs_get_source_properties(id.c_str());

	if (!props)
		return;

	if (d->HasKey("settings")) {
		obs_data_t *settings = obs_data_create();

		if (DeserializeData(d->GetValue("settings"), settings)) {
			obs_properties_apply_settings(props, settings);
		}

		obs_data_release(settings);
	}

	CefRefPtr<CefListValue> list = CefListValue::Create();

	obs_property_t *prop = obs_properties_first(props);

	do {
		if (!prop)
			break;

		CefRefPtr<CefValue> item = CefValue::Create();

		if (SerializeProp(prop, item)) {
			list->SetValue(list->GetSize(), item);
		}
	} while (obs_property_next(&prop));

	output->SetList(list);

	obs_properties_destroy(props);
}

void StreamElementsObsSceneManager::UngroupObsCurrentSceneItemsByGroupId(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("id") || d->GetType("id") != VTYPE_STRING)
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_sceneitem_t *group =
		FindSceneItemById(d->GetString("id").ToString().c_str());

	if (!group)
		return;

	if (!obs_sceneitem_is_group(group))
		return;

	obs_sceneitem_group_ungroup(group);

	RefreshObsSceneItemsList();

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
		if (stricmp(item.second.c_str(), id.c_str()) == 0) {
			actualId = item.second;

			break;
		} else if (stricmp(item.first.c_str(), id.c_str()) == 0) {
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

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_sceneitem_t *item =
		FindSceneItemById(input->GetString().ToString().c_str());

	output->SetBool(
		m_sceneItemsMonitor->InvokeCurrentSceneItemDefaultAction(item));
}

void StreamElementsObsSceneManager::InvokeCurrentSceneItemDefaultContextMenuById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	output->SetNull();

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	std::lock_guard<decltype(m_mutex)> guard(m_mutex);

	obs_sceneitem_t *item =
		FindSceneItemById(input->GetString().ToString().c_str());

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
