#include "StreamElementsScenesListWidgetManager.hpp"
#include "StreamElementsUtils.hpp"
#include "StreamElementsRemoteIconLoader.hpp"
#include "StreamElementsConfig.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QDockWidget>
#include <QLayout>
#include <QSizePolicy>
#include <QLabel>
#include <QPushButton>
#include <QAbstractListModel>
#include <QMouseEvent>

#include <map>
#include <unordered_map>

static const char *ITEM_PRIVATE_DATA_KEY_UI_ICON = "streamelements_ui_icon";

static const char *ITEM_PRIVATE_DATA_KEY_APPDATA = "streamelements_app_data";

static const char *WIDGET_PROP_SCENE_ID = "streamelements_scene_id";

static const char *WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT =
	"streamelements_default_action_bypass_count";

static const char *ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION =
	"streamelements_ui_default_action";

static const char *ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU =
	"streamelements_ui_context_menu";

/* ================================================================= */
/* ================================================================= */
/* ================================================================= */

class ItemData : public QObject {
private:
	QIcon m_icon;
	CefRefPtr<CefValue> m_defaultAction;
	CefRefPtr<CefValue> m_contextMenu;
	CefRefPtr<StreamElementsRemoteIconLoader> m_iconLoader = nullptr;
	std::function<void()> m_updateListener;

	void ClearIcon()
	{
		QPixmap pm(16, 16);
		pm.fill(Qt::transparent);
		m_icon = QIcon(pm);
	}

public:
	ItemData(std::function<void()> updateListener)
		: m_updateListener(updateListener),
		  m_defaultAction(CefValue::Create()),
		  m_contextMenu(CefValue::Create())
	{
		ClearIcon();
	}

	~ItemData()
	{
		if (m_iconLoader)
			m_iconLoader->Cancel();
	}

	QIcon icon() { return m_icon; }
	CefRefPtr<CefValue> defaultAction() { return m_defaultAction; }
	CefRefPtr<CefValue> contextMenu() { return m_contextMenu; }

	void LoadIconUrl(std::string url)
	{
		if (m_iconLoader)
			m_iconLoader->Cancel();

		m_iconLoader = StreamElementsRemoteIconLoader::Create(
			[this](const QIcon &newIcon) {
				m_icon = QIcon(newIcon);

				m_updateListener();
			},
			nullptr);

		m_iconLoader->LoadUrl(url.c_str());
	}

	void DeserializeDefaultAction(CefRefPtr<CefValue> input)
	{
		m_defaultAction = input->Copy();
	}

	void DeserializeContextMenu(CefRefPtr<CefValue> input)
	{
		m_contextMenu = input->Copy();
	}

	void DeserializeIcon(CefRefPtr<CefValue> input)
	{
		ClearIcon();

		QtPostTask([this]() { m_updateListener(); });

		if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
			return;

		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (!d->HasKey("url") || d->GetType("url") != VTYPE_STRING)
			return;

		LoadIconUrl(d->GetString("url").ToString());
	}
};

class SourceDataManager : public QObject {
private:
	std::map<obs_source_t *, ItemData *> m_itemData;

protected:
	SourceDataManager() {}

public:
	void clear()
	{
		for (auto kv : m_itemData) {
			kv.second->deleteLater();
		}

		m_itemData.clear();
	}

	void removeData(obs_source_t *key)
	{
		if (!m_itemData.count(key))
			return;

		m_itemData[key]->deleteLater();
		m_itemData.erase(key);
	}

	void removeDataNotInList(obs_frontend_source_list sources)
	{
		std::unordered_map<obs_source_t *, bool> sourcesMap;
		for (size_t i = 0; i < sources.sources.num; ++i) {
			sourcesMap[sources.sources.array[i]] = true;
		}

		std::vector<obs_source_t *> deleteList;

		for (auto kv : m_itemData) {
			if (!sourcesMap.count(kv.first)) {
				deleteList.emplace_back(kv.first);
			}
		}

		for (obs_source_t *key : deleteList) {
			removeData(key);
		}
	}

	void setData(obs_source_t *key, ItemData *value)
	{
		removeData(key);

		m_itemData[key] = value;
	}

