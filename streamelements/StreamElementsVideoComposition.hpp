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
	StreamElementsVideoCompositionBase(const std::string name, const std::string id): m_id(id), m_name(name) {}
	virtual ~StreamElementsVideoCompositionBase() {}

public:
	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

public:
	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;
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
		: StreamElementsVideoCompositionBase("Default", "default")
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
};
