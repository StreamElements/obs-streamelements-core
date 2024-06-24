#pragma once

#include "StreamElementsComposition.hpp"

class StreamElementsOutputBase : public StreamElementsCompositionEventListener {
private:
	std::string m_id;
	std::string m_name;
	bool m_enabled;
	std::shared_ptr<StreamElementsCompositionBase> m_composition;
	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
		m_compositionInfo;
	std::recursive_mutex m_mutex;

protected:
	bool IsObsNativeComposition()
	{
		if (!m_compositionInfo)
			return false;

		return m_compositionInfo->IsObsNative();
	}

public:
	StreamElementsOutputBase(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition)
		: m_id(id),
		  m_name(name),
		  m_composition(composition),
		  m_enabled(false)
	{
		m_compositionInfo = composition->GetCompositionInfo(this);
	}

	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

	virtual ~StreamElementsOutputBase() {}

	bool CanRemove() { return !IsActive() && !IsObsNativeComposition(); }

	bool CanChange() { return !IsActive() && !IsObsNativeComposition(); }

	virtual bool IsEnabled();
	virtual void SetEnabled(bool enabled);

	virtual bool Start();
	virtual void Stop();

	virtual bool IsActive() = 0;

protected:
	virtual bool CanStart();

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
			compositionInfo) = 0;
	virtual void StopInternal() = 0;
};

class StreamElementsCustomOutput
	: public StreamElementsOutputBase
{
private:
	std::recursive_mutex m_mutex;
	bool m_active;

	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
		m_compositionInfo = nullptr;

	obs_service_t *m_service;
	obs_output_t *m_output = nullptr;

	std::string m_bindToIP;

public:
	StreamElementsCustomOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition,
		obs_service_t *service, const char *bindToIP)
		: StreamElementsOutputBase(id, name, composition),
		  m_service(service),
		  m_active(false)
	{
		if (bindToIP) {
			m_bindToIP = bindToIP;
		} else {
			m_bindToIP = "";
		}
	}

	virtual ~StreamElementsCustomOutput()
	{
		Stop();

		obs_service_release(m_service);
		m_service = nullptr;
	}

	virtual bool IsActive();

protected:
	virtual bool
		StartInternal(std::shared_ptr<StreamElementsCompositionBase::CompositionInfo> compositionInfo);
	virtual void StopInternal();
};

/*
class StreamElementsObsNativeOutput
	: public StreamElementsOutputBase
{
public:
	StreamElementsObsNativeOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition)
		: StreamElementsOutputBase(id, name, composition)
	{
	}

	virtual ~StreamElementsObsNativeOutput()
	{

	}
}
*/
