#pragma once

#include <strings.h>
#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include "cef-headers.hpp"

#include "StreamElementsUtils.hpp"
#include "StreamElementsScenesListWidgetManager.hpp"

#include <shared_mutex>

class StreamElementsVideoCompositionEventListener {
public:
	StreamElementsVideoCompositionEventListener() {}
	~StreamElementsVideoCompositionEventListener() {}

public:
	//virtual void StopOutputRequested();
};

class StreamElementsVideoCompositionBase {
public:
	typedef std::vector<OBSSceneAutoRelease> scenes_t;

private:
	char m_header[7] = "header";
	std::string m_internal_type = "";

	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);

public:
	class CompositionInfo {
	private:
		std::shared_ptr<StreamElementsVideoCompositionBase> m_owner;
		StreamElementsVideoCompositionEventListener *m_listener;
		std::string m_holder;

	public:
		CompositionInfo(
			std::shared_ptr<StreamElementsVideoCompositionBase> owner,
			StreamElementsVideoCompositionEventListener* listener,
			std::string holder)
			: m_owner(owner), m_listener(listener), m_holder(holder)
		{
			m_owner->AddRef(m_holder);
		}

		virtual ~CompositionInfo() { m_owner->RemoveRef(m_holder);
		}

	public:
		std::shared_ptr<StreamElementsVideoCompositionBase> GetComposition()
		{
			return m_owner;
		}

	public:
		// When true, consumers probably want to opt-in to streaming
		// only when OBS main streaming output is active.
		virtual bool IsObsNative() = 0;

		virtual video_t *GetVideo() = 0;

		virtual void GetVideoBaseDimensions(uint32_t *videoWidth,
						    uint32_t *videoHeight) = 0;

		virtual void Render() = 0;

		obs_encoder_t* GetStreamingVideoEncoderRef() {
			auto result = GetStreamingVideoEncoder();

			if (result) {
				result = obs_encoder_get_ref(result);
			}

			return result;
		}

		obs_encoder_t* GetRecordingVideoEncoderRef() {
			auto result = GetRecordingVideoEncoder();

			if (result) {
				result = obs_encoder_get_ref(result);
			}

			return result;
		}

	protected:
		virtual obs_encoder_t *GetStreamingVideoEncoder() = 0;
		virtual obs_encoder_t *GetRecordingVideoEncoder() = 0;
	};

private:
	long m_refCounter = 0;
	std::map<std::string,int> m_holders;
	std::shared_mutex m_holders_mutex;

	void AddRef(std::string holder)
	{
		os_atomic_inc_long(&m_refCounter);

		std::unique_lock<decltype(m_holders_mutex)> lock(
			m_holders_mutex);

		if (!m_holders.count(holder))
			m_holders[holder] = 1;
		else
			m_holders[holder]++;

		if (IsTraceLogLevel()) {
			std::string str = "";
			for (auto pair : m_holders) {
				if (str.size())
					str += ", ";

				str += pair.first;
				str += " = ";
				char buf[32];
				snprintf(buf, sizeof(buf), "%d", pair.second);
				str += buf;
			}

			blog(LOG_ERROR,
			     "[obs-streamelements-core]: videoComposition('%s').addRef('%s'); refcount: %ld; ref holders: %s",
			     m_id.c_str(), holder.c_str(), m_refCounter,
			     str.c_str());
		}
	}

	void RemoveRef(std::string holder) {
		os_atomic_dec_long(&m_refCounter);

		std::unique_lock<decltype(m_holders_mutex)> lock(
			m_holders_mutex);

		if (!m_holders.count(holder)) {
			blog(LOG_ERROR,
			     "[obs-streamelements-core]: invalid holder value '%s' when removing reference from video composition '%s'",
			     holder.c_str(), m_id.c_str());
		} else {
			m_holders[holder]--;

			if (m_holders[holder] <= 0) {
				m_holders.erase(holder);
			}
		}

		if (IsTraceLogLevel()) {
			std::string str = "";
			for (auto pair : m_holders) {
				if (str.size())
					str += ", ";

				str += pair.first;
				str += " = ";
				char buf[32];
				snprintf(buf, sizeof(buf), "%d", pair.second);
				str += buf;
			}

			blog(LOG_ERROR,
			     "[obs-streamelements-core]: videoComposition('%s').removeRef('%s'); remaining refs: %ld; ref holders: %s",
			     m_id.c_str(), holder.c_str(), m_refCounter,
			     str.c_str());
		}
	}

