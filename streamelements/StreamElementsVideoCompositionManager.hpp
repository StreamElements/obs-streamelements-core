#pragma once

#include "StreamElementsVideoComposition.hpp"

class StreamElementsVideoCompositionManager {
private:
	std::recursive_mutex m_mutex;
	std::map<std::string, std::shared_ptr<StreamElementsVideoCompositionBase>> m_videoCompositionsMap;
	std::shared_ptr<StreamElementsVideoCompositionBase> m_nativeVideoComposition;

public:
	StreamElementsVideoCompositionManager();
	~StreamElementsVideoCompositionManager();

public:
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);
	void SerializeAllCompositions(CefRefPtr<CefValue> &output);
	void RemoveCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

	void SerializeAvailableEncoderClasses(obs_encoder_type type,
					CefRefPtr<CefValue> &output);

public:
	std::shared_ptr<StreamElementsVideoCompositionBase> GetObsNativeVideoComposition()
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		return m_nativeVideoComposition;
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionById(std::string id)
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		if (!m_videoCompositionsMap.count(id))
			return nullptr;

		return m_videoCompositionsMap[id];
	}

	std::shared_ptr<StreamElementsVideoCompositionBase>
	GetVideoCompositionByScene(obs_scene_t *lookupScene)
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

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
};
