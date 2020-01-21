#pragma once

#include "StreamElementsSceneItemsMonitor.hpp"
#include "StreamElementsScenesListWidgetManager.hpp"

#include "cef-headers.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QApplication>

#include <mutex>

class StreamElementsObsSceneManager {
public:
	StreamElementsObsSceneManager(QMainWindow *parent);
	virtual ~StreamElementsObsSceneManager();

public:
	void Reset()
	{
		DeserializeSceneItemsAuxiliaryActions(CefValue::Create(),
						      CefValue::Create());

		DeserializeScenesAuxiliaryActions(CefValue::Create(),
						  CefValue::Create());

		if (m_sceneItemsMonitor)
			m_sceneItemsMonitor->Update();

		if (m_scenesWidgetManager)
			m_scenesWidgetManager->Update();
	}

	/* Sources */

	void SerializeInputSourceClasses(CefRefPtr<CefValue> &output);

	void SerializeSourceClassProperties(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output);

	/* Scene items */

	void DeserializeObsBrowserSource(CefRefPtr<CefValue> &input,
					 CefRefPtr<CefValue> &output);

	void DeserializeObsGameCaptureSource(CefRefPtr<CefValue> &input,
					     CefRefPtr<CefValue> &output);

	void DeserializeObsVideoCaptureSource(CefRefPtr<CefValue> &input,
					      CefRefPtr<CefValue> &output);

	void DeserializeObsNativeSource(CefRefPtr<CefValue> &input,
					CefRefPtr<CefValue> &output);

	void DeserializeObsSceneItemGroup(CefRefPtr<CefValue> &input,
					  CefRefPtr<CefValue> &output);

	void SerializeObsCurrentSceneItems(CefRefPtr<CefValue> &output);

	void RemoveObsCurrentSceneItemsByIds(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output);

	void SetObsCurrentSceneItemPropertiesById(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

	void UngroupObsCurrentSceneItemsByGroupId(CefRefPtr<CefValue> input,
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

	void SerializeObsScenes(CefRefPtr<CefValue> &output);

	void SerializeObsCurrentScene(CefRefPtr<CefValue> &output);

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

	std::string ObsGetUniqueSceneName(std::string name);

	std::string ObsGetUniqueSceneCollectionName(std::string name);

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
