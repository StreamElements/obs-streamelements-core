#include "StreamElementsSceneItemsMonitor.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsRemoteIconLoader.hpp"
#include "StreamElementsConfig.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <string>
#include <vector>

#include <QDockWidget>
#include <QLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QContextMenuEvent>
#include <QGraphicsOpacityEffect>
#include <QToolButton>

#ifndef max
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

static const char *ITEM_PRIVATE_DATA_KEY_UI_AUXILIARY_ACTIONS =
	"streamelements_aux_ui_actions";

static const char *ITEM_PRIVATE_DATA_KEY_UI_ICON = "streamelements_ui_icon";

static const char *ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION =
	"streamelements_ui_default_action";

static const char *ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU =
	"streamelements_ui_context_menu";

static const char *ITEM_PRIVATE_DATA_KEY_APPDATA = "streamelements_app_data";

static const char *ITEM_PRIVATE_DATA_KEY_UI_SETTINGS = "streamelements_ui_settings";

static const char *WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT =
	"streamelements_default_action_bypass_count";

static const char *WIDGET_PROP_SCENE_ITEM_ID = "streamelements_scene_item_id";

static bool IsSupportedOBSVersion() {
	// OBS 29 has breaking changes in functions related to updating
	// toolbars' styles on Scenes and Scene Items.
	//
	// We have submitted a patch which will be included in OBS 30.
	//
	// For the moment, we'll disable related functionality in OBS 29 specifically.
	//
	auto obs_version = obs_get_version();

	if (obs_version >= MAKE_SEMANTIC_VERSION(29, 0, 0) &&
	    obs_version < MAKE_SEMANTIC_VERSION(30, 0, 0)) {
		return false;
	}

	return true;
}

static obs_sceneitem_t *FindSceneItemByIdAddRef(std::string id)
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

	if (!currentScene)
		return nullptr;

	obs_scene_t *scene = obs_scene_from_source(
		currentScene); // does not increment refcount

	ObsSceneEnumAllItems(scene, [&](obs_sceneitem_t *sceneitem) {
		if (context.searchPtr == sceneitem) {
			context.sceneitem = sceneitem;

			return false;
		}

		return true;
	});

	obs_source_release(currentScene);

	if (context.sceneitem)
		obs_sceneitem_addref(context.sceneitem);

	return context.sceneitem;
}

static obs_sceneitem_t *GetObjectSceneItemAddRef(QObject *o)
{
	QVariant sceneItemIdVariant = o->property(WIDGET_PROP_SCENE_ITEM_ID);

	return sceneItemIdVariant.isValid()
		       ? FindSceneItemByIdAddRef(
				 sceneItemIdVariant.toString().toStdString())
		       : nullptr;
}

typedef std::vector<obs_sceneitem_t *> sceneitem_list_t;

static void ReleaseSceneItemsList(sceneitem_list_t* list) {
	for (auto sceneitem : *list) {
		obs_sceneitem_release(sceneitem);
	}

	delete list;
}

static sceneitem_list_t* GetSelectedSceneItemsAddRef()
{
	obs_source_t *sceneSource = obs_frontend_get_current_scene();

	obs_scene_t *scene = obs_scene_from_source(sceneSource); // does not increment refcount

	sceneitem_list_t* sceneItems = new sceneitem_list_t();

	obs_scene_enum_items(scene,
			     [](obs_scene_t *scene, obs_sceneitem_t *sceneitem,
				void *param) {
			sceneitem_list_t *list = (sceneitem_list_t *)param;

			if (obs_sceneitem_selected(sceneitem)) {
				obs_sceneitem_addref(sceneitem);

				list->push_back(sceneitem);
			}

			// Continue iteration
			return true;
		},
		sceneItems);

	obs_source_release(sceneSource);

	return sceneItems;
}

#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
static bool
SceneItemHasCustomContextMenu(obs_sceneitem_t* scene_item)
{
	CefRefPtr<CefValue> value =
		StreamElementsSceneItemsMonitor::GetSceneItemContextMenu(
			scene_item);

	if (value->GetType() == VTYPE_LIST && value->GetList()->GetSize()) {
		return true;
	}

	return false;
}
#endif

static bool HandleDefaultActionRequest(obs_sceneitem_t *scene_item,
				       StreamElementsSceneItemsMonitor *monitor,
				       QEvent *e)
{
	bool handled = false;

	bool enabled =
		StreamElementsSceneItemsMonitor::GetSceneItemUISettingsEnabled(
			scene_item);

	if (!enabled) {
		handled = true;
	} else {
		#if SE_ENABLE_SCENEITEM_DEFAULT_ACTION
		CefRefPtr<CefValue> value = StreamElementsSceneItemsMonitor::
			GetSceneItemDefaultAction(scene_item);

		if (value->GetType() == VTYPE_DICTIONARY) {
			auto defaultAction = [monitor, scene_item]() {
				if (!scene_item)
					return;

				monitor
					->InvokeCurrentSceneItemDefaultAction(
						scene_item);
			};

			auto defaultContextMenu = [monitor, scene_item]() {
				if (!scene_item)
					return;

				monitor->InvokeCurrentSceneItemDefaultContextMenu(
						scene_item);
			};

			handled = DeserializeAndInvokeAction(
				value, defaultAction, defaultContextMenu);
		}
		#endif
	}

	return handled;
}

