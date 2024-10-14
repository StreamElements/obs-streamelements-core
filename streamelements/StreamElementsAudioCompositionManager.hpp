#pragma once

#include "StreamElementsAudioComposition.hpp"

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include "StreamElementsUtils.hpp"

#include <shared_mutex>

class StreamElementsAudioCompositionManager {
private:
	std::shared_mutex m_mutex;
	std::map<std::string,
		 std::shared_ptr<StreamElementsAudioCompositionBase>>
		m_audioCompositionsMap;
	std::shared_ptr<StreamElementsAudioCompositionBase>
		m_nativeAudioComposition;

public:
	StreamElementsAudioCompositionManager();
	~StreamElementsAudioCompositionManager();

public:
	void
	DeserializeExistingCompositionProperties(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output);
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);
	void SerializeAllCompositions(CefRefPtr<CefValue> &output);
	void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output);

	void SerializeAvailableEncoderClasses(obs_encoder_type type,
					      CefRefPtr<CefValue> &output);

	void SerializeAvailableTransitionClasses(CefRefPtr<CefValue> &output);

	void Reset();

public:
	std::shared_ptr<StreamElementsAudioCompositionBase>
	GetObsNativeAudioComposition()
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return m_nativeAudioComposition;
	}

	std::shared_ptr<StreamElementsAudioCompositionBase>
	GetAudioCompositionById(std::string id)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		if (!m_audioCompositionsMap.count(id))
			return nullptr;

		return m_audioCompositionsMap[id];
	}

	std::shared_ptr<StreamElementsAudioCompositionBase>
	GetAudioCompositionById(CefRefPtr<CefValue> input)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
			return GetObsNativeAudioComposition();

		auto d = input->GetDictionary();

		if (!d->HasKey("audioCompositionId") ||
		    d->GetType("audioCompositionId") != VTYPE_STRING)
			return GetObsNativeAudioComposition();

		std::string id = d->GetString("audioCompositionId");

		if (!m_audioCompositionsMap.count(id))
			return nullptr;

		return m_audioCompositionsMap[id];
	}
};
