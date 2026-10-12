#pragma once

#include <obs.h>
#include <obs.hpp>
#include <obs-frontend-api.h>
#include "cef-headers.hpp"

#include "SEUtils.hpp"

#include <shared_mutex>

class SEAudioCompositionEventListener {
public:
	SEAudioCompositionEventListener() {}
	~SEAudioCompositionEventListener() {}

public:
	//virtual void StopOutputRequested();
};

class SEAudioCompositionBase {
public:
	class CompositionInfo {
	private:
		std::shared_ptr<SEAudioCompositionBase> m_owner;
		SEAudioCompositionEventListener *m_listener;

	public:
		CompositionInfo(
			std::shared_ptr<SEAudioCompositionBase>
				owner,
			SEAudioCompositionEventListener *listener)
			: m_owner(owner), m_listener(listener)
		{
			m_owner->AddRef();
		}

		virtual ~CompositionInfo() { m_owner->RemoveRef(); }

	public:
		std::shared_ptr<SEAudioCompositionBase>
		GetComposition()
		{
			return m_owner;
		}

	protected:
		virtual obs_encoder_t *
		GetStreamingAudioEncoder(size_t index) = 0;

		virtual obs_encoder_t *
		GetRecordingAudioEncoder(size_t index) = 0;

	public:
		// When true, consumers probably want to opt-in to streaming
		// only when OBS main streaming output is active.
		virtual bool IsObsNative() = 0;

		virtual audio_t *GetAudio() = 0;

		obs_encoder_t *
			GetStreamingAudioEncoderRef(size_t index)
		{
			auto result = GetStreamingAudioEncoder(index);

			if (result) {
				result = SETRACE_ADDREF(obs_encoder_get_ref(
					result));
			}

			return result;
		}

		obs_encoder_t *
			GetRecordingAudioEncoderRef(size_t index)
		{
			auto result = GetRecordingAudioEncoder(index);

			if (result) {
				result = SETRACE_ADDREF(obs_encoder_get_ref(result));
			}

			return result;

		}
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
		SEAudioCompositionEventListener *listener) = 0;

private:
	std::string m_id;
	std::string m_name;

	std::shared_mutex m_mutex;

protected:
	SEAudioCompositionBase(const std::string id,
					   const std::string name)
		: m_id(id), m_name(name)
	{
	}
	virtual ~SEAudioCompositionBase() {}

public:
	std::string GetId() { return m_id; }

	std::string GetName()
	{
		std::shared_lock<decltype(m_mutex)> lock(m_mutex);

		return m_name;
	}

	void SetName(std::string name);

public:
	virtual bool IsObsNativeComposition() = 0;

	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;
};

// OBS Main Composition
class SEObsNativeAudioComposition
	: public SEAudioCompositionBase,
	  public std::enable_shared_from_this<
		  SEAudioCompositionBase> {
private:
	struct Private {
		explicit Private() = default;
	};

public:
	// ctor only usable by this class
	SEObsNativeAudioComposition(Private)
		: SEAudioCompositionBase("default", "Default")
	{
	}

	virtual ~SEObsNativeAudioComposition() {}

public:
	static std::shared_ptr<SEAudioCompositionBase> Create()
	{
		return std::make_shared<SEObsNativeAudioComposition>(
			Private());
	}

public:
	virtual bool IsObsNativeComposition() { return true; }

	virtual std::shared_ptr<
		SEAudioCompositionBase::CompositionInfo>
	GetCompositionInfo(
		SEAudioCompositionEventListener *listener);

	virtual bool CanRemove() { return false; }

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);
};

// Custom Composition
class SECustomAudioComposition
	: public SEAudioCompositionBase,
	  public std::enable_shared_from_this<
		  SEAudioCompositionBase> {
private:
	struct Private {
		explicit Private() = default;
	};

private:
	std::shared_mutex m_mutex;

public:
	// ctor only usable by this class
	SECustomAudioComposition(
		Private, std::string id, std::string name, std::string streamingAudioEncoderId,
		obs_data_t *streamingAudioEncoderSettings,
		obs_data_t *streamingAudioEncoderHotkeyData);

	virtual ~SECustomAudioComposition();

public:
	static std::shared_ptr<SECustomAudioComposition>
	Create(std::string id, std::string name,
	       std::string streamingAudioEncoderId,
	       obs_data_t *streamingAudioEncoderSettings,
	       obs_data_t *streamingAudioEncoderHotkeyData)
	{
		return std::make_shared<SECustomAudioComposition>(
			Private(), id, name, streamingAudioEncoderId,
			streamingAudioEncoderSettings,
			streamingAudioEncoderHotkeyData);
	}

	static std::shared_ptr<SECustomAudioComposition>
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
		SEAudioCompositionBase::CompositionInfo>
	GetCompositionInfo(
		SEAudioCompositionEventListener *listener);

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);
};
