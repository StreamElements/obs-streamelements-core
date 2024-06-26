#pragma once

#include "StreamElementsComposition.hpp"

class StreamElementsCompositionManager {
private:
	std::recursive_mutex m_mutex;
	std::map<std::string, std::shared_ptr<StreamElementsCompositionBase>> m_map;
	std::shared_ptr<StreamElementsCompositionBase> m_nativeComposition;

public:
	StreamElementsCompositionManager();
	~StreamElementsCompositionManager();

public:
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);
	void SerializeAllCompositions(CefRefPtr<CefValue> &output);
	void RemoveCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output);

public:
	std::shared_ptr<StreamElementsCompositionBase> GetObsNativeComposition()
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		return m_nativeComposition;
	}

	std::shared_ptr<StreamElementsCompositionBase>
	GetCompositionById(std::string id)
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		if (!m_map.count(id))
			return nullptr;

		return m_map[id];
	}
};