static bool
HandleSceneItemContextMenuRequest(obs_sceneitem_t *scene_item,
				  StreamElementsSceneItemsMonitor *monitor,
				  QEvent *e)
{
	#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
	bool handled = false;

	bool enabled =
		StreamElementsSceneItemsMonitor::GetSceneItemUISettingsEnabled(
			scene_item);

	if (!enabled) {
		handled = true;
	} else {
		CefRefPtr<CefValue> value =
			StreamElementsSceneItemsMonitor::GetSceneItemContextMenu(
				scene_item);

		if (value->GetType() == VTYPE_LIST &&
		    value->GetList()->GetSize()) {
			auto defaultAction = [monitor, scene_item]() {
				if (!scene_item)
					return;

				monitor->InvokeCurrentSceneItemDefaultAction(
					scene_item);
			};

			auto defaultContextMenu = [monitor, scene_item]() {
				if (!scene_item)
					return;

				monitor->InvokeCurrentSceneItemDefaultContextMenu(
					scene_item);
			};

			QMenu menu; //(m_mainWindow);

			handled = DeserializeMenu(value, menu, defaultAction,
						  defaultContextMenu);

			if (handled) {
				QContextMenuEvent *event =
					dynamic_cast<QContextMenuEvent *>(e);

				if (event)
					menu.exec(event->globalPos());
				else
					handled = false;
			}
		}
	}

	return handled;
	#else
	return false;
	#endif
}

static bool IsContextMenuAllowed() {
	if (StreamElementsConfig::GetInstance()->IsOnBoardingMode())
		return true;

	bool allowed = true;

	sceneitem_list_t *list = GetSelectedSceneItemsAddRef();

	if (list->size() > 1) {
		// Selected items list >1 - OBS default behavior if no item has custom
		// context menu.
		//
		// We can not disable context menu altogether due to "Group Items" menu
		// item which is special to multiple item selection.
		//
		for (auto scene_item : *list) {
			#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
			if (SceneItemHasCustomContextMenu(scene_item)) {
				// Item has custom context menu, we don't allow default context
				// menu in this case since it can lead to inconsistency.
				//
				// We also do not allow custom context menu in this case.
				allowed = false;
				break;
			}
			#endif

			if (!StreamElementsSceneItemsMonitor::
				    GetSceneItemUISettingsMultiselectContextMenuEnabled(
					    scene_item)) {
				// Context menu for multiple scene items is explicitly disabled
				// by uiSettings of any selected scene item: disable context menu
				// in this case.
				allowed = false;
				break;
			}
		}
	} else if (list->size() == 1) {
		// Single seelcted scene item
		allowed = true;
	}

	ReleaseSceneItemsList(list);

	return allowed;
}

static bool IsSceneItemSettingsActionAllowed()
{
	#if SE_ENABLE_SCENEITEM_DEFAULT_ACTION
	if (StreamElementsConfig::GetInstance()->IsOnBoardingMode())
		return true;

	bool allowed = true;

	sceneitem_list_t *list = GetSelectedSceneItemsAddRef();

	allowed = list->size() == 1;

	if (allowed) {
		allowed = StreamElementsSceneItemsMonitor::
			GetSceneItemUISettingsEnabled((*list)[0]);
	}

	if (allowed) {
		auto scene_item = (*list)[0];

		CefRefPtr<CefValue> value = StreamElementsSceneItemsMonitor::
			GetSceneItemDefaultAction(scene_item);

		if (value->GetType() == VTYPE_DICTIONARY) {
			// Default action override is in effect
			allowed = false;
		}
	}

	ReleaseSceneItemsList(list);

	return allowed;
	#else
	return true;
	#endif
}

static bool IsSceneItemReorderActionAllowed() {
	if (StreamElementsConfig::GetInstance()->IsOnBoardingMode())
		return true;

	bool allowed = true;

	sceneitem_list_t *list = GetSelectedSceneItemsAddRef();

	allowed = list->size() == 1;

	if (allowed) {
		allowed = StreamElementsSceneItemsMonitor::
			GetSceneItemUISettingsEnabled((*list)[0]);
	}

	ReleaseSceneItemsList(list);

	return allowed;
}

static class SceneItemsLocalEventFilter : public QObject {
private:
	QMainWindow *m_mainWindow;
	StreamElementsSceneItemsMonitor *m_monitor;

public:
	SceneItemsLocalEventFilter(QMainWindow *mainWindow,
				   StreamElementsSceneItemsMonitor *monitor)
		: m_mainWindow(mainWindow), m_monitor(monitor)
	{
	}

	~SceneItemsLocalEventFilter() {}

