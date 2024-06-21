#pragma once

#include "StreamElementsComposition.hpp"

class StreamElementsOutput : public StreamElementsCompositionEventListener {
private:
	std::string m_id;
	std::string m_name;
	std::shared_ptr<StreamElementsCompositionBase> m_composition;

	std::recursive_mutex m_mutex;
	bool m_enabled;
	bool m_active;

	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
		m_compositionInfo = nullptr;

	obs_service_t *m_service;
	obs_output_t *m_output = nullptr;

public:
	StreamElementsOutput(
		std::string id,
		std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition, obs_service_t* service, bool enabled)
		: m_id(id), m_name(name), m_composition(composition), m_service(service), m_enabled(enabled), m_active(false)
	{
		// TODO: Deserialize input
	}

	virtual ~StreamElementsOutput() {
		Stop();

		obs_service_release(m_service);
		m_service = nullptr;
	}

	bool IsEnabled();
	void SetEnabled(bool enabled);

	virtual bool IsActive();

	virtual bool Start();
	virtual void Stop();

protected:
	virtual bool StartInternal();
	virtual void StopInternal();
};
