#pragma once

#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsDeferredExecutive.hpp"
#include "StreamElementsUtils.hpp"

#include <QObject>
#include <QMainWindow>
#include <QListView>
#include <QAbstractItemModel>
#include <QToolBar>
#include <QListWidget>
#include <QToolButton>

#include <obs.h>

#include "cef-headers.hpp"

#define SE_ENABLE_SCENEITEM_ACTIONS 0
#define SE_ENABLE_SCENEITEM_ICONS 0
#define SE_ENABLE_SCENEITEM_DEFAULT_ACTION 0
#define SE_ENABLE_SCENEITEM_CONTEXT_MENU 0
#define SE_ENABLE_SCENEITEM_RENDERING_SETTINGS 0
#define SE_ENABLE_SCENEITEM_UI_EXTENSIONS                            \
	(SE_ENABLE_SCENEITEM_ACTIONS || SE_ENABLE_SCENEITEM_ICONS || \
	 SE_ENABLE_SCENEITEM_DEFAULT_ACTION ||                       \
	 SE_ENABLE_SCENEITEM_CONTEXT_MENU ||                         \
	 SE_ENABLE_SCENEITEM_RENDERING_SETTINGS)

class StreamElementsSceneItemsMonitor : public QObject,
					public StreamElementsObsAppMonitor {
public:
	StreamElementsSceneItemsMonitor(QMainWindow *mainWindow);
	virtual ~StreamElementsSceneItemsMonitor();

	/* ==================================================== */

	static CefRefPtr<CefValue>
	GetSceneItemPropertyValue(obs_sceneitem_t *scene_item, const char *key);

	void SetSceneItemPropertyValue(obs_sceneitem_t *scene_item,
				       const char *key,
				       CefRefPtr<CefValue> value,
				       bool triggerUpdate = true);

	#if SE_ENABLE_SCENEITEM_ACTIONS
	static CefRefPtr<CefListValue>
	GetSceneItemActions(obs_sceneitem_t *scene_item);

	void SetSceneItemActions(obs_sceneitem_t *scene_item,
				 CefRefPtr<CefListValue> m_actions);
	#endif

	#if SE_ENABLE_SCENEITEM_ICONS
	static CefRefPtr<CefValue>
	GetSceneItemIcon(obs_sceneitem_t *scene_item);

	void SetSceneItemIcon(obs_sceneitem_t *scene_item,
			      CefRefPtr<CefValue> m_icon);
	#endif

	#if SE_ENABLE_SCENEITEM_DEFAULT_ACTION
	static CefRefPtr<CefValue>
	GetSceneItemDefaultAction(obs_sceneitem_t *scene_item);

	void SetSceneItemDefaultAction(obs_sceneitem_t *scene_item,
				       CefRefPtr<CefValue> action);
	#endif

	#if SE_ENABLE_SCENEITEM_CONTEXT_MENU
	static CefRefPtr<CefValue>
	GetSceneItemContextMenu(obs_sceneitem_t *scene_item);

	void SetSceneItemContextMenu(obs_sceneitem_t *scene_item,
				     CefRefPtr<CefValue> menu);
	#endif

	CefRefPtr<CefValue> static GetSceneItemAuxiliaryData(
		obs_sceneitem_t *scene_item);

	void SetSceneItemAuxiliaryData(obs_sceneitem_t *scene_item,
				       CefRefPtr<CefValue> data);

	#if SE_ENABLE_SCENEITEM_RENDERING_SETTINGS
	CefRefPtr<CefValue> static GetSceneItemUISettings(
		obs_sceneitem_t *scene_item);
	void SetSceneItemUISettings(obs_sceneitem_t *scene_item,
				    CefRefPtr<CefValue> data);
	#endif

	bool static GetSceneItemUISettingsEnabled(
		obs_sceneitem_t *scene_item);

	bool static GetSceneItemUISettingsMultiselectContextMenuEnabled(
		obs_sceneitem_t *scene_item);

	bool InvokeCurrentSceneItemDefaultAction(obs_sceneitem_t *scene_item);

	bool
	InvokeCurrentSceneItemDefaultContextMenu(obs_sceneitem_t *scene_item);

	bool
	DeserializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> m_actions)
	{
		m_sceneItemsToolBarActions = m_actions->Copy();

		UpdateSceneItemsToolbar();

		return true;
	}

	void SerializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> &output)
	{
		output = m_sceneItemsToolBarActions->Copy();
	}

	void Update() {
		#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
		ScheduleUpdateSceneItemsWidgets();
		#endif
	}

	QWidget* GetWidgetAtLocalPosition(const QPointF &p) {
		QModelIndex index = m_sceneItemsListView->indexAt(QPoint(p.x(), p.y()));

		if (!index.isValid())
			return nullptr;

		return m_sceneItemsListView->indexWidget(index);
	}

	QListView* GetSceneItemsListView() { return m_sceneItemsListView; }

private:
	void DisconnectSignalHandlers();

	#if SE_ENABLE_SCENEITEM_UI_EXTENSIONS
	void ScheduleUpdateSceneItemsWidgets();

	void UpdateSceneItemsWidgets();
	#endif

	void UpdateSceneItemsToolbar();

private slots:
	void HandleSceneItemsModelReset();
	void HandleSceneItemsModelItemInsertedRemoved(const QModelIndex &, int,
						      int);
	void HandleSceneItemsModelItemMoved(const QModelIndex &, int, int,
					    const QModelIndex &, int);

protected:
	virtual void OnObsExit() override;

private:
	QMainWindow *m_mainWindow;
	QListView *m_sceneItemsListView = nullptr;
	QToolBar *m_sceneItemsToolBar = nullptr;
	QAbstractItemModel *m_sceneItemsModel = nullptr;
	bool m_enableSignals = false;
	StreamElementsDeferredExecutive
		m_updateSceneItemsWidgetsThrottledExecutive;
	QObject *m_eventFilter = nullptr;
	CefRefPtr<CefValue> m_sceneItemsToolBarActions = CefValue::Create();
	QToolButton *m_nativeActionSourcePropertiesButton = nullptr;
};
