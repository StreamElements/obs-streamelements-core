#pragma once

#include "StreamElementsVideoComposition.hpp"
#include <shared_mutex>

class StreamElementsVideoCompositionManager {
private:
	std::shared_mutex m_mutex;
	std::map<std::string, std::shared_ptr<StreamElementsVideoCompositionBase>> m_videoCompositionsMap;
	std::shared_ptr<StreamElementsVideoCompositionBase> m_nativeVideoComposition;

public:
	StreamElementsVideoCompositionManager();
	~StreamElementsVideoCompositionManager();

public:
	void
	DeserializeExistingCompositionProperties(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output);
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);
	void SerializeAllCompositions(CefRefPtr<CefValue> &output);
	void RemoveCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void SerializeAvailableEncoderClasses(obs_encoder_type type,
					CefRefPtr<CefValue> &output);

	void SerializeAvailableTransitionClasses(CefRefPtr<CefValue> &output);

	void Reset();

public:
	std::shared_ptr<StreamElementsVideoCompositionBase> GetObsNativeVideoComposition()
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return m_nativeVideoComposition;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionById(std::string id)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		if (!m_videoCompositionsMap.count(id))
			return nullptr;

		return m_videoCompositionsMap[id];
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionById(CefRefPtr<CefValue> input)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
			return GetObsNativeVideoComposition();

		auto d = input->GetDictionary();

		if (!d->HasKey("videoCompositionId") || d->GetType("videoCompositionId") != VTYPE_STRING)
			return GetObsNativeVideoComposition();

		std::string id = d->GetString("videoCompositionId");

		if (!m_videoCompositionsMap.count(id))
			return nullptr;

		return m_videoCompositionsMap[id];
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionByScene(obs_scene_t *lookupScene)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			std::vector<obs_scene_t *> scenes;
			kv.second->GetAllScenes(scenes);

			for (auto scene : scenes) {
				if (scene == lookupScene)
					return kv.second;
			}
		}

		return nullptr;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionBySceneId(std::string lookupId)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->GetSceneById(lookupId))
				return kv.second;
		}

		return nullptr;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionBySceneName(std::string lookupName)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->GetSceneByName(lookupName))
				return kv.second;
		}

		return nullptr;
	}

	obs_sceneitem_t *GetSceneItemById(std::string lookupSceneItemId, bool addRef = false)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			std::vector<obs_scene_t *> scenes;

			auto result =
				kv.second->GetSceneItemById(lookupSceneItemId, addRef);

			if (result)
				return result;
		}

		return nullptr;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionBySceneItemId(std::string lookupSceneItemId)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->GetSceneItemById(lookupSceneItemId,
							false))
				return kv.second;
		}

		return nullptr;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionBySceneItemId(std::string lookupSceneItemId, obs_scene_t** result_scene)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->GetSceneItemById(lookupSceneItemId,
							result_scene, false))
				return kv.second;
		}

		return nullptr;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionBySceneItemName(std::string lookupSceneItemName,
					   obs_scene_t **result_scene)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->GetSceneItemByName(lookupSceneItemName,
							  result_scene, false))
				return kv.second;
		}

		return nullptr;
	}
};