	ItemData *data(obs_source_t *key) const
	{
		for (auto kv : m_itemData) {
			if (kv.first == key)
				return kv.second;
		}

		return nullptr;
	}

	static SourceDataManager *instance() { return &s_instance; }

private:
	static SourceDataManager s_instance;
};

SourceDataManager SourceDataManager::s_instance;

/* ================================================================= */
/* ================================================================= */
/* ================================================================= */

static class ScenesLocalEventFilter : public QObject {
private:
	StreamElementsScenesListWidgetManager *m_manager;

public:
	ScenesLocalEventFilter(StreamElementsScenesListWidgetManager *manager)
		: m_manager(manager)
	{
	}

	~ScenesLocalEventFilter() {}

protected:
	virtual bool eventFilter(QObject *o, QEvent *e) override
	{
		if (!o)
			return false;

		if (e->type() == QEvent::Paint) {
			/* Every time the widget is painted, check whether it's viewMode()
			 * has changed.
			 *
			 * If so, widgets must be updated with features supported by each
			 * viewMode()
			 */
			m_manager->CheckViewMode();
		}

		if (e->type() != QEvent::MouseButtonDblClick && e->type() != QEvent::ContextMenu)
			return false;

		if (!m_manager->GetScenesListWidget() ||
		    !m_manager->GetScenesListWidget()->viewport() ||
		    (o != m_manager->GetScenesListWidget()->viewport() &&
		     o != m_manager->GetScenesListWidget()))
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

			obs_source_t *scene = obs_frontend_get_current_scene();

			if (!scene)
				return false;

			bool handled = false;

			CefRefPtr<CefValue> value =
				m_manager->GetSceneDefaultAction(scene);

			if (value->GetType() == VTYPE_DICTIONARY) {
				auto defaultAction = [this, scene]() {
					if (!scene)
						return;

					m_manager->InvokeCurrentSceneDefaultAction();
				};

				auto defaultContextMenu = [this, scene]() {
					if (!scene)
						return;

					m_manager->InvokeCurrentSceneDefaultContextMenu();
				};

				handled = DeserializeAndInvokeAction(
					value, defaultAction,
					defaultContextMenu);
			}

			obs_source_release(scene);

			return handled;
		}