	virtual bool eventFilter(QObject *target, QEvent *e) override
	{
		if (!target)
			return false;

		if (e->type() != QEvent::MouseButtonDblClick &&
		    e->type() != QEvent::ContextMenu)
			return false;

		QMouseEvent *mouse = dynamic_cast<QMouseEvent *>(e);
		QContextMenuEvent *contextMenu =
			dynamic_cast<QContextMenuEvent *>(e);

		if (!mouse && !contextMenu)
			return false;

		if (target->objectName() == QString("preview")) {
			/* OBS preview pane */
			if (e->type() == QEvent::ContextMenu) {
				// Handle context menu for preview window events
				bool handled = !IsContextMenuAllowed();

				if (!handled) {
					sceneitem_list_t *list =
						GetSelectedSceneItemsAddRef();

					if (list->size() > 0) {
						// First selected scene item
						obs_sceneitem_t *scene_item =
							(*list)[0];

						handled =
							HandleSceneItemContextMenuRequest(
								scene_item,
								this->m_monitor,
								e);
					}

					ReleaseSceneItemsList(list);
				}

				return handled;
			}

			return false; // not handled
		}

		/* Make sure events belong to QListView item widgets */
		if (!target->parent() ||
		    !m_monitor->GetSceneItemsListView()->viewport() ||
		    target->parent() !=
			    m_monitor->GetSceneItemsListView()->viewport())
			return false;

		QWidget *o = dynamic_cast<QWidget *>(target);

		if (!o)
			return false;

		if (e->type() == QEvent::MouseButtonDblClick) {
			auto mouseEvent = dynamic_cast<QMouseEvent *>(e);

			if (mouseEvent->button() != Qt::LeftButton) {
				// No right mouse button doubleclicks
				return true;
			}

			QVariant bypassValue = o->property(
				WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT);
			if (bypassValue.isValid()) {
				if (bypassValue.value<int>() > 0) {
					bypassValue.setValue<int>(
						bypassValue.value<int>() - 1);

					o->setProperty(
						WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT,
						bypassValue);

					return false;
				}
			}

			obs_sceneitem_t *scene_item =
				GetObjectSceneItemAddRef(o);

			if (!scene_item)
				return false;

			bool handled = false;

			handled = HandleDefaultActionRequest(
				scene_item, this->m_monitor, e);

			if (scene_item)
				obs_sceneitem_release(scene_item);

			return handled;
		}

		if (e->type() == QEvent::ContextMenu) {
			obs_sceneitem_t *scene_item =
				GetObjectSceneItemAddRef(o);

			if (!scene_item)
				return false;

			bool handled = !IsContextMenuAllowed();

			if (!handled) {
				handled = HandleSceneItemContextMenuRequest(
					scene_item, this->m_monitor, e);
			}

			if (scene_item)
				obs_sceneitem_release(scene_item);

			return handled;
		}

		return false;
	}
};

/* ================================================================= */
/* ================================================================= */
/* ================================================================= */

StreamElementsSceneItemsMonitor::StreamElementsSceneItemsMonitor(
	QMainWindow *mainWindow)
	: m_mainWindow(mainWindow)
{
	m_eventFilter = new SceneItemsLocalEventFilter(m_mainWindow, this);

	QDockWidget *sceneItemsDock =
		(QDockWidget *)m_mainWindow->findChild<QDockWidget *>(
			"sourcesDock");

	QDockWidget *scenesDock =
		(QDockWidget *)m_mainWindow->findChild<QDockWidget *>(
			"scenesDock");

	if (!sceneItemsDock || !scenesDock)
		return;

	m_sceneItemsListView =
		(QListView *)sceneItemsDock->findChild<QListView *>("sources");

	if (!m_sceneItemsListView)
		return;

	QApplication::instance()->installEventFilter(m_eventFilter);
	//m_sceneItemsListView->viewport()->installEventFilter(m_eventFilter);
	//m_sceneItemsListView->installEventFilter(m_eventFilter);

	m_sceneItemsModel = m_sceneItemsListView->model();

	if (!m_sceneItemsModel)
		return;

	m_sceneItemsToolBar = (QToolBar *)sceneItemsDock->findChild<QToolBar *>(
		"sourcesToolbar");

	if (!m_sceneItemsToolBar)
		return;

	/* Subscribe to signals */

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	QObject::connect(
		m_sceneItemsModel, &QAbstractItemModel::modelReset, this,
		&StreamElementsSceneItemsMonitor::HandleSceneItemsModelReset);

	QObject::connect(m_sceneItemsModel, &QAbstractItemModel::rowsInserted,
			 this,
			 &StreamElementsSceneItemsMonitor::
				 HandleSceneItemsModelItemInsertedRemoved);

	QObject::connect(m_sceneItemsModel, &QAbstractItemModel::rowsRemoved,
			 this,
			 &StreamElementsSceneItemsMonitor::
				 HandleSceneItemsModelItemInsertedRemoved);

	QObject::connect(
		m_sceneItemsModel, &QAbstractItemModel::rowsMoved, this,
		&StreamElementsSceneItemsMonitor::HandleSceneItemsModelItemMoved);
	#endif

	/* ======= */

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	ScheduleUpdateSceneItemsWidgets();
	#endif

	m_sceneItemsToolBarActions->SetNull();

	UpdateSceneItemsToolbar();

	m_enableSignals = true;
}

