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
		std::shared_ptr<StreamElementsCompositionBase> composition);
	virtual ~StreamElementsOutputBase();

	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

	virtual bool CanRemove() { return !IsActive(); }
	virtual bool CanChange() { return !IsActive(); }

	virtual bool IsEnabled();
	virtual void SetEnabled(bool enabled);

	virtual bool Start();
	virtual void Stop();

	virtual bool IsActive() { return false; }
	virtual bool CanDisable() { return false; }

	void SerializeOutput(CefRefPtr<CefValue> &output);

	virtual void
	SerializeStreamingSettings(CefRefPtr<CefValue> &output) = 0;

protected:
	virtual bool CanStart();

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
			compositionInfo) = 0;
	virtual void StopInternal() = 0;

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);
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

	virtual bool IsActive() override;
	virtual bool CanDisable() override;

	virtual void
	SerializeStreamingSettings(CefRefPtr<CefValue> &output) override;

protected:
	virtual bool
		StartInternal(std::shared_ptr<StreamElementsCompositionBase::CompositionInfo> compositionInfo);
	virtual void StopInternal();
};

class StreamElementsObsNativeOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition)
		: StreamElementsOutputBase(id, name, composition)
	{
	}

	virtual ~StreamElementsObsNativeOutput() {}

	virtual bool CanRemove() override { return false; }
	virtual bool CanChange() override  { return false; }
	virtual bool CanDisable() override { return false; }

	virtual bool IsActive() override;

protected:
	virtual bool StartInternal(
		std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
			compositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeStreamingSettings(CefRefPtr<CefValue> &output) override;
};