		if (e->type() == QEvent::ContextMenu) {
			obs_source_t *scene = obs_frontend_get_current_scene();

			if (!scene)
				return false;

			CefRefPtr<CefValue> value =
				m_manager->GetSceneContextMenu(scene);

			bool handled = false;

			if (value->GetType() == VTYPE_LIST &&
			    value->GetList()->GetSize()) {
				auto defaultAction = [this, scene]() {
					if (!scene)
						return;

					m_manager
						->InvokeCurrentSceneDefaultAction();
				};

				auto defaultContextMenu = [this, scene]() {
					if (!scene)
						return;

					m_manager
						->InvokeCurrentSceneDefaultContextMenu();
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

			obs_source_release(scene);

			return handled;
		}

		return false;
	}
};

/* ================================================================= */
/* ================================================================= */
/* ================================================================= */

StreamElementsScenesListWidgetManager::StreamElementsScenesListWidgetManager(
	QMainWindow *mainWindow)
	: m_mainWindow(mainWindow)
{
	QDockWidget *scenesDock =
		(QDockWidget *)m_mainWindow->findChild<QDockWidget *>(
			"scenesDock");

	if (!scenesDock)
		return;

	m_nativeWidget =
		(QListWidget *)scenesDock->findChild<QListWidget *>("scenes");

	if (!m_nativeWidget)
		return;

	m_prevViewMode = m_nativeWidget->viewMode();

	m_eventFilter = new ScenesLocalEventFilter(this);

	QApplication::instance()->installEventFilter(m_eventFilter);

	m_scenesToolBar =
		(QToolBar *)scenesDock->findChild<QToolBar *>("scenesToolbar");

	if (!m_scenesToolBar)
		return;

	m_scenesToolBarActions->SetNull();

	//QtPostTask([this]() {
		auto model = m_nativeWidget->model();

		/* Subscribe to signals */

		QObject::connect(model, &QAbstractItemModel::modelReset, this,
				 &StreamElementsScenesListWidgetManager::
					 HandleScenesModelReset);

		QObject::connect(model, &QAbstractItemModel::rowsInserted, this,
				 &StreamElementsScenesListWidgetManager::
					 HandleScenesModelItemInsertedRemoved);

		QObject::connect(model, &QAbstractItemModel::rowsRemoved, this,
				 &StreamElementsScenesListWidgetManager::
					 HandleScenesModelItemInsertedRemoved);

		QObject::connect(model, &QAbstractItemModel::rowsMoved, this,
				 &StreamElementsScenesListWidgetManager::
					 HandleScenesModelItemMoved);

		signal_handler_connect(
			obs_get_signal_handler(), "source_rename",
			StreamElementsScenesListWidgetManager::HandleSceneRename,
			this);

		QObject::connect(m_nativeWidget,
				 &QListWidget::itemDoubleClicked, this,
				 &StreamElementsScenesListWidgetManager::
					 HandleScenesItemDoubleClicked);

		ScheduleUpdateWidgets();
		UpdateScenesToolbar();

		m_enableSignals = true;
	//});
}

void StreamElementsScenesListWidgetManager::CheckViewMode()
{
	QListWidget::ViewMode mode = m_nativeWidget->viewMode();

	if (mode != m_prevViewMode) {
		m_prevViewMode = mode;

		ScheduleUpdateWidgets();
	}
}

StreamElementsScenesListWidgetManager::
		~StreamElementsScenesListWidgetManager()
{
	m_enableSignals = false;

	if (!m_nativeWidget)
		return;

	//if (!m_nativeWidget->viewport())
	//	return;

	QApplication::instance()->removeEventFilter(m_eventFilter);
	//m_nativeWidget->viewport()->removeEventFilter(m_eventFilter);
	//m_nativeWidget->removeEventFilter(m_eventFilter);

	m_eventFilter->deleteLater();

	auto model = m_nativeWidget->model();

	QObject::disconnect(m_nativeWidget, &QListWidget::itemDoubleClicked,
			    this,
			    &StreamElementsScenesListWidgetManager::
				    HandleScenesItemDoubleClicked);

	signal_handler_disconnect(
		obs_get_signal_handler(), "source_rename",
		StreamElementsScenesListWidgetManager::HandleSceneRename, this);

	QObject::disconnect(
		model, &QAbstractItemModel::modelReset, this,
		&StreamElementsScenesListWidgetManager::HandleScenesModelReset);

	QObject::disconnect(model, &QAbstractItemModel::rowsInserted, this,
			    &StreamElementsScenesListWidgetManager::
				    HandleScenesModelItemInsertedRemoved);

	QObject::disconnect(model, &QAbstractItemModel::rowsRemoved, this,
			    &StreamElementsScenesListWidgetManager::
				    HandleScenesModelItemInsertedRemoved);

	QObject::disconnect(model, &QAbstractItemModel::rowsMoved, this,
			    &StreamElementsScenesListWidgetManager::
				    HandleScenesModelItemMoved);

	//m_nativeWidget->setItemDelegate(m_prevEditDelegate);
	//m_editDelegate->deleteLater();
}

void StreamElementsScenesListWidgetManager::HandleSceneRename(
	void *data, calldata_t *params)
{
	StreamElementsScenesListWidgetManager *self =
		(StreamElementsScenesListWidgetManager *)data;

	self->ScheduleUpdateWidgets();
}

void StreamElementsScenesListWidgetManager::HandleScenesModelReset()
{
	if (!m_enableSignals)
		return;

	ScheduleUpdateWidgets();
}

void StreamElementsScenesListWidgetManager::HandleScenesModelItemInsertedRemoved(
	const QModelIndex &, int, int)
{
	if (!m_enableSignals)
		return;

	ScheduleUpdateWidgets();
}

void StreamElementsScenesListWidgetManager::HandleScenesModelItemMoved(
	const QModelIndex &, int, int, const QModelIndex &, int)
{
	if (!m_enableSignals)
		return;

	ScheduleUpdateWidgets();
}

CefRefPtr<CefValue>
StreamElementsScenesListWidgetManager::GetScenePropertyValue(
	obs_source_t *scene, const char *key)
{
	CefRefPtr<CefValue> result = CefValue::Create();
	result->SetNull();

	obs_data_t *private_data = obs_source_get_private_settings(scene);

	const char *json = obs_data_get_string(private_data, key);

	if (json) {
		result = CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);
	}

