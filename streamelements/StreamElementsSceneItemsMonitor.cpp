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

static const char *ITEM_PRIVATE_DATA_KEY_UI_AUXILIARY_ACTIONS =
	"streamelements_aux_ui_actions";

static const char *ITEM_PRIVATE_DATA_KEY_UI_ICON = "streamelements_ui_icon";

static const char *ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION =
	"streamelements_ui_default_action";

static const char *ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU =
	"streamelements_ui_context_menu";

static const char *ITEM_PRIVATE_DATA_KEY_APPDATA = "streamelements_app_data";

static const char *WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT =
	"streamelements_default_action_bypass_count";

static const char *WIDGET_PROP_SCENE_ITEM_ID = "streamelements_scene_item_id";

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

		/* Make sure events belong to QListView item widgets */
		if (!target->parent() ||
		    !m_monitor->GetSceneItemsListView()->viewport() ||
		    target->parent() !=
			    m_monitor->GetSceneItemsListView()->viewport())
			return false;

		QWidget *o = dynamic_cast<QWidget *>(target);
		/*
		QWidget *o = m_monitor->GetWidgetAtLocalPosition(
			mouse ? mouse->localPos() : contextMenu->pos());*/

		if (!o)
			return false;

		if (e->type() == QEvent::MouseButtonDblClick) {
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

			CefRefPtr<CefValue> value =
				StreamElementsSceneItemsMonitor::
					GetSceneItemDefaultAction(scene_item);

			if (value->GetType() == VTYPE_DICTIONARY) {
				auto defaultAction = [this, scene_item]() {
					if (!scene_item)
						return;

					this->m_monitor
						->InvokeCurrentSceneItemDefaultAction(
							scene_item);
				};

				auto defaultContextMenu = [this, scene_item]() {
					if (!scene_item)
						return;

					this->m_monitor
						->InvokeCurrentSceneItemDefaultAction(
							scene_item);
				};

				handled = DeserializeAndInvokeAction(
					value, defaultAction,
					defaultContextMenu);

				if (scene_item)
					obs_sceneitem_release(scene_item);
			}

			return handled;
		}

		if (e->type() == QEvent::ContextMenu) {
			obs_sceneitem_t *scene_item =
				GetObjectSceneItemAddRef(o);

			if (!scene_item)
				return false;

			CefRefPtr<CefValue> value =
				StreamElementsSceneItemsMonitor::
					GetSceneItemContextMenu(scene_item);

			bool handled = false;

			if (value->GetType() == VTYPE_LIST &&
			    value->GetList()->GetSize()) {
				auto defaultAction = [this, scene_item]() {
					if (!scene_item)
						return;

					this->m_monitor
						->InvokeCurrentSceneItemDefaultAction(
							scene_item);
				};

				auto defaultContextMenu = [this, scene_item]() {
					if (!scene_item)
						return;

					this->m_monitor
						->InvokeCurrentSceneItemDefaultContextMenu(
							scene_item);
				};

				QMenu menu; //(m_mainWindow);

				handled = DeserializeMenu(value, menu,
							  defaultAction,
							  defaultContextMenu);

				if (handled) {
					QContextMenuEvent *event = dynamic_cast<
						QContextMenuEvent *>(e);

					if (event)
						menu.exec(event->globalPos());
					else
						handled = false;
				}
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

	/* ======= */

	ScheduleUpdateSceneItemsWidgets();

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
}

void StreamElementsSceneItemsMonitor::HandleSceneItemsModelReset()
{
	if (!m_enableSignals)
		return;

	ScheduleUpdateSceneItemsWidgets();
}

void StreamElementsSceneItemsMonitor::HandleSceneItemsModelItemInsertedRemoved(
	const QModelIndex &, int, int)
{
	if (!m_enableSignals)
		return;

	ScheduleUpdateSceneItemsWidgets();
}

void StreamElementsSceneItemsMonitor::HandleSceneItemsModelItemMoved(
	const QModelIndex &, int, int, const QModelIndex &, int)
{
	if (!m_enableSignals)
		return;

	ScheduleUpdateSceneItemsWidgets();
}

void StreamElementsSceneItemsMonitor::ScheduleUpdateSceneItemsWidgets()
{
	m_updateSceneItemsWidgetsThrottledExecutive.Signal(
		[this]() { UpdateSceneItemsWidgets(); }, 250);
}

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

	if (triggerUpdate) {
		ScheduleUpdateSceneItemsWidgets();
	}
}

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

static void deserializeAuxSceneItemsControls(
	StreamElementsSceneItemsMonitor *monitor, obs_sceneitem_t *scene_item,
	QWidget *auxWidget, QWidget *parentWidget, QLabel *nativeIconLabel)
{
	auxWidget->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

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
}

static void deserializeSceneItemIcon(StreamElementsSceneItemsMonitor *monitor,
				     obs_scene_t *scene,
				     obs_sceneitem_t *scene_item,
				     QWidget *iconWidget, QWidget *parentWidget,
				     QPixmap *defaultPixmap,
				     QLabel *nativeIconLabel)
{
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

	iconLayout->addWidget(itemIcon);
	if (!nativeIconLabel) {
		iconLayout->addSpacing(2);
	}
}

