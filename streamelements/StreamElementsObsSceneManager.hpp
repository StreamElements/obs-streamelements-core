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

class SESignalHandlerData;

class StreamElementsObsSceneManager {
private:
	SESignalHandlerData *m_signalHandlerData = nullptr;

public:
	StreamElementsObsSceneManager(QMainWindow *parent);
	virtual ~StreamElementsObsSceneManager();

private:
	void CleanObsSceneSignals();

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
				    CefRefPtr<CefValue> &output,
				    bool serializeProperties);

	void RemoveObsSceneItemsByIds(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output);

	void SetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output);

	void GetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
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

	/* Viewport Coords */

	void SerializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
						CefRefPtr<CefValue> &output);
	void DeserializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

	void SerializeSceneItemViewportBoundingRectangle(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void DeserializeSceneItemViewportPosition(CefRefPtr<CefValue> input,
						  CefRefPtr<CefValue> &output);

protected:
	void DeserializeAuxiliaryObsSceneItemProperties(
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		obs_sceneitem_t *sceneitem, CefRefPtr<CefDictionaryValue> d);

protected:
	QMainWindow *mainWindow() { return m_parent; }

	void ObsAddSourceInternal(
		obs_source_t *parentScene, obs_sceneitem_t *parentGroup,
		const char *sourceId, const char *sourceName,
		obs_data_t *sourceSettings, obs_data_t *sourceHotkeyData,
		bool preferExistingSource, const char *existingSourceId,
		obs_source_t **output_source,
		obs_sceneitem_t **output_sceneitem, bool *isExistingSource);

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

class SESignalHandlerData {
public:
	SESignalHandlerData(
		StreamElementsObsSceneManager *obsSceneManager,
		StreamElementsVideoCompositionBase *videoCompositionBase)
		: m_obsSceneManager(obsSceneManager),
		  m_videoCompositionBase(videoCompositionBase)
	{
		AddRef();

		m_wait_promise.set_value(); // release by default
	}

	~SESignalHandlerData()
	{
		Clear();
	}

private:
	SESignalHandlerData(
		SESignalHandlerData *parent,
		StreamElementsObsSceneManager *obsSceneManager,
		StreamElementsVideoCompositionBase *videoCompositionBase,
		obs_scene_t *scene)
		: m_parent(parent),
		  m_obsSceneManager(obsSceneManager),
		  m_videoCompositionBase(videoCompositionBase),
		  m_scene(SETRACE_ADDREF(obs_scene_get_ref(scene)))
	{
	}

	void Clear()
	{
		if (!m_parent) {
			Wait();
		}

		// std::unique_lock lock(m_scenes_mutex);

		m_videoCompositionBase = nullptr;
		m_obsSceneManager = nullptr;

		for (auto kv : m_scenes) {
			delete kv.second;
		}

		m_scenes.clear();
		m_scenes_refcount.clear();

		if (m_scene) {
			obs_scene_release(SETRACE_DECREF(m_scene));
		}
	}

public:
	void AddRef() {
		if (m_parent) {
			m_parent->AddRef();
			
			return;
		} else {
			std::unique_lock lock(m_refcount_mutex);

			++m_refcount;
		}
	}

	void Release() {
		bool shouldDelete = false;
		
		if (m_parent) {
			m_parent->Release();
			
			return;
		} else {
			std::unique_lock lock(m_refcount_mutex);

			--m_refcount;
			
			if (m_refcount == 0) {
				shouldDelete = true;
			}
		}
		
		if (shouldDelete) {
			blog(LOG_INFO,
			     "[obs-streamelements-core]: released SESignalHandlerData for video composition '%s'", m_videoCompositionBase->GetId().c_str());
			
			delete this;
		}
	}

	void GetScenes(std::vector<obs_scene_t *> &scenes)
	{
		SESignalHandlerData *self = this;

		while (self->m_parent)
			self = m_parent;

		std::shared_lock lock(self->m_scenes_mutex);
		for (auto kv : self->m_scenes)
			scenes.push_back(kv.first);
	}

	SESignalHandlerData *GetSceneRef(obs_scene_t *scene)
	{
		if (obs_scene_is_group(scene) && m_scene) {
			// If it's a group, use OUR scene (root scene) instead
			scene = m_scene;
		}

		if (m_parent) {
			return m_parent->GetSceneRef(scene);
		}

		std::shared_lock lock(m_scenes_mutex);

		if (!m_scenes.count(scene))
			return nullptr;

		return m_scenes[scene];
	}

	SESignalHandlerData* AddSceneRef(obs_scene_t* scene) {
		if (obs_scene_is_group(scene) && m_scene) {
			// If it's a group, use OUR scene (root scene) instead
			scene = m_scene;
		}

		if (m_parent) {
			return m_parent->AddSceneRef(scene);
		}

		std::unique_lock lock(m_scenes_mutex);

		if (!m_scenes.count(scene)) {
			m_scenes[scene] = new SESignalHandlerData(
				this,
				m_obsSceneManager, m_videoCompositionBase,
				scene);

			m_scenes_refcount[scene] = 1;

			AddRef();
		} else {
			++m_scenes_refcount[scene];
		}

		return m_scenes[scene];
	}

	void RemoveSceneRef(obs_scene_t* scene) {
		if (obs_scene_is_group(scene) && m_scene) {
			// If it's a group, use OUR scene (root scene) instead
			scene = m_scene;
		}

		if (m_parent) {
			m_parent->RemoveSceneRef(scene);

			return;
		}

		std::unique_lock lock(m_scenes_mutex);

		if (!m_scenes.count(scene))
			return;

		--m_scenes_refcount[scene];

		if (m_scenes_refcount[scene] == 0) {
			delete m_scenes[scene];

			m_scenes_refcount.erase(scene);
			m_scenes.erase(scene);

			Release();
		}
	}

	obs_scene_t* GetRootSceneRef()
	{
		std::shared_lock lock(m_scenes_mutex);

		auto item = this;

		obs_scene_t* scene = m_scene;

		while (scene && item->m_scene && item->m_parent) {
			item = item->m_parent;

			if (item->m_scene)
				scene = item->m_scene;
		}

		return SETRACE_ADDREF(obs_scene_get_ref(scene));
	}

	void Lock()
	{
		if (m_parent) {
			m_parent->Lock();

			return;
		}

		std::unique_lock lock(m_wait_mutex);

		++m_wait_count;

		if (m_wait_count == 1) {
			m_wait_promise = std::promise<void>();
			m_wait_future = m_wait_promise.get_future();
		}
	}

	void Unlock()
	{
		if (m_parent) {
			m_parent->Unlock();

			return;
		}

		std::unique_lock lock(m_wait_mutex);

		--m_wait_count;

		if (m_wait_count == 0) {
			m_wait_promise.set_value();
		}
	}

	void Wait()
	{
		if (m_parent)
		{
			m_parent->Wait();

			return;
		}

		std::shared_future<void> future;

		{
			std::shared_lock lock(m_wait_mutex);

			if (m_wait_count == 0)
				return;

			future = m_wait_future;
		}

		future.wait();
	}

public:
	// char m_header[7] = "header"; // TODO: Remvoe debug marker
	StreamElementsObsSceneManager *m_obsSceneManager = nullptr;
	StreamElementsVideoCompositionBase *m_videoCompositionBase = nullptr;
	obs_scene_t* m_scene = nullptr;
	// char m_footer[7] = "footer"; // TODO: Remvoe debug marker

private:
	std::shared_mutex m_wait_mutex;
	std::promise<void> m_wait_promise;
	std::shared_future<void> m_wait_future = m_wait_promise.get_future();
	uint32_t m_wait_count = 0;

private:
	std::shared_mutex m_scenes_mutex;
	std::map<obs_scene_t *, SESignalHandlerData *> m_scenes;
	std::map<obs_scene_t *, uint32_t> m_scenes_refcount;
	SESignalHandlerData *m_parent = nullptr;

	std::shared_mutex m_refcount_mutex;
	uint32_t m_refcount = 0;
};

void add_scene_signals(obs_source_t *scene, SESignalHandlerData *data);
void add_scene_signals(obs_scene_t *scene, SESignalHandlerData *data);
void add_source_signals(obs_source_t *source, SESignalHandlerData *data);
void remove_scene_signals(obs_source_t *scene, SESignalHandlerData *data);
void remove_scene_signals(obs_scene_t *scene, SESignalHandlerData *data);
void remove_source_signals(obs_source_t *source, SESignalHandlerData *data);
