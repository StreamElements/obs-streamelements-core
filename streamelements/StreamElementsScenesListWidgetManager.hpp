#pragma once

#include "StreamElementsDeferredExecutive.hpp"
#include "StreamElementsUtils.hpp"

#include <obs.h>

#include "cef-headers.hpp"

#include <QMainWindow>
#include <QListWidget>
#include <QListView>
#include <QToolBar>

#define SE_ENABLE_SCENE_ICONS 0
#define SE_ENABLE_SCENE_DEFAULT_ACTION 0
#define SE_ENABLE_SCENE_CONTEXT_MENU 0
#define SE_ENABLE_SCENES_UI_EXTENSIONS \
	(SE_ENABLE_SCENE_ICONS || SE_ENABLE_SCENE_DEFAULT_ACTION || \
	 SE_ENABLE_SCENE_CONTEXT_MENU)

class StreamElementsScenesListWidgetManager : public QObject {
public:
	StreamElementsScenesListWidgetManager(QMainWindow *mainWindow);
	virtual ~StreamElementsScenesListWidgetManager();

public:
	static CefRefPtr<CefValue> GetScenePropertyValue(obs_source_t *scene,
							 const char *key);

	void SetScenePropertyValue(obs_source_t *scene, const char *key,
				   CefRefPtr<CefValue> value,
				   bool triggerUpdate = true);

	static CefRefPtr<CefValue> GetSceneAuxiliaryData(obs_source_t *scene);

	void SetSceneAuxiliaryData(obs_source_t *scene,
				   CefRefPtr<CefValue> data);

	#if SE_ENABLE_SCENE_ICONS
	static CefRefPtr<CefValue> GetSceneIcon(obs_source_t *scene);

	void SetSceneIcon(obs_source_t *scene, CefRefPtr<CefValue> icon);
	#endif

	#if SE_ENABLE_SCENE_DEFAULT_ACTION
	static CefRefPtr<CefValue> GetSceneDefaultAction(obs_source_t *scene);

	void SetSceneDefaultAction(obs_source_t *scene,
				   CefRefPtr<CefValue> icon);
	#endif

	#if SE_ENABLE_SCENE_CONTEXT_MENU
	static CefRefPtr<CefValue> GetSceneContextMenu(obs_source_t *scene);

	void SetSceneContextMenu(obs_source_t *scene, CefRefPtr<CefValue> icon);
	#endif

	bool InvokeCurrentSceneDefaultAction();
	bool InvokeCurrentSceneDefaultContextMenu();

	bool DeserializeScenesAuxiliaryActions(CefRefPtr<CefValue> m_actions)
	{
		m_scenesToolBarActions = m_actions->Copy();

		UpdateScenesToolbar();

		return true;
	}

	void SerializeScenesAuxiliaryActions(CefRefPtr<CefValue> &output)
	{
		output = m_scenesToolBarActions->Copy();
	}

	void Update() {
		#if SE_ENABLE_SCENES_UI_EXTENSIONS
		ScheduleUpdateWidgets();
		#endif
	}

	QListWidget *GetScenesListWidget() { return m_nativeWidget; }

	void CheckViewMode();

private:
	void HandleScenesModelReset();
	void HandleScenesModelItemInsertedRemoved(const QModelIndex &, int,
						  int);
	void HandleScenesModelItemMoved(const QModelIndex &, int, int,
					const QModelIndex &, int);

	static void HandleSceneRename(void *data, calldata_t *params);

	void HandleScenesItemDoubleClicked(QListWidgetItem *item);

	#if SE_ENABLE_SCENES_UI_EXTENSIONS
	void ScheduleUpdateWidgets();
	void UpdateWidgets();
	#endif

	void UpdateScenesToolbar();

private:
	QMainWindow *m_mainWindow = nullptr;
	QListWidget *m_nativeWidget = nullptr;
	bool m_enableSignals = false;
	StreamElementsDeferredExecutive m_updateWidgetsDeferredExecutive;
	QAbstractItemDelegate *m_editDelegate;
	QAbstractItemDelegate *m_prevEditDelegate;
	CefRefPtr<CefValue> m_scenesToolBarActions = CefValue::Create();
	QToolBar *m_scenesToolBar = nullptr;
	QObject *m_eventFilter = nullptr;
	QListWidget::ViewMode m_prevViewMode = QListView::ListMode;
};
