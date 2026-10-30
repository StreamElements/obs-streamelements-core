#pragma once
#include "SECompositionServices.hpp"

#include "SEVideoComposition.hpp"
#include <shared_mutex>
#include <string>
#include <map>
#include "cef-headers.hpp"
#include "ISEVideoCompositionManager.hpp"

#include <memory>

//
// Forward declaration rather than an include: the real header drags windows.h
// in ahead of winsock2.h on these translation units.
//
class SEWebsocketApiServer;

class SEVideoCompositionManager
	: public std::enable_shared_from_this<SEVideoCompositionManager>,
	  public ISEVideoCompositionManager {
private:
	std::shared_ptr<SEWebsocketApiServer> m_apiServer;

	std::shared_mutex m_mutex;
	std::map<std::string, std::shared_ptr<SEVideoCompositionBase>> m_videoCompositionsMap;
	std::shared_ptr<SEVideoCompositionBase> m_nativeVideoComposition;

	std::map<std::string, CefRefPtr<CefDictionaryValue>>
		m_availableEncoderClassesCache;

	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;
	std::shared_ptr<SEMessageBus> m_messageBus;

	//
	// What every composition this manager creates is handed. The weak
	// self-handle is filled in per call site, because it cannot be taken
	// inside this class's own constructor.
	//
	SECompositionServices CompositionServices()
	{
		return {m_hostEventDispatcher, m_messageBus,
			weak_from_this()};
	}

public:
	SEVideoCompositionManager(
		std::shared_ptr<SEWebsocketApiServer> apiServer,
		std::shared_ptr<SEHostEventDispatcher>
			hostEventDispatcher,
		std::shared_ptr<SEMessageBus> messageBus);
	~SEVideoCompositionManager();

public:
	void DeserializeExistingCompositionProperties(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> &output) override;
	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) override;
	void SerializeAllCompositions(CefRefPtr<CefValue> &output) override;
	void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) override;

	//
	// Split by kind rather than parameterized by one, so the two handler
	// bodies that call them stop naming OBS_ENCODER_VIDEO and
	// OBS_ENCODER_AUDIO. The obs-typed pair below does the work.
	//
	void SerializeAvailableVideoEncoderClasses(
		CefRefPtr<CefValue> &output) override;
	void SerializeAvailableAudioEncoderClasses(
		CefRefPtr<CefValue> &output) override;

	void SerializeAvailableVideoEncoderClassProperties(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> &output) override;
	void SerializeAvailableAudioEncoderClassProperties(
		CefRefPtr<CefValue> input,
		CefRefPtr<CefValue> &output) override;

	void SerializeAvailableEncoderClasses(obs_encoder_type type,
					CefRefPtr<CefValue> &output);

	void SerializeAvailableEncoderClassPropertiesForSettings(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output,
		obs_encoder_type encoder_type);

	void SerializeAvailableTransitionClasses(
		CefRefPtr<CefValue> &output) override;

	void Reset() override;

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

public:
	//
	// ISEVideoCompositionManager's view of the three getters above. They
	// cannot be the same methods: shared_ptr has no return-type
	// covariance, and the eleven callers of the getters above need the
	// concrete SEVideoCompositionBase -- SEBrowserWidget hands it to
	// SetVideoCompositionView, SEOutput reads its CompositionInfo.
	//
	std::shared_ptr<ISEVideoComposition>
	GetVideoComposition(CefRefPtr<CefValue> input) override
	{
		return GetVideoCompositionById(input);
	}

	std::shared_ptr<ISEVideoComposition>
	GetVideoComposition(std::string id) override
	{
		return GetVideoCompositionById(id);
	}

	std::shared_ptr<ISEVideoComposition>
	GetNativeVideoComposition() override
	{
		return GetObsNativeVideoComposition();
	}

public:
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
