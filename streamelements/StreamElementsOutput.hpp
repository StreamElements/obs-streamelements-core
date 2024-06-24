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

public:
	StreamElementsOutputBase(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition,
		bool enabled)
		: m_id(id),
		  m_name(name),
		  m_composition(composition),
		  m_enabled(false)
	{
		m_compositionInfo = composition->GetCompositionInfo(this);

		SetEnabled(enabled);
	}

	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

	virtual ~StreamElementsOutputBase() {}

	virtual bool CanRemove() { return false; }
	virtual bool CanChange() { return false; }
	virtual bool CanStart();

	virtual bool IsEnabled();
	virtual void SetEnabled(bool enabled);

	virtual bool Start();
	virtual void Stop();

	virtual bool IsActive() = 0;

protected:
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

public:
	StreamElementsCustomOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition,
		bool enabled,
		obs_service_t *service)
		: StreamElementsOutputBase(id, name, composition, enabled),
		  m_service(service),
		  m_active(false)
	{
	}

	virtual ~StreamElementsCustomOutput()
	{
		Stop();

		obs_service_release(m_service);
		m_service = nullptr;
	}

	virtual bool IsActive();

	virtual bool CanRemove() { return true; }
	virtual bool CanChange() { return !IsActive(); }

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
		std::shared_ptr<StreamElementsCompositionBase> composition,
		bool enabled, obs_service_t *service)
		: StreamElementsOutputBase(id, name, composition, enabled)
	{
	}

	virtual ~StreamElementsObsNativeOutput()
	{

	}

	virtual bool CanRemove() { return false; }
	virtual bool CanChange() { return false; }
	virtual bool CanStart() { return false; }
}
*/