	obs_data_release(private_data);

	if (!result.get()) {
		result = CefValue::Create();

		result->SetNull();
	}

	return result;
}

void StreamElementsScenesListWidgetManager::SetScenePropertyValue(
	obs_source_t *scene, const char *key, CefRefPtr<CefValue> value,
	bool triggerUpdate)
{
	obs_data_t *private_data = obs_source_get_private_settings(scene);

	std::string json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();

	obs_data_set_string(private_data, key, json.c_str());

	obs_data_release(private_data);

	if (triggerUpdate) {
		ScheduleUpdateWidgets();
	}
}

CefRefPtr<CefValue>
StreamElementsScenesListWidgetManager::GetSceneAuxiliaryData(obs_source_t *scene)
{
	return GetScenePropertyValue(scene, ITEM_PRIVATE_DATA_KEY_APPDATA);
}

void StreamElementsScenesListWidgetManager::SetSceneAuxiliaryData(
	obs_source_t *scene, CefRefPtr<CefValue> data)
{
	SetScenePropertyValue(scene, ITEM_PRIVATE_DATA_KEY_APPDATA, data,
			      false);
}

CefRefPtr<CefValue>
StreamElementsScenesListWidgetManager::GetSceneIcon(obs_source_t *scene)
{
	CefRefPtr<CefValue> iconValue =
		GetScenePropertyValue(scene, ITEM_PRIVATE_DATA_KEY_UI_ICON);

	if (iconValue->GetType() != VTYPE_DICTIONARY) {
		iconValue->SetNull();
	}

	return iconValue;
}

void StreamElementsScenesListWidgetManager::SetSceneIcon(
	obs_source_t *scene, CefRefPtr<CefValue> icon)
{
	auto data = SourceDataManager::instance()->data(scene);
	if (data) {
		data->DeserializeIcon(icon);
	}

	SetScenePropertyValue(scene, ITEM_PRIVATE_DATA_KEY_UI_ICON, icon);
}

CefRefPtr<CefValue>
StreamElementsScenesListWidgetManager::GetSceneDefaultAction(obs_source_t *scene)
{
	CefRefPtr<CefValue> data = GetScenePropertyValue(
		scene, ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION);

	if (data->GetType() != VTYPE_DICTIONARY) {
		data->SetNull();
	}

	return data;
}

void StreamElementsScenesListWidgetManager::SetSceneDefaultAction(
	obs_source_t *scene, CefRefPtr<CefValue> defaultAction)
{
	auto data = SourceDataManager::instance()->data(scene);
	if (data) {
		data->DeserializeDefaultAction(defaultAction);
	}

	SetScenePropertyValue(scene, ITEM_PRIVATE_DATA_KEY_UI_DEFAULT_ACTION,
			      defaultAction);
}

CefRefPtr<CefValue>
StreamElementsScenesListWidgetManager::GetSceneContextMenu(obs_source_t *scene)
{
	CefRefPtr<CefValue> data = GetScenePropertyValue(
		scene, ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU);

	if (data->GetType() != VTYPE_LIST) {
		data->SetNull();
	}

	return data;
}

void StreamElementsScenesListWidgetManager::SetSceneContextMenu(
	obs_source_t *scene, CefRefPtr<CefValue> contextMenu)
{
	auto data = SourceDataManager::instance()->data(scene);
	if (data) {
		data->DeserializeContextMenu(contextMenu);
	}

	SetScenePropertyValue(scene, ITEM_PRIVATE_DATA_KEY_UI_CONTEXT_MENU,
			      contextMenu);
}

void StreamElementsScenesListWidgetManager::ScheduleUpdateWidgets()
{
	if (!m_enableSignals)
		return;

	m_updateWidgetsDeferredExecutive.Signal([this]() { UpdateWidgets(); },
						250);
}