StreamElementsSceneItemsMonitor::~StreamElementsSceneItemsMonitor()
{
	if (!m_sceneItemsListView)
		return;

	QApplication::instance()->removeEventFilter(m_eventFilter);
	//m_sceneItemsListView->viewport()->removeEventFilter(m_eventFilter);
	//m_sceneItemsListView->removeEventFilter(m_eventFilter);

	m_eventFilter->deleteLater();

	DisconnectSignalHandlers();
}

void StreamElementsSceneItemsMonitor::OnObsExit()
{
	m_enableSignals = false;

	m_updateSceneItemsWidgetsThrottledExecutive.Cancel();
}

void StreamElementsSceneItemsMonitor::DisconnectSignalHandlers()
{
	m_updateSceneItemsWidgetsThrottledExecutive.Cancel();

	if (!m_sceneItemsModel)
		return;

	/* Unsubscribe from signals */

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	QObject::disconnect(
		m_sceneItemsModel, &QAbstractItemModel::modelReset, this,
		&StreamElementsSceneItemsMonitor::HandleSceneItemsModelReset);

	QObject::disconnect(m_sceneItemsModel,
			    &QAbstractItemModel::rowsInserted, this,
			    &StreamElementsSceneItemsMonitor::
				    HandleSceneItemsModelItemInsertedRemoved);

	QObject::disconnect(m_sceneItemsModel, &QAbstractItemModel::rowsRemoved,
			    this,
			    &StreamElementsSceneItemsMonitor::
				    HandleSceneItemsModelItemInsertedRemoved);

	QObject::disconnect(
		m_sceneItemsModel, &QAbstractItemModel::rowsMoved, this,
		&StreamElementsSceneItemsMonitor::HandleSceneItemsModelItemMoved);
	#endif
}

void StreamElementsSceneItemsMonitor::HandleSceneItemsModelReset()
{
	if (!m_enableSignals)
		return;

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	ScheduleUpdateSceneItemsWidgets();
	#endif
}

void StreamElementsSceneItemsMonitor::HandleSceneItemsModelItemInsertedRemoved(
	const QModelIndex &, int, int)
{
	if (!m_enableSignals)
		return;

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	ScheduleUpdateSceneItemsWidgets();
	#endif
}

void StreamElementsSceneItemsMonitor::HandleSceneItemsModelItemMoved(
	const QModelIndex &, int, int, const QModelIndex &, int)
{
	if (!m_enableSignals)
		return;

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	ScheduleUpdateSceneItemsWidgets();
	#endif
}

#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
void StreamElementsSceneItemsMonitor::ScheduleUpdateSceneItemsWidgets()
{
	m_updateSceneItemsWidgetsThrottledExecutive.Signal(
		[this]() { UpdateSceneItemsWidgets(); }, 250);
}
#endif

typedef std::vector<obs_sceneitem_t *> sceneitems_vector_t;

static bool retrieveSceneItemsWithAddRef(obs_scene_t *, obs_sceneitem_t *item,
					 void *ptr)
{
	sceneitems_vector_t *items =
		reinterpret_cast<sceneitems_vector_t *>(ptr);

	if (obs_sceneitem_is_group(item)) {
		obs_data_t *data = obs_sceneitem_get_private_settings(item);

		/* WARNING: COMPATIBILITY: COMPAT: This relies on OBS internal data management.
		 *                                 May jeopardize compatibility with future releases
		 *                                 of OBS.
		 */
		bool collapse = obs_data_get_bool(data, "collapsed");
		if (!collapse) {
			obs_scene_t *scene =
				obs_sceneitem_group_get_scene(item);

			obs_scene_enum_items(
				scene, retrieveSceneItemsWithAddRef, items);
		}

		obs_data_release(data);
	}

	obs_sceneitem_addref(item);
	items->insert(items->begin(), item);
	return true;
};

static void release_sceneitems(sceneitems_vector_t *items)
{
	for (obs_sceneitem_t *item : *items) {
		obs_sceneitem_release(item);
	}
}

CefRefPtr<CefValue> StreamElementsSceneItemsMonitor::GetSceneItemPropertyValue(
	obs_sceneitem_t *scene_item, const char *key)
{
	CefRefPtr<CefValue> result = CefValue::Create();
	result->SetNull();

	if (!scene_item)
		return result;

	obs_data_t *scene_item_private_data =
		obs_sceneitem_get_private_settings(scene_item);

	if (!scene_item_private_data)
		return result;

	const char *json = obs_data_get_string(scene_item_private_data, key);

	if (json) {
		result = CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);
	}

	obs_data_release(scene_item_private_data);

	if (!result.get()) {
		result = CefValue::Create();

		result->SetNull();
	}

	return result;
}

void StreamElementsSceneItemsMonitor::SetSceneItemPropertyValue(
	obs_sceneitem_t *scene_item, const char *key, CefRefPtr<CefValue> value,
	bool triggerUpdate)
{
	obs_data_t *scene_item_private_data =
		obs_sceneitem_get_private_settings(scene_item);

	std::string json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();

	obs_data_set_string(scene_item_private_data, key, json.c_str());

	obs_data_release(scene_item_private_data);

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	if (triggerUpdate) {
		ScheduleUpdateSceneItemsWidgets();
	}
	#endif
}