public:
	virtual bool CanRemove()
	{
		return os_atomic_load_long(&m_refCounter) == 0;
	}

	virtual std::shared_ptr<CompositionInfo>
		GetCompositionInfo(StreamElementsVideoCompositionEventListener* listener, std::string holder) = 0;

private:
	std::string m_id;
	std::string m_name;

	std::shared_mutex m_mutex;

protected:
	StreamElementsVideoCompositionBase(const std::string internal_type,
					   const std::string id,
					   const std::string name)
		: m_internal_type(internal_type), m_id(id), m_name(name)
	{
		obs_frontend_add_event_callback(
			StreamElementsVideoCompositionBase::
				handle_obs_frontend_event,
			this);
	}
	virtual ~StreamElementsVideoCompositionBase() {
		obs_frontend_remove_event_callback(
			StreamElementsVideoCompositionBase::
				handle_obs_frontend_event,
			this);

		// Make sure we clean up when canvas is destroyed
		HandleObsSceneCollectionCleanup();
	}

	virtual void HandleObsSceneCollectionCleanup() {}

public:
	std::string GetId() { return m_id; }

	std::string GetName()
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return m_name;
	}

	void SetName(std::string name);

	std::string GetUniqueSceneName(std::string name);

	bool HasSceneId(std::string id) {
		if (!id.size())
			return true;

		scenes_t scenes;
		{
			std::shared_lock<decltype(m_mutex)> lock(m_mutex);
			GetAllScenesInternal(scenes);
		}

		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			auto id1 = GetIdFromPointer(*it);
			auto id2 =
				GetIdFromPointer(obs_scene_get_source(*it));

			if (id == id1 || id == id2)
				return true;
		}

		return false;
	}

	obs_scene_t* GetSceneByIdRef(std::string id) {
		if (!id.size())
			return GetCurrentSceneRef();

		scenes_t scenes;
		{
			std::shared_lock<decltype(m_mutex)> lock(m_mutex);
			GetAllScenesInternal(scenes);
		}

		//auto pointer = GetPointerFromId(id.c_str());

		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			auto id1 = GetIdFromPointer(*it);
			auto id2 =
				GetIdFromPointer(obs_scene_get_source(*it));

			if (id == id1 || id == id2)
				return obs_scene_get_ref(*it);
		}

		return nullptr;
	}

	bool HasSceneName(std::string name)
	{
		if (!name.size())
			return true;

		scenes_t scenes;
		{
			std::shared_lock<decltype(m_mutex)> lock(m_mutex);
			GetAllScenesInternal(scenes);
		}

		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			if (strcasecmp(obs_source_get_name(
					    obs_scene_get_source(*it)),
				    name.c_str()) == 0)
				return true;
		}

		return false;
	}

	obs_scene_t *GetSceneByNameRef(std::string name)
	{
		if (!name.size())
			return GetCurrentSceneRef();

		scenes_t scenes;
		{
			std::shared_lock<decltype(m_mutex)> lock(m_mutex);
			GetAllScenesInternal(scenes);
		}

		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			if (strcasecmp(obs_source_get_name(
					    obs_scene_get_source(*it)),
				    name.c_str()) == 0)
				return obs_scene_get_ref(*it);
		}

		return nullptr;
	}

	bool HasScene(obs_scene_t *scene)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		scenes_t scenes;
		GetAllScenesInternal(scenes);

		for (auto it = scenes.cbegin(); it != scenes.cend(); ++it) {
			if (*it == scene)
				return true;
		}

		return false;
	}

	bool SerializeScene(
		std::string id,
		CefRefPtr<CefValue>& result) {
		OBSSceneAutoRelease scene = GetSceneByIdRef(id);

		return SerializeScene(scene, result);
	}

	bool SerializeScene(
		obs_scene_t* scene,
		CefRefPtr<CefValue>& result) {
		result->SetNull();

		if (!scene)
			return false;

		return SerializeScene(obs_scene_get_source(scene), result);
	}

	bool SerializeScene(obs_source_t* scene, CefRefPtr<CefValue>& result) {
		result->SetNull();

		if (!scene)
			return false;

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("id", GetIdFromPointer(scene));
		d->SetString("videoCompositionId", GetId());
		d->SetString("name", obs_source_get_name(scene));

		d->SetBool("active",
			   GetCurrentScene() == obs_scene_from_source(scene));

#if SE_ENABLE_SCENE_ICONS
		d->SetValue("icon",
			    StreamElementsScenesListWidgetManager::GetSceneIcon(
				    scene)
				    ->Copy());
#endif

		d->SetValue("auxiliaryData",
			    StreamElementsScenesListWidgetManager::
				    GetSceneAuxiliaryData(scene)
					    ->Copy());

#if SE_ENABLE_SCENE_DEFAULT_ACTION
		d->SetValue("defaultAction",
			    StreamElementsScenesListWidgetManager::
				    GetSceneDefaultAction(scene)
					    ->Copy());
#endif

#if SE_ENABLE_SCENE_CONTEXT_MENU
		d->SetValue("contextMenu",
			    StreamElementsScenesListWidgetManager::
				    GetSceneContextMenu(scene)
					    ->Copy());
#endif

		result->SetDictionary(d);

		return true;
	}

