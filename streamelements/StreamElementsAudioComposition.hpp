#pragma once

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include "cef-headers.hpp"

#include "StreamElementsUtils.hpp"

class StreamElementsAudioCompositionEventListener {
public:
	StreamElementsAudioCompositionEventListener() {}
	~StreamElementsAudioCompositionEventListener() {}

public:
	//virtual void StopOutputRequested();
};

class StreamElementsAudioCompositionBase {
public:
	class CompositionInfo {
	private:
		std::shared_ptr<StreamElementsAudioCompositionBase> m_owner;
		StreamElementsAudioCompositionEventListener *m_listener;

	public:
		CompositionInfo(
			std::shared_ptr<StreamElementsAudioCompositionBase>
				owner,
			StreamElementsAudioCompositionEventListener *listener)
			: m_owner(owner), m_listener(listener)
		{
			m_owner->AddRef();
		}

		virtual ~CompositionInfo() { m_owner->RemoveRef(); }

	public:
		std::shared_ptr<StreamElementsAudioCompositionBase>
		GetComposition()
		{
			return m_owner;
		}

	public:
		// When true, consumers probably want to opt-in to streaming
		// only when OBS main streaming output is active.
		virtual bool IsObsNative() = 0;

		virtual obs_encoder_t *
		GetStreamingAudioEncoder(size_t index) = 0;

		virtual obs_encoder_t *
		GetRecordingAudioEncoder(size_t index) = 0;

		virtual audio_t *GetAudio() = 0;
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

	void RemoveRef()
	{
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

	virtual std::shared_ptr<CompositionInfo> GetCompositionInfo(
		StreamElementsAudioCompositionEventListener *listener) = 0;

private:
	std::string m_id;
	std::string m_name;

	std::recursive_mutex m_mutex;

protected:
	StreamElementsAudioCompositionBase(const std::string id,
					   const std::string name)
		: m_id(id), m_name(name)
	{
	}
	virtual ~StreamElementsAudioCompositionBase() {}

public:
	std::string GetId() { return m_id; }

	std::string GetName()
	{
		std::lock_guard<decltype(m_mutex)> lock(m_mutex);

		return m_name;
	}

	void SetName(std::string name);

public:
	virtual bool IsObsNativeComposition() = 0;

	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;
};

// OBS Main Composition
class StreamElementsObsNativeAudioComposition
	: public StreamElementsAudioCompositionBase,
	  public std::enable_shared_from_this<
		  StreamElementsAudioCompositionBase> {
private:
	struct Private {
		explicit Private() = default;
	};

public:
	// ctor only usable by this class
	StreamElementsObsNativeAudioComposition(Private)
		: StreamElementsAudioCompositionBase("default", "Default")
	{
	}

	virtual ~StreamElementsObsNativeAudioComposition() {}

public:
	static std::shared_ptr<StreamElementsAudioCompositionBase> Create()
	{
		return std::make_shared<StreamElementsObsNativeAudioComposition>(
			Private());
	}

public:
	virtual bool IsObsNativeComposition() { return true; }

	virtual std::shared_ptr<
		StreamElementsAudioCompositionBase::CompositionInfo>
	GetCompositionInfo(
		StreamElementsAudioCompositionEventListener *listener);

	virtual bool CanRemove() { return false; }

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);
};

// Custom Composition
class StreamElementsCustomAudioComposition
	: public StreamElementsAudioCompositionBase,
	  public std::enable_shared_from_this<
		  StreamElementsAudioCompositionBase> {
private:
	struct Private {
		explicit Private() = default;
	};

private:
	std::recursive_mutex m_mutex;

public:
	// ctor only usable by this class
	StreamElementsCustomAudioComposition(
		Private, std::string id, std::string name, std::string streamingAudioEncoderId,
		obs_data_t *streamingAudioEncoderSettings,
		obs_data_t *streamingAudioEncoderHotkeyData);

	virtual ~StreamElementsCustomAudioComposition();

public:
	static std::shared_ptr<StreamElementsCustomAudioComposition>
	Create(std::string id, std::string name,
	       std::string streamingAudioEncoderId,
	       obs_data_t *streamingAudioEncoderSettings,
	       obs_data_t *streamingAudioEncoderHotkeyData)
	{
		return std::make_shared<StreamElementsCustomAudioComposition>(
			Private(), id, name, streamingAudioEncoderId,
			streamingAudioEncoderSettings,
			streamingAudioEncoderHotkeyData);
	}

	static std::shared_ptr<StreamElementsCustomAudioComposition>
	Create(std::string id, std::string name,
	       CefRefPtr<CefValue> streamingAudioEncoders,
	       CefRefPtr<CefValue> recordingAudioEncoders);

private:
	audio_t *m_audio = nullptr;
	obs_encoder_t *m_streamingAudioEncoders[MAX_AUDIO_MIXES] = {nullptr};
	obs_encoder_t *m_recordingAudioEncoders[MAX_AUDIO_MIXES] = {nullptr};

private:
	void SetRecordingEncoder(std::string recordingAudioEncoderId,
				 obs_data_t *recordingAudioEncoderSettings,
				 obs_data_t *recordingAudioEncoderHotkeyData);

		public:
	virtual bool IsObsNativeComposition() { return false; }

	virtual std::shared_ptr<
		StreamElementsAudioCompositionBase::CompositionInfo>
	GetCompositionInfo(
		StreamElementsAudioCompositionEventListener *listener);

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);
};