#if SE_ENABLE_SCENEITEM_ICONS
CefRefPtr<CefValue>
StreamElementsSceneItemsMonitor::GetSceneItemIcon(obs_sceneitem_t *scene_item)
{
	CefRefPtr<CefValue> iconValue = GetSceneItemPropertyValue(
		scene_item, ITEM_PRIVATE_DATA_KEY_UI_ICON);

	if (iconValue->GetType() != VTYPE_DICTIONARY) {
		iconValue->SetNull();
	}

	return iconValue;
}

void StreamElementsSceneItemsMonitor::SetSceneItemIcon(
	obs_sceneitem_t *scene_item, CefRefPtr<CefValue> m_icon)
{
	SetSceneItemPropertyValue(scene_item, ITEM_PRIVATE_DATA_KEY_UI_ICON,
				  m_icon);
}
#endif

#if SE_ENABLE_SCENEITEM_ACTIONS
CefRefPtr<CefListValue> StreamElementsSceneItemsMonitor::GetSceneItemActions(
	obs_sceneitem_t *scene_item)
{
	CefRefPtr<CefValue> actionsValue = GetSceneItemPropertyValue(
		scene_item, ITEM_PRIVATE_DATA_KEY_UI_AUXILIARY_ACTIONS);

	if (actionsValue->GetType() != VTYPE_LIST) {
		actionsValue->SetList(CefListValue::Create());
	}

	return actionsValue->GetList();
}

void StreamElementsSceneItemsMonitor::SetSceneItemActions(
	obs_sceneitem_t *scene_item, CefRefPtr<CefListValue> m_actions)
{
	CefRefPtr<CefValue> val = CefValue::Create();

	val->SetList(m_actions);

	SetSceneItemPropertyValue(
		scene_item, ITEM_PRIVATE_DATA_KEY_UI_AUXILIARY_ACTIONS, val);
}
#endif

#if SE_ENABLE_SCENEITEM_DEFAULT_ACTION
CefRefPtr<CefValue> StreamElementsSceneItemsMonitor::GetSceneItemDefaultAction(
	obs_sceneitem_t *scene_item)
{
	return GetSceneItemPropertyValue(
		scene_item, ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION);
}

void StreamElementsSceneItemsMonitor::SetSceneItemDefaultAction(
	obs_sceneitem_t *scene_item, CefRefPtr<CefValue> action)
{
	SetSceneItemPropertyValue(
		scene_item, ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION, action);
}
#endif

#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
CefRefPtr<CefValue> StreamElementsSceneItemsMonitor::GetSceneItemContextMenu(
	obs_sceneitem_t *scene_item)
{
	return GetSceneItemPropertyValue(scene_item,
					 ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU);
}

void StreamElementsSceneItemsMonitor::SetSceneItemContextMenu(
	obs_sceneitem_t *scene_item, CefRefPtr<CefValue> menu)
{
	SetSceneItemPropertyValue(scene_item,
				  ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU, menu);
}
#endif

CefRefPtr<CefValue> StreamElementsSceneItemsMonitor::GetSceneItemAuxiliaryData(
	obs_sceneitem_t *scene_item)
{
	return GetSceneItemPropertyValue(scene_item,
					 ITEM_PRIVATE_DATA_KEY_APPDATA);
}

void StreamElementsSceneItemsMonitor::SetSceneItemAuxiliaryData(
	obs_sceneitem_t *scene_item, CefRefPtr<CefValue> data)
{
	SetSceneItemPropertyValue(scene_item, ITEM_PRIVATE_DATA_KEY_APPDATA,
				  data, false);
}

#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
CefRefPtr<CefValue> StreamElementsSceneItemsMonitor::GetSceneItemUISettings(
	obs_sceneitem_t *scene_item)
{
	return GetSceneItemPropertyValue(scene_item, ITEM_PRIVATE_DATA_KEY_UI_SETTINGS);
}
#endif

bool StreamElementsSceneItemsMonitor::GetSceneItemUISettingsEnabled(
	obs_sceneitem_t *scene_item)
{
	#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
	CefRefPtr<CefValue> settings = GetSceneItemUISettings(scene_item);

	if (!settings || settings->GetType() != VTYPE_DICTIONARY)
		return true;

	CefRefPtr<CefDictionaryValue> d = settings->GetDictionary();

	if (d->HasKey("enabled") && d->GetType("enabled") == VTYPE_BOOL)
		return d->GetBool("enabled");
	else
		return true;
	#else
	return true;
	#endif
}

bool StreamElementsSceneItemsMonitor::GetSceneItemUISettingsMultiselectContextMenuEnabled(
	obs_sceneitem_t *scene_item)
{
	#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
	CefRefPtr<CefValue> settings = GetSceneItemUISettings(scene_item);

	if (!settings || settings->GetType() != VTYPE_DICTIONARY)
		return true;

	CefRefPtr<CefDictionaryValue> d = settings->GetDictionary();

	if (d->HasKey("multipleItemsContextMenuEnabled") &&
	    d->GetType("multipleItemsContextMenuEnabled") == VTYPE_BOOL)
		return d->GetBool("multipleItemsContextMenuEnabled");
	else
		return true;
	#else
	return true;
	#endif
}

