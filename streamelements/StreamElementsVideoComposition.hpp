#pragma once

#include <obs.h>
#include "cef-headers.hpp"

class StreamElementsCompositionEventListener {
public:
	StreamElementsCompositionEventListener() {}
	~StreamElementsCompositionEventListener() {}

public:
	//virtual void StopOutputRequested();
};

class StreamElementsVideoCompositionBase {
public:
	class CompositionInfo {
	private:
		std::shared_ptr<StreamElementsVideoCompositionBase> m_owner;
		StreamElementsCompositionEventListener *m_listener;

	public:
		CompositionInfo(
			std::shared_ptr<StreamElementsVideoCompositionBase> owner,
			StreamElementsCompositionEventListener* listener)
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
		virtual obs_encoder_t *
		GetStreamingAudioEncoder(size_t index) = 0;

		virtual video_t *GetVideo() = 0;
		virtual audio_t *GetAudio() = 0;

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
		GetCompositionInfo(StreamElementsCompositionEventListener* listener) = 0;

private:
	std::string m_id;
	std::string m_name;

protected:
	StreamElementsVideoCompositionBase(
		const std::string id, const std::string name)
		: m_id(id), m_name(name)
	{
	}
	virtual ~StreamElementsVideoCompositionBase() {}

public:
	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

public:
	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;

	virtual obs_scene_t *GetCurrentScene() = 0;
	virtual void AddScene(obs_scene_t *scene) = 0;
	virtual bool RemoveScene(obs_scene_t *scene) = 0;
	virtual bool SetCurrentScene(obs_scene_t *scene) = 0;
	virtual void GetAllScenes(std::vector<obs_scene_t *> &scenes) = 0;
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
	virtual std::shared_ptr<
		StreamElementsVideoCompositionBase::CompositionInfo>
		GetCompositionInfo(StreamElementsCompositionEventListener* listener);

	virtual bool CanRemove() { return false; }

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);

	virtual obs_scene_t *GetCurrentScene();

	virtual void AddScene(obs_scene_t *scene);
	virtual bool RemoveScene(obs_scene_t *scene);
	virtual bool SetCurrentScene(obs_scene_t *scene);
	virtual void GetAllScenes(std::vector<obs_scene_t *> &scenes);
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
	static std::shared_ptr<StreamElementsVideoCompositionBase>
	Create(std::string id, std::string name, uint32_t width,
	       uint32_t height, std::string streamingVideoEncoderId, obs_data_t* streamingVideoEncoderSettings, obs_data_t* streamingVideoEncoderHotkeyData)
	{
		return std::make_shared<StreamElementsCustomVideoComposition>(
			Private(), id, name, width, height, streamingVideoEncoderId, streamingVideoEncoderSettings, streamingVideoEncoderHotkeyData);
	}

	static std::shared_ptr<StreamElementsVideoCompositionBase>
	Create(std::string id, std::string name, uint32_t width,
	       uint32_t height, CefRefPtr<CefValue> streamingVideoEncoders);

private:
	obs_encoder_t *m_streamingVideoEncoder = nullptr;
	obs_view_t *m_view = nullptr;
	video_t *m_video = nullptr;

	obs_source_t *m_transition = nullptr;

	std::vector<obs_scene_t *> m_scenes;
	obs_scene_t *m_currentScene = nullptr;

public:
	virtual std::shared_ptr<
		StreamElementsVideoCompositionBase::CompositionInfo>
	GetCompositionInfo(StreamElementsCompositionEventListener *listener);

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);

	virtual obs_scene_t *GetCurrentScene();

	virtual void AddScene(obs_scene_t *scene);
	virtual bool RemoveScene(obs_scene_t *scene);
	virtual bool SetCurrentScene(obs_scene_t *scene);
	virtual void GetAllScenes(std::vector<obs_scene_t *> &scenes);
};
