#pragma once

#include <obs.h>
#include "cef-headers.hpp"

class StreamElementsCompositionEventListener {
public:
	StreamElementsCompositionEventListener() {}
	~StreamElementsCompositionEventListener() {}

public:
	//virtual void CompositionChanged(){};
};

class StreamElementsCompositionBase {
public:
	class StreamElementsCompositionInfo {
	private:
		StreamElementsCompositionBase *m_owner;
		StreamElementsCompositionEventListener *m_listener;

	public:
		StreamElementsCompositionInfo(
			StreamElementsCompositionBase *owner,
			StreamElementsCompositionEventListener* listener)
			: m_owner(owner), m_listener(listener)
		{
			m_owner->AddRef();
		}

		virtual ~StreamElementsCompositionInfo() {
			m_owner->RemoveRef();
		}

	public:
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

	virtual std::shared_ptr<StreamElementsCompositionInfo>
		GetCompositionInfo(StreamElementsCompositionEventListener* listener) = 0;

private:
	std::string m_id;
	std::string m_name;

protected:
	StreamElementsCompositionBase(const std::string name, const std::string id): m_id(id), m_name(name) {}
	virtual ~StreamElementsCompositionBase() {}

public:
	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

public:
	virtual void SerializeComposition(CefRefPtr<CefValue> &output) = 0;
};

// OBS Main Composition
class StreamElementsDefaultComposition : public StreamElementsCompositionBase,
	  public std::enable_shared_from_this<
		  StreamElementsDefaultComposition> {
private:
	struct Private {
		explicit Private() = default;
	};

public:
	// ctor only usable by this class
	StreamElementsDefaultComposition(Private)
		: StreamElementsCompositionBase("Default", "default")
	{

	}

	virtual ~StreamElementsDefaultComposition() {
	}

public:
	static std::shared_ptr<StreamElementsCompositionBase> Create()
	{
		return std::make_shared<StreamElementsDefaultComposition>(
			Private());
	}

public:
	virtual std::shared_ptr<
		StreamElementsCompositionBase::StreamElementsCompositionInfo>
		GetCompositionInfo(StreamElementsCompositionEventListener* listener);

	virtual bool CanRemove() { return false; }

	virtual void SerializeComposition(CefRefPtr<CefValue> &output);
};