protected:
	virtual obs_scene_t *GetCurrentScene() = 0;
	virtual void GetAllScenesInternal(scenes_t &scenes) = 0;

public:
	virtual bool IsObsNativeComposition() = 0;

	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;

	virtual obs_scene_t *GetCurrentSceneRef() = 0;
	virtual obs_scene_t *AddScene(std::string requestName) = 0;
	virtual bool SetCurrentScene(obs_scene_t *scene) = 0;

	virtual void TakeScreenshot() = 0;

	bool SafeRemoveScene(obs_scene_t *sceneToRemove);
	obs_sceneitem_t *GetSceneItemById(std::string id, bool addRef = false);
	obs_sceneitem_t *GetSceneItemById(std::string id, obs_scene_t** result_scene, bool addRef = false);

	obs_sceneitem_t *GetSceneItemByName(std::string name,
					    obs_scene_t **result_scene,
					    bool addRef = false);
	obs_sceneitem_t *GetSceneItemByName(std::string name,
					    bool addRef = false);

	void SerializeTransition(CefRefPtr<CefValue> &output);
	void DeserializeTransition(CefRefPtr<CefValue> input,
				   CefRefPtr<CefValue> &output);

	void GetAllScenes(scenes_t &scenes)
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		GetAllScenesInternal(scenes);
	}

protected:
	virtual bool RemoveScene(obs_scene_t *scene) = 0;
	virtual void SetTransition(obs_source_t *transition) = 0;
	virtual obs_source_t *GetTransition() = 0;

	virtual int GetTransitionDurationMilliseconds() = 0;
	virtual void SetTransitionDurationMilliseconds(int duration) = 0;
};

// OBS Main Composition
class StreamElementsObsNativeVideoComposition : public StreamElementsVideoCompositionBase,
	  public std::enable_shared_from_this<StreamElementsVideoCompositionBase> {
private:
	struct Private {
		explicit Private() = default;
	};

public:
	// ctor only usable by this class
	StreamElementsObsNativeVideoComposition(Private)
		: StreamElementsVideoCompositionBase("obs_native_video_composition", "default", "Default")
	{

	}

	virtual ~StreamElementsObsNativeVideoComposition() {
	}

public:
	static std::shared_ptr<StreamElementsVideoCompositionBase> Create()
	{
		return std::make_shared<StreamElementsObsNativeVideoComposition>(
			Private());
	}

protected:
	virtual obs_scene_t *GetCurrentScene() override;
	virtual void GetAllScenesInternal(scenes_t &scenes) override;

public:
	virtual bool IsObsNativeComposition() override { return true; }

	virtual std::shared_ptr<
		StreamElementsVideoCompositionBase::CompositionInfo>
		GetCompositionInfo(StreamElementsVideoCompositionEventListener* listener, std::string holder) override;

	virtual bool CanRemove() override { return false; }

	virtual void SerializeComposition(CefRefPtr<CefValue> &output) override;
	
	virtual obs_scene_t *GetCurrentSceneRef() override;

	virtual obs_scene_t *AddScene(std::string requestName) override;
	virtual bool SetCurrentScene(obs_scene_t *scene) override;

	virtual void TakeScreenshot() override {
		obs_frontend_take_screenshot();
	}

protected:
	virtual bool RemoveScene(obs_scene_t *scene) override;

	virtual void SetTransition(obs_source_t *transition) override;
	virtual obs_source_t *GetTransition() override;

	virtual int GetTransitionDurationMilliseconds() override
	{
		return obs_frontend_get_transition_duration();
	}

	virtual void SetTransitionDurationMilliseconds(int duration) override;
};

