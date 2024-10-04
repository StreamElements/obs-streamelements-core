#pragma once

#include <obs.h>
#include <obs-frontend-api.h>
#include "cef-headers.hpp"

#include "StreamElementsUtils.hpp"
#include "StreamElementsScenesListWidgetManager.hpp"

class StreamElementsVideoCompositionEventListener {
public:
	StreamElementsVideoCompositionEventListener() {}
	~StreamElementsVideoCompositionEventListener() {}

public:
	//virtual void StopOutputRequested();
};

class StreamElementsVideoCompositionBase {
public:
	class CompositionInfo {
	private:
		std::shared_ptr<StreamElementsVideoCompositionBase> m_owner;
		StreamElementsVideoCompositionEventListener *m_listener;

	public:
		CompositionInfo(
			std::shared_ptr<StreamElementsVideoCompositionBase> owner,
			StreamElementsVideoCompositionEventListener* listener)
			: m_owner(owner), m_listener(listener)
		{
			m_owner->AddRef();
		}

		virtual ~CompositionInfo() {
			m_owner->RemoveRef();
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

		virtual obs_encoder_t *GetStreamingVideoEncoder() = 0;

		virtual obs_encoder_t *GetRecordingVideoEncoder() = 0;

		virtual video_t *GetVideo() = 0;

		virtual void GetVideoBaseDimensions(uint32_t *videoWidth,
						    uint32_t *videoHeight) = 0;

		virtual void Render() = 0;
	};

private:
	size_t m_refCounter = 0;
	std::mutex m_refCounterMutex;

	void AddRef()
	{
		std::lock_guard<decltype(m_refCounterMutex)> lock(
			m_refCounterMutex);

		++m_refCounter;
	}

	void RemoveRef() {
		std::lock_guard<decltype(m_refCounterMutex)> lock(
			m_refCounterMutex);

		--m_refCounter;
	}

public:
	virtual bool CanRemove()
	{
		std::lock_guard<decltype(m_refCounterMutex)> lock(
			m_refCounterMutex);

		return m_refCounter == 0;
	}

	virtual std::shared_ptr<CompositionInfo>
		GetCompositionInfo(StreamElementsVideoCompositionEventListener* listener) = 0;

private:
	std::string m_id;
	std::string m_name;

	std::recursive_mutex m_mutex;

protected:
	StreamElementsVideoCompositionBase(
		const std::string id, const std::string name)
		: m_id(id), m_name(name)
	{
	}
	virtual ~StreamElementsVideoCompositionBase() {}

public:
	std::string GetId() { return m_id; }

	std::string GetName()
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		return m_name;
	}

	void SetName(std::string name);

	std::string GetUniqueSceneName(std::string name);

	obs_scene_t* GetSceneById(std::string id) {
		if (!id.size())
			return GetCurrentScene();

		std::vector<obs_scene_t *> scenes;
		GetAllScenes(scenes);

		auto pointer = GetPointerFromId(id.c_str());

		for (auto scene : scenes) {
			auto id1 = GetIdFromPointer(scene);
			auto id2 =
				GetIdFromPointer(obs_scene_get_source(scene));

			if (id == id1 || id == id2)
				return scene;
		}

		return nullptr;
	}

	obs_scene_t *GetSceneByName(std::string name)
	{
		if (!name.size())
			return GetCurrentScene();

		std::vector<obs_scene_t *> scenes;
		GetAllScenes(scenes);

		for (auto scene : scenes) {
			if (stricmp(obs_source_get_name(obs_scene_get_source(scene)), name.c_str()) == 0)
				return scene;
		}

		return nullptr;
	}

	bool SerializeScene(
		std::string id,
		CefRefPtr<CefValue>& result) {
		return SerializeScene(GetSceneById(id), result);
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

public:
	virtual bool IsObsNativeComposition() = 0;

	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;

	virtual obs_scene_t *GetCurrentScene() = 0;
	virtual obs_scene_t *AddScene(std::string requestName) = 0;
	virtual bool SetCurrentScene(obs_scene_t *scene) = 0;
	virtual void GetAllScenes(std::vector<obs_scene_t *> &scenes) = 0;

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
		: StreamElementsVideoCompositionBase("default", "Default")
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

public:
	virtual bool IsObsNativeComposition() { return true; }

	virtual std::shared_ptr<
		StreamElementsVideoCompositionBase::CompositionInfo>
		GetCompositionInfo(StreamElementsVideoCompositionEventListener* listener);

	virtual bool CanRemove() { return false; }

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);
	
	virtual obs_scene_t *GetCurrentScene();

	virtual obs_scene_t *AddScene(std::string requestName);
	virtual bool SetCurrentScene(obs_scene_t *scene);
	virtual void GetAllScenes(std::vector<obs_scene_t *> &scenes);

	virtual void TakeScreenshot() override {
		obs_frontend_take_screenshot();
	}

protected:
	virtual bool RemoveScene(obs_scene_t *scene);

	virtual void SetTransition(obs_source_t *transition);
	virtual obs_source_t *GetTransition();

	virtual int GetTransitionDurationMilliseconds()
	{
		return obs_frontend_get_transition_duration();
	}

	virtual void SetTransitionDurationMilliseconds(int duration);
};

// Custom Composition
class StreamElementsCustomVideoComposition
	: public StreamElementsVideoCompositionBase,
	  public std::enable_shared_from_this<
		  StreamElementsVideoCompositionBase> {
private:
	struct Private {
		explicit Private() = default;
	};

private:
	std::recursive_mutex m_mutex;

	uint32_t m_baseWidth;
	uint32_t m_baseHeight;

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
	obs_scene_t *m_currentScene = nullptr;

public:
	virtual bool IsObsNativeComposition() { return false; }

	virtual std::shared_ptr<
		StreamElementsVideoCompositionBase::CompositionInfo>
	GetCompositionInfo(StreamElementsVideoCompositionEventListener *listener);

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);

	virtual obs_scene_t *GetCurrentScene();

	virtual obs_scene_t *AddScene(std::string requestName);
	virtual bool SetCurrentScene(obs_scene_t *scene);
	virtual void GetAllScenes(std::vector<obs_scene_t *> &scenes);

	virtual void TakeScreenshot() override
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		obs_frontend_take_source_screenshot(m_transition);
	}

protected:
	virtual bool RemoveScene(obs_scene_t *scene);
	virtual void SetTransition(obs_source_t *transition);
	virtual obs_source_t *GetTransition();

	virtual int GetTransitionDurationMilliseconds()
	{
		return m_transitionDurationMs;
	}

	virtual void SetTransitionDurationMilliseconds(int duration);
};
