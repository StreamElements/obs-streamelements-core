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
};
