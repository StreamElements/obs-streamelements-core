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

#include <obs.h>

#include "cef-headers.hpp"

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

	static CefRefPtr<CefListValue>
	GetSceneItemActions(obs_sceneitem_t *scene_item);

	void SetSceneItemActions(obs_sceneitem_t *scene_item,
				 CefRefPtr<CefListValue> m_actions);

	static CefRefPtr<CefValue>
	GetSceneItemIcon(obs_sceneitem_t *scene_item);

	void SetSceneItemIcon(obs_sceneitem_t *scene_item,
			      CefRefPtr<CefValue> m_icon);

	static CefRefPtr<CefValue>
	GetSceneItemDefaultAction(obs_sceneitem_t *scene_item);

	void SetSceneItemDefaultAction(obs_sceneitem_t *scene_item,
				       CefRefPtr<CefValue> action);

	static CefRefPtr<CefValue>
	GetSceneItemContextMenu(obs_sceneitem_t *scene_item);

	void SetSceneItemContextMenu(obs_sceneitem_t *scene_item,
				     CefRefPtr<CefValue> menu);

	CefRefPtr<CefValue> static GetSceneItemAuxiliaryData(
		obs_sceneitem_t *scene_item);

	void SetSceneItemAuxiliaryData(obs_sceneitem_t *scene_item,
				       CefRefPtr<CefValue> data);

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

	void Update() { ScheduleUpdateSceneItemsWidgets(); }

	QWidget* GetWidgetAtLocalPosition(const QPointF &p) {
		QModelIndex index = m_sceneItemsListView->indexAt(QPoint(p.x(), p.y()));

		if (!index.isValid())
			return nullptr;

		return m_sceneItemsListView->indexWidget(index);
	}

	QListView* GetSceneItemsListView() { return m_sceneItemsListView; }

private:
	void DisconnectSignalHandlers();

	void ScheduleUpdateSceneItemsWidgets();

	void UpdateSceneItemsWidgets();

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
	//QListView *m_scenesListView = nullptr;
	//QListWidget *m_scenesListWidget = nullptr;
	QToolBar *m_sceneItemsToolBar = nullptr;
	QAbstractItemModel *m_sceneItemsModel = nullptr;
	//QAbstractItemModel *m_scenesModel = nullptr;
	bool m_enableSignals = false;
	StreamElementsDeferredExecutive
		m_updateSceneItemsWidgetsThrottledExecutive;
	QObject *m_eventFilter = nullptr;
	CefRefPtr<CefValue> m_sceneItemsToolBarActions = CefValue::Create();
};