#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
void StreamElementsSceneItemsMonitor::SetSceneItemUISettings(
	obs_sceneitem_t *scene_item, CefRefPtr<CefValue> data)
{
	SetSceneItemPropertyValue(scene_item,
				  ITEM_PRIVATE_DATA_KEY_UI_SETTINGS,
				  data, false);
}
#endif

static void deserializeAuxSceneItemsControls(
	StreamElementsSceneItemsMonitor *monitor, obs_sceneitem_t *scene_item,
	QWidget *auxWidget, QWidget *parentWidget, QLabel *nativeIconLabel)
{
#if SE_ENABLE_SCENEITEM_ACTIONS
	auxWidget->setSizePolicy(QSizePolicy::Preferred,
				 QSizePolicy::MinimumExpanding);

	auxWidget->setStyleSheet("background: none");
	auxWidget->setAutoFillBackground(false);

	QHBoxLayout *auxLayout =
		static_cast<QHBoxLayout *>(auxWidget->layout());

	if (!auxLayout) {
		auxLayout = new QHBoxLayout();
		auxLayout->setContentsMargins(0, 0, 0, 0);
		auxWidget->setLayout(auxLayout);
	}

	while (auxLayout->count()) {
		auxLayout->removeItem(auxLayout->itemAt(0));
	}

	CefRefPtr<CefListValue> list = monitor->GetSceneItemActions(scene_item);

	for (size_t index = 0; index < list->GetSize(); ++index) {
		QWidget *control = DeserializeAuxiliaryControlWidget(
			list->GetValue(index),
			[monitor, scene_item]() {
				monitor->InvokeCurrentSceneItemDefaultAction(
					scene_item);
			},
			[monitor, scene_item]() {
				monitor->InvokeCurrentSceneItemDefaultContextMenu(
					scene_item);
			});

		if (!control)
			continue;

		auxLayout->addWidget(control);

		if (obs_get_version() >= MAKE_SEMANTIC_VERSION(24, 0, 0)) {
			/* OBS 24 added spacing between action icons */
			auxLayout->addSpacing(2);
		}
	}
#endif
}

static void deserializeSceneItemIcon(StreamElementsSceneItemsMonitor *monitor,
				     obs_scene_t *scene,
				     obs_sceneitem_t *scene_item,
				     QWidget *iconWidget, QWidget *parentWidget,
				     QPixmap *defaultPixmap,
				     QLabel *nativeIconLabel)
{
	#if SE_ENABLE_SCENEITEM_ICONS
	iconWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

	QHBoxLayout *iconLayout =
		static_cast<QHBoxLayout *>(iconWidget->layout());

	if (!iconLayout) {
		iconLayout = new QHBoxLayout();
		iconLayout->setContentsMargins(0, 0, 0, 0);
		iconWidget->setLayout(iconLayout);
	}

	while (iconLayout->count()) {
		iconLayout->removeItem(iconLayout->itemAt(0));
	}

	std::string url = "";

	QWidget *itemIcon = DeserializeRemoteIconWidget(
		monitor->GetSceneItemIcon(scene_item), defaultPixmap);

	itemIcon->setAutoFillBackground(false);
	itemIcon->setStyleSheet("background: none");

	iconLayout->addWidget(itemIcon);
	if (!nativeIconLabel) {
		iconLayout->addSpacing(2);
	} else {
		nativeIconLabel->setAutoFillBackground(false);
		nativeIconLabel->setStyleSheet("background: none;");
	}
	#endif
}

#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
static void
deserializeSceneItemUISettings(StreamElementsSceneItemsMonitor *monitor,
			       obs_scene_t *scene, obs_sceneitem_t *scene_item,
			       QWidget *widget,
			       QLabel *nativeTextLabel, bool isSignedIn)
{
	double opacity = 1.0;
	bool enabled = true;
	QPalette textPallette = qApp->palette("QLabel");

	if (isSignedIn) {
		CefRefPtr<CefValue> settings =
			monitor->GetSceneItemUISettings(scene_item);

		if (!!settings && settings->GetType() == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> d =
				settings->GetDictionary();

			if (d->HasKey("opacity") &&
			    d->GetType("opacity") == VTYPE_DOUBLE) {
				opacity = max(min(d->GetDouble("opacity"), 1.0),
					      0.0);
			}

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (nativeTextLabel) {
				if (d->HasKey("color") &&
				    d->GetType("color") == VTYPE_STRING) {
					textPallette.setColor(
						nativeTextLabel
							->foregroundRole(),
						QColor(d->GetString("color")
							       .c_str()));
				}
			}
		}
	}

	if (opacity < 1.0) {
		auto effect = new QGraphicsOpacityEffect(widget);
		effect->setOpacity(opacity);
		widget->setGraphicsEffect(effect);
	} else {
		widget->setGraphicsEffect(nullptr);
	}

	widget->setEnabled(enabled);

	if (nativeTextLabel) {
		std::string color = textPallette.color(nativeTextLabel->foregroundRole())
			.name()
			.toStdString();

		std::string css = "QLabel { color: ";
		css += color;
		css += "; }";

		nativeTextLabel->setStyleSheet(css.c_str());

		nativeTextLabel->setSizePolicy(QSizePolicy::MinimumExpanding, QSizePolicy::MinimumExpanding);
	}
}
#endif

