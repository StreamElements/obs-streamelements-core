#pragma once

#include "SEDeferredExecutive.hpp"
#include "SEUtils.hpp"

#include <obs.h>

#include "cef-headers.hpp"

#include <QMainWindow>
#include <QPointer>
#include <QListWidget>
#include <QListView>
#include <QToolBar>

#define SE_ENABLE_SCENE_ICONS 0
#define SE_ENABLE_SCENE_DEFAULT_ACTION 0
#define SE_ENABLE_SCENE_CONTEXT_MENU 0
#define SE_ENABLE_SCENES_UI_EXTENSIONS \
	(SE_ENABLE_SCENE_ICONS || SE_ENABLE_SCENE_DEFAULT_ACTION || \
	 SE_ENABLE_SCENE_CONTEXT_MENU)

class SEAuxiliaryControlsBuilder;
class SEConfig;

class SEScenesListWidgetManager : public QObject {
public:
	SEScenesListWidgetManager(QMainWindow *mainWindow,
				  std::shared_ptr<SEAuxiliaryControlsBuilder>
					  auxiliaryControlsBuilder,
				  std::shared_ptr<SEConfig> config);
	virtual ~SEScenesListWidgetManager();

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
	QPointer<QMainWindow> m_mainWindow = nullptr;

	//
	// Builds the toolbar's auxiliary controls from the description a page
	// sends. Public below, because the compiled-out scene UI extensions
	// reach it from file-static helpers that hold the manager rather than
	// from members.
	//
	std::shared_ptr<SEAuxiliaryControlsBuilder> m_auxiliaryControls;
	std::shared_ptr<SEConfig> m_config;

public:
	std::shared_ptr<SEAuxiliaryControlsBuilder> GetAuxiliaryControls()
	{
		return m_auxiliaryControls;
	}

private:
	QPointer<QListWidget> m_nativeWidget = nullptr;
	bool m_enableSignals = false;
	SEDeferredExecutive m_updateWidgetsDeferredExecutive;
	QPointer<QAbstractItemDelegate> m_editDelegate;
	QPointer<QAbstractItemDelegate> m_prevEditDelegate;
	CefRefPtr<CefValue> m_scenesToolBarActions = CefValue::Create();
	QPointer<QToolBar> m_scenesToolBar = nullptr;
	QPointer<QObject> m_eventFilter = nullptr;
	QListWidget::ViewMode m_prevViewMode = QListView::ListMode;
};