void StreamElementsScenesListWidgetManager::UpdateScenesToolbar()
{
	QWidget *widget = m_scenesToolBar->findChild<QWidget *>(
		"streamelements_scenes_toolbar_aux_actions");

	if (widget)
		m_scenesToolBar->layout()->removeWidget(widget);

	widget = new QWidget();
	widget->setObjectName(
		"streamelements_scenes_toolbar_aux_actions");

	QHBoxLayout *layout = new QHBoxLayout();
	layout->setContentsMargins(0, 0, 0, 0);
	widget->setLayout(layout);

	layout->addSpacerItem(new QSpacerItem(
		16, 16, QSizePolicy::Expanding, QSizePolicy::Minimum));

	if (!m_scenesToolBarActions.get() ||
		m_scenesToolBarActions->GetType() != VTYPE_LIST)
		return;

	CefRefPtr<CefListValue> list =
		m_scenesToolBarActions->GetList();

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

	m_scenesToolBar->addWidget(widget);

	QMainWindow *mainWindow = (QMainWindow *)obs_frontend_get_main_window();

	QDockWidget *scenesDock =
		(QDockWidget *)mainWindow->findChild<QDockWidget *>(
			"scenesDock");

	if (list->GetSize()) {
		scenesDock->setMinimumWidth(
			m_scenesToolBar->sizeHint().width());
	} else {
		scenesDock->setMinimumWidth(0);
	}
}

void StreamElementsScenesListWidgetManager::UpdateWidgets()
{
	struct obs_frontend_source_list sources = {};

	obs_frontend_get_scenes(&sources);

	obs_source_t *current_scene = obs_frontend_get_current_scene();

	if (!current_scene)
		return;

	bool isSignedIn =
		!StreamElementsConfig::GetInstance()->IsOnBoardingMode();

	SourceDataManager::instance()->removeDataNotInList(sources);

	for (int rowIndex = 0; rowIndex < m_nativeWidget->count() &&
				rowIndex < sources.sources.num;
		++rowIndex) {
		if (!isSignedIn) {
			m_nativeWidget->item(rowIndex)->setIcon(
				QIcon());

			continue;
		}

		obs_source_t *scene = sources.sources.array[rowIndex];

		ItemData *data =
			SourceDataManager::instance()->data(scene);

		if (!data) {
			data = new ItemData(
				[this]() { ScheduleUpdateWidgets(); });

			SourceDataManager::instance()->setData(scene,
								data);

			data->DeserializeIcon(GetSceneIcon(scene));

			data->DeserializeDefaultAction(
				GetSceneDefaultAction(scene));

			data->DeserializeContextMenu(
				GetSceneContextMenu(scene));
		}

		if (m_nativeWidget->viewMode() == QListView::IconMode) {
			/* Grid Mode: icons are not supported */
			m_nativeWidget->item(rowIndex)->setIcon(QIcon());
		} else {
			m_nativeWidget->item(rowIndex)->setIcon(data->icon());
		}
	}

	obs_source_release(current_scene);

	obs_frontend_source_list_free(
		(obs_frontend_source_list *)&sources);
}

void StreamElementsScenesListWidgetManager::HandleScenesItemDoubleClicked(
	QListWidgetItem *item)
{
}

bool StreamElementsScenesListWidgetManager::InvokeCurrentSceneDefaultAction()
{
	if (!m_nativeWidget)
		return false;

	QWidget *widget = m_nativeWidget->viewport();

	if (!widget)
		return false;

	QVariant value =
		widget->property(WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT);
	if (!value.isValid()) {
		value.setValue<int>(1);
	} else {
		value.setValue<int>(value.value<int>() + 1);
	}

	widget->setProperty(WIDGET_PROP_DEFAULT_ACTION_BYPASS_COUNT, value);

	QApplication::sendEvent(
		widget,
		new QMouseEvent(QMouseEvent::MouseButtonDblClick, QPoint(0, 0),
				Qt::LeftButton, Qt::NoButton, Qt::NoModifier));

	return true;
}

bool StreamElementsScenesListWidgetManager::InvokeCurrentSceneDefaultContextMenu()
{
	if (!m_nativeWidget)
		return false;

	const QRect rect =
		m_nativeWidget->visualRect(m_nativeWidget->currentIndex());

	QtPostTask([=]() {
		m_nativeWidget->customContextMenuRequested(
			QPoint(rect.left() + (rect.width() / 2),
			       rect.top() + (rect.height() / 2)));
	});

	return true;
}
