#pragma once

#include "SEVideoComposition.hpp"
#include <shared_mutex>
#include <string>
#include <map>
#include "cef-headers.hpp"

class SEVideoCompositionManager {
private:
	std::shared_mutex m_mutex;
	std::map<std::string, std::shared_ptr<SEVideoCompositionBase>> m_videoCompositionsMap;
	std::shared_ptr<SEVideoCompositionBase> m_nativeVideoComposition;

	std::map<std::string, CefRefPtr<CefDictionaryValue>>
		m_availableEncoderClassesCache;

public:
	SEVideoCompositionManager();
	~SEVideoCompositionManager();

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

	void SerializeAvailableEncoderClassPropertiesForSettings(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output,
		obs_encoder_type encoder_type);

	void SerializeAvailableTransitionClasses(CefRefPtr<CefValue> &output);

	void Reset();

public:
	std::shared_ptr<SEVideoCompositionBase> GetObsNativeVideoComposition()
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return m_nativeVideoComposition;
	}

	std::shared_ptr<SEVideoCompositionBase>
	GetVideoCompositionById(std::string id)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		if (!m_videoCompositionsMap.count(id))
			return nullptr;

		return m_videoCompositionsMap[id];
	}

	std::shared_ptr<SEVideoCompositionBase>
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

	std::shared_ptr<SEVideoCompositionBase>
	GetVideoCompositionByScene(obs_scene_t *lookupScene)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->HasScene(lookupScene))
				return kv.second;
		}

		return nullptr;
	}

	std::shared_ptr<SEVideoCompositionBase>
	GetVideoCompositionBySceneId(std::string lookupId)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->HasSceneId(lookupId))
				return kv.second;
		}

		return nullptr;
	}

	std::shared_ptr<SEVideoCompositionBase>
	GetVideoCompositionBySceneName(std::string lookupName)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		for (auto kv : m_videoCompositionsMap) {
			if (kv.second->HasSceneName(lookupName))
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

	std::shared_ptr<SEVideoCompositionBase>
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

	std::shared_ptr<SEVideoCompositionBase>
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

	std::shared_ptr<SEVideoCompositionBase>
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