#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
void StreamElementsSceneItemsMonitor::UpdateSceneItemsWidgets()
{
	if (!m_sceneItemsModel)
		return;

	sceneitems_vector_t sceneItems;

	obs_source_t *sceneSource = obs_frontend_get_current_scene();

	if (!sceneSource)
		return;

	obs_scene_t *scene = obs_scene_from_source(
		sceneSource); // does not increment refcount

	obs_scene_enum_items(scene, retrieveSceneItemsWithAddRef, &sceneItems);

	bool isSignedIn =
		!StreamElementsConfig::GetInstance()->IsOnBoardingMode();

	for (auto toolButton : m_sceneItemsToolBar->findChildren<QToolButton *>()) {
		auto action = toolButton->defaultAction();

		if (!action)
			continue;

		bool enabled = true;

		if (action->objectName() == "actionSourceProperties") {
			enabled = IsSceneItemSettingsActionAllowed();
		} else if (action->objectName() == "actionSourceUp" ||
			   action->objectName() == "actionSourceDown") {
			enabled = IsSceneItemReorderActionAllowed();
		}

		toolButton->setEnabled(enabled);

		if (enabled) {
			toolButton->setGraphicsEffect(nullptr);
		} else {
			auto effect = new QGraphicsOpacityEffect(toolButton);
			effect->setOpacity(0.2);
			toolButton->setGraphicsEffect(effect);
		}
	}

	for (int rowIndex = 0; rowIndex < m_sceneItemsModel->rowCount() &&
			       rowIndex < sceneItems.size();
	     ++rowIndex) {
		obs_sceneitem_t *scene_item = sceneItems[rowIndex];

		QModelIndex index = m_sceneItemsModel->index(rowIndex, 0);
		QWidget *widget = m_sceneItemsListView->indexWidget(index);

		if (!widget)
			continue;

		QBoxLayout *layout =
			dynamic_cast<QBoxLayout *>(widget->layout());

		if (!layout)
			continue;

		{
			QList<QWidget *> list = widget->findChildren<QWidget *>(
				"streamelements_aux_widget",
				Qt::FindChildrenRecursively);

			for (QWidget *w : list) {
				w->setVisible(false);
				layout->removeWidget(w);
			}
		}

		{
			QList<QWidget *> list = widget->findChildren<QWidget *>(
				"streamelements_icon_widget",
				Qt::FindChildrenRecursively);

			for (QWidget *w : list) {
				w->setVisible(false);
				layout->removeWidget(w);
			}
		}

		QPixmap defaultIconPixmap(16, 16);
		defaultIconPixmap.fill(Qt::transparent);

		QLabel *nativeIconLabel = nullptr;
		QLabel *nativeTextLabel = nullptr;

		{
			QList<QLabel *> list = widget->findChildren<QLabel *>(
				QString(), Qt::FindDirectChildrenOnly);

			for (QLabel *label : list) {
#if QT_VERSION_MAJOR >= 6
				if (!label->pixmap().isNull()) {
#else
				if (label->pixmap()) {
#endif
					// Native icon
					label->setVisible(!isSignedIn);

					if (!nativeIconLabel) {
						nativeIconLabel = label;
					}

					if (isSignedIn) {
						defaultIconPixmap =
#if QT_VERSION_MAJOR >= 6
							label->pixmap();
#else
							*label->pixmap();
#endif
					}
				} else {
					nativeTextLabel = label;
				}
			}
		}

		if (isSignedIn) {
			/* Create and add icon widget to the item's layout */

			QWidget *iconWidget = new QWidget();
			iconWidget->setObjectName("streamelements_icon_widget");

			if (nativeIconLabel) {
				/* Only if native icon is enabled */
				layout->insertWidget(
					layout->indexOf(nativeIconLabel),
					iconWidget);
			}

			/* Create and add auxiliary actions widget to the item's layout */

			QWidget *auxWidget = new QWidget();
			auxWidget->setObjectName("streamelements_aux_widget");

			int auxWidgetPos =
				nativeTextLabel
					? layout->indexOf(nativeTextLabel) + 1
					: widget->children().count() >= 6
						  ? layout->count() -
							    3 // OBS 25+ has an icon
						  : layout->count() -
							    2; // OBS 24- has no icon

			layout->insertWidget(auxWidgetPos, auxWidget);

			deserializeSceneItemIcon(this, scene, scene_item,
						 iconWidget, widget,
						 &defaultIconPixmap,
						 nativeIconLabel);

			deserializeAuxSceneItemsControls(this, scene_item,
							 auxWidget, widget,
							 nativeIconLabel);

			deserializeSceneItemUISettings(this, scene, scene_item,
						       widget, nativeTextLabel,
						       isSignedIn);

			{
				QVariant value(QString(
					GetIdFromPointer(scene_item).c_str()));

				widget->setProperty(WIDGET_PROP_SCENE_ITEM_ID,
						    value);
			}
		}
	}

	release_sceneitems(&sceneItems);
	obs_source_release(sceneSource);
}
#endif

bool StreamElementsSceneItemsMonitor::InvokeCurrentSceneItemDefaultAction(
	obs_sceneitem_t *scene_item)
{
	bool result = false;

	sceneitems_vector_t sceneItems;

	obs_source_t *sceneSource = obs_frontend_get_current_scene();

	if (!sceneSource)
		return false;

	obs_scene_t *scene = obs_scene_from_source(
		sceneSource); // does not increment refcount

	obs_scene_enum_items(scene, retrieveSceneItemsWithAddRef, &sceneItems);

	for (int rowIndex = 0; rowIndex < m_sceneItemsModel->rowCount() &&
			       rowIndex < sceneItems.size() && !result;
	     ++rowIndex) {
		obs_sceneitem_t *item = sceneItems[rowIndex];

		if (scene_item != item)
			continue;

		QModelIndex index = m_sceneItemsModel->index(rowIndex, 0);
		QWidget *widget = m_sceneItemsListView->indexWidget(index);

		if (!widget)
			continue;

		QVariant value = widget->property(
			WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT);
		if (!value.isValid()) {
			value.setValue<int>(1);
		} else {
			value.setValue<int>(value.value<int>() + 1);
		}

		widget->setProperty(WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT,
				    value);

		QApplication::sendEvent(
			widget,
			new QMouseEvent(QMouseEvent::MouseButtonDblClick,
					QPoint(0, 0), Qt::LeftButton,
					Qt::NoButton, Qt::NoModifier));

		result = true;
	}

	release_sceneitems(&sceneItems);
	obs_source_release(sceneSource);

	return result;
}

bool StreamElementsSceneItemsMonitor::InvokeCurrentSceneItemDefaultContextMenu(
	obs_sceneitem_t *scene_item)
{
	bool result = false;

	sceneitems_vector_t sceneItems;

	obs_source_t *sceneSource = obs_frontend_get_current_scene();

	if (!sceneSource)
		return false;

	obs_scene_t *scene = obs_scene_from_source(
		sceneSource); // does not increment refcount

	obs_scene_enum_items(scene, retrieveSceneItemsWithAddRef, &sceneItems);

	for (int rowIndex = 0; rowIndex < m_sceneItemsModel->rowCount() &&
			       rowIndex < sceneItems.size() && !result;
	     ++rowIndex) {
		obs_sceneitem_t *item = sceneItems[rowIndex];

		if (scene_item != item)
			continue;

		QModelIndex index = m_sceneItemsModel->index(rowIndex, 0);
		QWidget *widget = m_sceneItemsListView->indexWidget(index);

		if (!widget)
			continue;

		m_sceneItemsListView->scrollTo(index);
		m_sceneItemsListView->setCurrentIndex(index);

		const QRect rect = m_sceneItemsListView->visualRect(index);

		QtPostTask([=]() {
			m_sceneItemsListView->customContextMenuRequested(
				QPoint(rect.left() + (rect.width() / 2),
				       rect.top() + (rect.height() / 2)));
		});

		result = true;
	}

	release_sceneitems(&sceneItems);
	obs_source_release(sceneSource);

	return result;
}

void StreamElementsSceneItemsMonitor::UpdateSceneItemsToolbar()
{
	if (!IsSupportedOBSVersion())
		return;

	QWidget *widget = m_sceneItemsToolBar->findChild<QWidget *>(
		"streamelements_sources_toolbar_aux_actions");

	if (widget)
		m_sceneItemsToolBar->layout()->removeWidget(widget);

	widget = new QWidget();
	widget->setObjectName("streamelements_sources_toolbar_aux_actions");

	QHBoxLayout *layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	widget->setLayout(layout);

	layout->addSpacerItem(new QSpacerItem(16, 16, QSizePolicy::Expanding,
					      QSizePolicy::Minimum));

	if (!m_sceneItemsToolBarActions.get() ||
	    m_sceneItemsToolBarActions->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list = m_sceneItemsToolBarActions->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		QWidget *control = DeserializeAuxiliaryControlWidget(
			list->GetValue(index));

		if (!control)
			continue;

		layout->addWidget(control);
		layout->addSpacing(1);
	}

	widget->setStyleSheet("background: none");

	widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

	m_sceneItemsToolBar->addWidget(widget);

	QMainWindow *mainWindow = (QMainWindow *)obs_frontend_get_main_window();

	QDockWidget *sceneItemsDock =
		(QDockWidget *)mainWindow->findChild<QDockWidget *>(
			"sourcesDock");

	if (list->GetSize()) {
		sceneItemsDock->setFixedWidth(
			m_sceneItemsToolBar->sizeHint().width());
		
		QtPostTask([sceneItemsDock, this]() {
			sceneItemsDock->setMinimumWidth(
				m_sceneItemsToolBar->sizeHint().width());

			sceneItemsDock->setMaximumWidth(16777215); // default according to Qt docs
		});
	} else {
		sceneItemsDock->setMinimumWidth(0);
	}
}
