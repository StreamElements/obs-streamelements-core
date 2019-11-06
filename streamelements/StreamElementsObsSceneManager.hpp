#pragma once

#include "cef-headers.hpp"

#include <obs.h>
#include <obs-frontend-api.h>

#include <QMainWindow>
#include <QApplication>

#include <mutex>

class StreamElementsObsSceneManager
{
public:
	StreamElementsObsSceneManager(QMainWindow* parent);
	virtual ~StreamElementsObsSceneManager();

public:
	/* Sources */

	void SerializeInputSourceClasses(CefRefPtr<CefValue> &output);

	void SerializeSourceClassProperties(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output);

	/* Scene items */

	void DeserializeObsBrowserSource(
		CefRefPtr<CefValue>& input,
		CefRefPtr<CefValue>& output);

	void DeserializeObsGameCaptureSource(CefRefPtr<CefValue> &input,
					     CefRefPtr<CefValue> &output);

	void DeserializeObsVideoCaptureSource(CefRefPtr<CefValue> &input,
					      CefRefPtr<CefValue> &output);

	void DeserializeObsNativeSource(CefRefPtr<CefValue> &input,
					CefRefPtr<CefValue> &output);

	void DeserializeObsSceneItemGroup(CefRefPtr<CefValue> &input,
					  CefRefPtr<CefValue> &output);

	void SerializeObsCurrentSceneItems(
		CefRefPtr<CefValue>& output);

	void RemoveObsCurrentSceneItemsByIds(CefRefPtr<CefValue> input,
					     CefRefPtr<CefValue> &output);

	void SetObsCurrentSceneItemPropertiesById(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

	void UngroupObsCurrentSceneItemsByGroupId(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

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

protected:
	QMainWindow* mainWindow() { return m_parent; }

	void ObsAddSourceInternal(
		obs_source_t* parentScene,
		obs_sceneitem_t* parentGroup,
		const char* sourceId,
		const char* sourceName,
		obs_data_t* sourceSettings,
		obs_data_t* sourceHotkeyData,
		bool preferExistingSource,
		obs_source_t** output_source,
		obs_sceneitem_t** output_sceneitem);

	void RefreshObsSceneItemsList();

	std::string ObsGetUniqueSourceName(std::string name);

	std::string ObsGetUniqueSceneName(std::string name);

protected:
	std::recursive_mutex m_mutex;

private:
	QMainWindow* m_parent;
};