void StreamElementsSceneItemsMonitor::UpdateSceneItemsWidgets()
{
	sceneitems_vector_t sceneItems;

	obs_source_t *sceneSource = obs_frontend_get_current_scene();
	obs_scene_t *scene = obs_scene_from_source(
		sceneSource); // does not increment refcount

	obs_scene_enum_items(scene, retrieveSceneItemsWithAddRef,
				&sceneItems);

	for (int rowIndex = 0;
		rowIndex < m_sceneItemsModel->rowCount() &&
		rowIndex < sceneItems.size();
		++rowIndex) {
		obs_sceneitem_t *scene_item = sceneItems[rowIndex];

		QModelIndex index =
			m_sceneItemsModel->index(rowIndex, 0);
		QWidget *widget =
			m_sceneItemsListView->indexWidget(index);

		if (!widget)
			continue;

		QBoxLayout *layout =
			dynamic_cast<QBoxLayout *>(widget->layout());

		if (!layout)
			continue;

		bool isSignedIn = !StreamElementsConfig::GetInstance()
						->IsOnBoardingMode();

		{
			QList<QWidget *> list =
				widget->findChildren<QWidget *>(
					"streamelements_aux_widget",
					Qt::FindChildrenRecursively);

			for (QWidget *w : list) {
				w->setVisible(false);
				layout->removeWidget(w);
			}
		}

		{
			QList<QWidget *> list =
				widget->findChildren<QWidget *>(
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
			QList<QLabel *> list =
				widget->findChildren<QLabel *>(
					QString(),
					Qt::FindDirectChildrenOnly);

			for (QLabel *label : list) {
				if (label->pixmap()) {
					// Native icon
					label->setVisible(!isSignedIn);

					if (!nativeIconLabel) {
						nativeIconLabel = label;
					}

					if (isSignedIn) {
						defaultIconPixmap =
							*label->pixmap();
					}
				} else {
					nativeTextLabel = label;
				}
			}
		}

		if (isSignedIn) {
			/* Create and add icon widget to the item's layout */

			QWidget *iconWidget = new QWidget();
			iconWidget->setObjectName(
				"streamelements_icon_widget");

			if (nativeIconLabel) {
				/* Only if native icon is enabled */
				layout->insertWidget(
					layout->indexOf(
						nativeIconLabel),
					iconWidget);
			}

			/* Create and add auxiliary actions widget to the item's layout */

			QWidget *auxWidget = new QWidget();
			auxWidget->setObjectName(
				"streamelements_aux_widget");

			int auxWidgetPos =
				nativeTextLabel
					? layout->indexOf(
							nativeTextLabel) +
							1
					: widget->children().count() >=
								6
							? layout->count() -
								3 // OBS 25+ has an icon
							: layout->count() -
								2; // OBS 24- has no icon

#ifndef __APPLE__
			layout->insertWidget(auxWidgetPos, auxWidget);
#else
			layout->insertWidget(auxWidgetPos - 1,
						auxWidget);
#endif

			deserializeSceneItemIcon(this, scene,
							scene_item, iconWidget,
							widget,
							&defaultIconPixmap,
							nativeIconLabel);

			deserializeAuxSceneItemsControls(
				this, scene_item, auxWidget, widget,
				nativeIconLabel);

			{
				QVariant value(QString(
					GetIdFromPointer(scene_item)
						.c_str()));

				widget->setProperty(
					WIDGET_PROP_SCENE_ITEM_ID,
					value);
			}
		}
	}

	release_sceneitems(&sceneItems);
	obs_source_release(sceneSource);
}

bool StreamElementsSceneItemsMonitor::InvokeCurrentSceneItemDefaultAction(
	obs_sceneitem_t *scene_item)
{
	bool result = false;

	sceneitems_vector_t sceneItems;

	obs_source_t *sceneSource = obs_frontend_get_current_scene();
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
	QWidget *widget = m_sceneItemsToolBar->findChild<QWidget *>(
		"streamelements_sources_toolbar_aux_actions");

	if (widget)
		m_sceneItemsToolBar->layout()->removeWidget(widget);

	widget = new QWidget();
	widget->setObjectName(
		"streamelements_sources_toolbar_aux_actions");

	QHBoxLayout *layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	widget->setLayout(layout);

	layout->addSpacerItem(new QSpacerItem(
		16, 16, QSizePolicy::Expanding, QSizePolicy::Minimum));

	if (!m_sceneItemsToolBarActions.get() ||
		m_sceneItemsToolBarActions->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list =
		m_sceneItemsToolBarActions->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		QWidget *control = DeserializeAuxiliaryControlWidget(
			list->GetValue(index));

		if (!control)
			continue;

		layout->addWidget(control);
		layout->addSpacing(1);
	}

	widget->setStyleSheet("background: none");

	widget->setSizePolicy(QSizePolicy::Expanding,
				QSizePolicy::Minimum);

	m_sceneItemsToolBar->addWidget(widget);
}