class SESignalHandlerData;

// Custom Composition
class StreamElementsCustomVideoComposition
	: public StreamElementsVideoCompositionBase,
	  public std::enable_shared_from_this<
		  StreamElementsCustomVideoComposition> {
private:
	struct Private {
		explicit Private() = default;
	};

	SESignalHandlerData *m_signalHandlerData = nullptr;

private:
	std::shared_mutex m_mutex;

	uint32_t m_baseWidth;
	uint32_t m_baseHeight;

	obs_source_t *m_audioWrapperSource = nullptr;

	std::shared_mutex m_currentSceneMutex;
	obs_scene_t *m_currentScene = nullptr;

public:
	// ctor only usable by this class
	StreamElementsCustomVideoComposition(
		Private, std::string id, std::string name, uint32_t baseWidth,
		uint32_t baseHeight, std::string streamingVideoEncoderId,
		obs_data_t *streamingVideoEncoderSettings,
		obs_data_t *streamingVideoEncoderHotkeyData);

	virtual ~StreamElementsCustomVideoComposition();

public:
	static std::shared_ptr<StreamElementsCustomVideoComposition>
	Create(std::string id, std::string name, uint32_t width,
	       uint32_t height, std::string streamingVideoEncoderId, obs_data_t* streamingVideoEncoderSettings, obs_data_t* streamingVideoEncoderHotkeyData)
	{
		return std::make_shared<StreamElementsCustomVideoComposition>(
			Private(), id, name, width, height, streamingVideoEncoderId, streamingVideoEncoderSettings, streamingVideoEncoderHotkeyData);
	}

	static std::shared_ptr<StreamElementsCustomVideoComposition>
	Create(std::string id, std::string name, uint32_t width,
	       uint32_t height, CefRefPtr<CefValue> streamingVideoEncoders,
	       CefRefPtr<CefValue> recordingVideoEncoders);

private:
	void SetRecordingEncoder(std::string recordingVideoEncoderId,
				 obs_data_t *recordingVideoEncoderSettings,
				 obs_data_t *recordingVideoEncoderHotkeyData);

private:
	obs_encoder_t *m_streamingVideoEncoder = nullptr;
	obs_encoder_t *m_recordingVideoEncoder = nullptr;
	obs_view_t *m_view = nullptr;
	video_t *m_video = nullptr;

	obs_source_t *m_transition = nullptr;
	int m_transitionDurationMs = 0;

	std::vector<obs_scene_t *> m_scenes;

protected:
	virtual obs_scene_t *GetCurrentScene() override;
	virtual void GetAllScenesInternal(scenes_t &scenes) override;

public:
	virtual bool IsObsNativeComposition() override { return false; }

	virtual std::shared_ptr<
		StreamElementsVideoCompositionBase::CompositionInfo>
	GetCompositionInfo(StreamElementsVideoCompositionEventListener *listener, std::string holder) override;

	virtual void SerializeComposition(CefRefPtr<CefValue> &output) override;

	virtual obs_scene_t *GetCurrentSceneRef() override;

	virtual obs_scene_t *AddScene(std::string requestName) override;
	virtual bool SetCurrentScene(obs_scene_t *scene) override;

	virtual void TakeScreenshot() override
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		obs_frontend_take_source_screenshot(m_transition);
	}

protected:
	virtual bool RemoveScene(obs_scene_t *scene) override;
	virtual void SetTransition(obs_source_t *transition) override;
	virtual obs_source_t *GetTransition() override;

	virtual int GetTransitionDurationMilliseconds() override
	{
		return m_transitionDurationMs;
	}

	virtual void SetTransitionDurationMilliseconds(int duration) override;

	virtual void HandleObsSceneCollectionCleanup() override;

public:
	void ProcessRenderingRoot(std::function<void(obs_source_t *)> callback);
};
