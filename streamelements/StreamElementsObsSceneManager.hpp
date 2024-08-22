#pragma once

#include "StreamElementsSceneItemsMonitor.hpp"
#include "StreamElementsScenesListWidgetManager.hpp"
#include "StreamElementsVideoComposition.hpp"

#include "cef-headers.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QApplication>

#include <mutex>

void add_scene_signals(obs_source_t *scene, void *data);
void add_scene_signals(obs_scene_t *scene, void *data);
void add_source_signals(obs_source_t *source, void *data);
void remove_scene_signals(obs_source_t *scene, void *data);
void remove_scene_signals(obs_scene_t *scene, void *data);
void remove_source_signals(obs_source_t *source, void *data);

class StreamElementsObsSceneManager {
public:
	StreamElementsObsSceneManager(QMainWindow *parent);
	virtual ~StreamElementsObsSceneManager();

public:
	void Update()
	{
		if (m_sceneItemsMonitor)
			m_sceneItemsMonitor->Update();

		if (m_scenesWidgetManager)
			m_scenesWidgetManager->Update();
	}

	void Reset()
	{
		auto out1 = CefValue::Create();
		auto out2 = CefValue::Create();

		DeserializeSceneItemsAuxiliaryActions(CefValue::Create(),
						      out1);

		DeserializeScenesAuxiliaryActions(CefValue::Create(),
						  out2);

		Update();
	}

	/* Sources */

	void SerializeInputSourceClasses(CefRefPtr<CefValue> &output);

	void SerializeSourceClassProperties(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output);

	/* Scene items */

	void DeserializeObsBrowserSource(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);

	void DeserializeObsGameCaptureSource(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output);

	void DeserializeObsVideoCaptureSource(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output);

	void DeserializeObsNativeSource(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output);

	void DeserializeObsSceneItemGroup(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output);

	void SerializeObsSceneItems(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);

	void RemoveObsSceneItemsByIds(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);

	void SetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void UngroupObsSceneItemsByGroupId(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void
	InvokeCurrentSceneItemDefaultActionById(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);

	void InvokeCurrentSceneItemDefaultContextMenuById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void DeserializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> input,
						   CefRefPtr<CefValue> &output);

	void SerializeSceneItemsAuxiliaryActions(CefRefPtr<CefValue> &output);

	/* Scenes */

	void SerializeObsScenes(CefRefPtr<CefValue> input,
				CefRefPtr<CefValue> &output);

	void SerializeObsCurrentScene(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);

	void DeserializeObsScene(CefRefPtr<CefValue> input,
				 CefRefPtr<CefValue> &output);

	void SetCurrentObsSceneById(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);

	void RemoveObsScenesByIds(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output);

	void SetObsScenePropertiesById(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output);

	void DeserializeScenesAuxiliaryActions(CefRefPtr<CefValue> input,
					       CefRefPtr<CefValue> &output);

	void SerializeScenesAuxiliaryActions(CefRefPtr<CefValue> &output);

	/* Scene collections */

	void SerializeObsSceneCollections(CefRefPtr<CefValue> &output);

	void SerializeObsCurrentSceneCollection(CefRefPtr<CefValue> &output);

	void DeserializeObsSceneCollection(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void
	DeserializeObsCurrentSceneCollectionById(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output);

	/* OBS Native Dialogs */

	void OpenSceneItemPropertiesById(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output);
	void OpenSceneItemFiltersById(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);
	void OpenSceneItemInteractionById(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output);
	void OpenSceneItemTransformEditorById(CefRefPtr<CefValue> input,
					      CefRefPtr<CefValue> &output);

protected:
	void DeserializeAuxiliaryObsSceneItemProperties(
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		obs_sceneitem_t *sceneitem, CefRefPtr<CefDictionaryValue> d);

protected:
	QMainWindow *mainWindow() { return m_parent; }

	void ObsAddSourceInternal(obs_source_t *parentScene,
				  obs_sceneitem_t *parentGroup,
				  const char *sourceId, const char *sourceName,
				  obs_data_t *sourceSettings,
				  obs_data_t *sourceHotkeyData,
				  bool preferExistingSource,
				  obs_source_t **output_source,
				  obs_sceneitem_t **output_sceneitem);

	void RefreshObsSceneItemsList();

	std::string ObsGetUniqueSourceName(std::string name);

	std::string ObsGetUniqueSceneCollectionName(std::string name);

	std::string ObsSetUniqueSourceName(obs_source_t *source,
					   std::string name);

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);

protected:
	std::recursive_mutex m_mutex;

private:
	QMainWindow *m_parent;
	StreamElementsSceneItemsMonitor *m_sceneItemsMonitor = nullptr;
	StreamElementsScenesListWidgetManager *m_scenesWidgetManager = nullptr;
};
