#pragma once
#include "SECompositionServices.hpp"
#include "ISEAudioCompositionManager.hpp"

#include "SEAudioComposition.hpp"

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include "cef-headers.hpp"
#include "SEUtils.hpp"

#include <string>
#include <map>
#include <shared_mutex>

#include <memory>

//
// Forward declaration rather than an include: the real header drags windows.h
// in ahead of winsock2.h on these translation units.
//
class SEWebsocketApiServer;

class SEAudioCompositionManager : public ISEAudioCompositionManager {
private:
	std::shared_ptr<SEWebsocketApiServer> m_apiServer;

	std::shared_mutex m_mutex;
	std::map<std::string,
		 std::shared_ptr<SEAudioCompositionBase>>
		m_audioCompositionsMap;
	std::shared_ptr<SEAudioCompositionBase>
		m_nativeAudioComposition;

	std::map<std::string, CefRefPtr<CefDictionaryValue>>
		m_availableEncoderClassesCache;

	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;
	std::shared_ptr<SEMessageBus> m_messageBus;

	//
	// Audio compositions announce too; the video manager handle in here
	// is unused on this side.
	//
	SECompositionServices CompositionServices()
	{
		return {m_hostEventDispatcher, m_messageBus, {}};
	}

public:
	SEAudioCompositionManager(
		std::shared_ptr<SEWebsocketApiServer> apiServer,
		std::shared_ptr<SEHostEventDispatcher>
			hostEventDispatcher,
		std::shared_ptr<SEMessageBus> messageBus);
	~SEAudioCompositionManager();

public:
	void DeserializeExistingCompositionProperties(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> &output) override;
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) override;
	void SerializeAllCompositions(CefRefPtr<CefValue> &output) override;
	void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) override;

	void SerializeAvailableEncoderClasses(obs_encoder_type type,
					      CefRefPtr<CefValue> &output);

	void SerializeAvailableTransitionClasses(CefRefPtr<CefValue> &output);

	void Reset();

private:
	//
	// The lock-free bodies behind the three public getters below. Each of
	// those takes m_mutex and delegates here, so no public getter calls
	// another one.
	//
	// Same defect and same fix as SEVideoCompositionManager, which
	// carries the full reasoning: m_mutex is a std::shared_mutex,
	// acquiring one recursively is undefined behaviour, and MSVC backs it
	// with an SRWLOCK that Microsoft documents as non-recursive -- so a
	// writer arriving between the outer and inner shared acquisition
	// deadlocks the inner one. Reset() and DeserializeComposition are the
	// writers here too.
	//
	std::shared_ptr<SEAudioCompositionBase>
	GetObsNativeAudioCompositionInternal()
	{
		return m_nativeAudioComposition;
	}

	std::shared_ptr<SEAudioCompositionBase>
	GetAudioCompositionByIdInternal(std::string id)
	{
		if (!m_audioCompositionsMap.count(id))
			return nullptr;

		return m_audioCompositionsMap[id];
	}

public:
	std::shared_ptr<SEAudioCompositionBase>
	GetObsNativeAudioComposition()
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return GetObsNativeAudioCompositionInternal();
	}

	std::shared_ptr<SEAudioCompositionBase>
	GetAudioCompositionById(std::string id)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return GetAudioCompositionByIdInternal(id);
	}

	std::shared_ptr<SEAudioCompositionBase>
	GetAudioCompositionById(CefRefPtr<CefValue> input)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
			return GetObsNativeAudioCompositionInternal();

		auto d = input->GetDictionary();

		if (!d->HasKey("audioCompositionId") ||
		    d->GetType("audioCompositionId") != VTYPE_STRING)
			return GetObsNativeAudioCompositionInternal();

		std::string id = d->GetString("audioCompositionId");

		return GetAudioCompositionByIdInternal(id);
	}
};
