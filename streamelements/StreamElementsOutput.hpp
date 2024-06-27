#pragma once

#include <obs-frontend-api.h>
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

	CefRefPtr<CefDictionaryValue> m_auxData;

	bool m_outputEventsConnected = false;

	std::string m_error;

public:
	StreamElementsOutputBase(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition,
		CefRefPtr<CefDictionaryValue> auxData);
	virtual ~StreamElementsOutputBase();

	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }

	virtual bool CanRemove() { return !IsActive(); }
	virtual bool CanChange() { return !IsActive(); }

	virtual bool IsEnabled();
	virtual void SetEnabled(bool enabled);

	virtual bool IsActive();

	virtual bool CanDisable() { return false; }

	virtual bool IsObsNative() { return false; }

	void SerializeOutput(CefRefPtr<CefValue> &output);

	virtual void
	SerializeStreamingSettings(CefRefPtr<CefValue> &output) = 0;

protected:
	virtual obs_output_t *GetOutput() = 0;

	virtual bool CanStart();

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
			compositionInfo) = 0;
	virtual void StopInternal() = 0;

protected:
	bool Start();
	void Stop();

	void ConnectOutputEvents();
	void DisconnectOutputEvents();

private:
	void SetError(std::string error) { m_error = error; }

private:
	static void handle_obs_frontend_event(enum obs_frontend_event event,
					      void *data);

	static void handle_output_start(void *my_data, calldata_t *cd);
	static void handle_output_stop(void *my_data, calldata_t *cd);
	static void handle_output_pause(void *my_data, calldata_t *cd);
	static void handle_output_unpause(void *my_data, calldata_t *cd);
	static void handle_output_starting(void *my_data, calldata_t *cd);
	static void handle_output_stopping(void *my_data, calldata_t *cd);
	static void handle_output_activate(void *my_data, calldata_t *cd);
	static void handle_output_deactivate(void *my_data, calldata_t *cd);
	static void handle_output_reconnect(void *my_data, calldata_t *cd);
	static void handle_output_reconnect_success(void *my_data,
						    calldata_t *cd);
};

class StreamElementsCustomOutput
	: public StreamElementsOutputBase
{
private:
	std::recursive_mutex m_mutex;

	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
		m_compositionInfo = nullptr;

	obs_service_t *m_service;
	obs_output_t *m_output = nullptr;

	std::string m_bindToIP;

public:
	StreamElementsCustomOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition,
		obs_service_t *service, const char *bindToIP,
		CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, composition, auxData),
		  m_service(service)
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

	virtual bool CanDisable() override;

	virtual void
	SerializeStreamingSettings(CefRefPtr<CefValue> &output) override;

	static std::shared_ptr<StreamElementsCustomOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNative() override { return false; }

protected:
	virtual obs_output_t *GetOutput() override { return m_output; }

	virtual bool
		StartInternal(std::shared_ptr<StreamElementsCompositionBase::CompositionInfo> compositionInfo);
	virtual void StopInternal();
};

class StreamElementsObsNativeOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsCompositionBase> composition)
		: StreamElementsOutputBase(id, name, composition, CefDictionaryValue::Create())
	{
	}

	virtual ~StreamElementsObsNativeOutput()
	{
	}

	virtual bool CanRemove() override { return false; }
	virtual bool CanChange() override  { return false; }
	virtual bool CanDisable() override { return false; }

	virtual bool IsObsNative() override { return true; }

	virtual bool CanStart() override { return true; }

protected:
	virtual obs_output_t *GetOutput() override { return obs_frontend_get_streaming_output(); }

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
			compositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeStreamingSettings(CefRefPtr<CefValue> &output) override;
};
