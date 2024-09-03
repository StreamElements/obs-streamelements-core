#pragma once

#include <obs-frontend-api.h>
#include "StreamElementsVideoComposition.hpp"

class StreamElementsOutputBase : public StreamElementsCompositionEventListener {
public:
	enum ObsStateDependencyType {
		Streaming,
		Recording,
		None
	};

	enum ObsOutputType {
		StreamingOutput,
		RecordingOutput
	};

private:
	std::string m_id;
	std::string m_name;
	bool m_enabled;
	std::shared_ptr<StreamElementsVideoCompositionBase> m_videoComposition;
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo;
	std::recursive_mutex m_mutex;

	CefRefPtr<CefDictionaryValue> m_auxData;

	bool m_outputEventsConnected = false;

	std::string m_error;

	ObsStateDependencyType m_obsStateDependency = Streaming;
	ObsOutputType m_obsOutputType = StreamingOutput;

public:
	StreamElementsOutputBase(
		std::string id, std::string name,
		ObsOutputType obsOutputType,
		ObsStateDependencyType obsStateDependency,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		CefRefPtr<CefDictionaryValue> auxData);
	virtual ~StreamElementsOutputBase();

	std::string GetId() { return m_id; }
	std::string GetName() { return m_name; }
	ObsOutputType GetOutputType() { return m_obsOutputType; }

	virtual bool CanRemove() { return !IsActive(); }
	virtual bool CanChange() { return !IsActive(); }

	virtual bool IsEnabled();
	virtual void SetEnabled(bool enabled);

	virtual bool IsActive();

	virtual bool CanDisable() { return false; }

	virtual bool IsObsNativeOutput() { return false; }

	void SerializeOutput(CefRefPtr<CefValue> &output);

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) = 0;

	virtual bool CanSplitRecordingOutput() { return false; }
	virtual bool TriggerSplitRecordingOutput() { return false; }

protected:
	virtual obs_output_t *GetOutput() = 0;

	virtual bool CanStart();

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo) = 0;
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

class StreamElementsCustomStreamingOutput
	: public StreamElementsOutputBase
{
private:
	std::recursive_mutex m_mutex;

	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo = nullptr;

	obs_service_t *m_service;
	obs_output_t *m_output = nullptr;

	std::string m_bindToIP;

public:
	StreamElementsCustomStreamingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition,
		obs_service_t *service, const char *bindToIP,
		CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, StreamingOutput, Streaming, videoComposition, auxData),
		  m_service(service)
	{
		if (bindToIP) {
			m_bindToIP = bindToIP;
		} else {
			m_bindToIP = "";
		}
	}

	virtual ~StreamElementsCustomStreamingOutput()
	{
		Stop();

		obs_service_release(m_service);
		m_service = nullptr;
	}

	virtual bool CanDisable() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	static std::shared_ptr<StreamElementsCustomStreamingOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

protected:
	virtual obs_output_t *GetOutput() override { return m_output; }

	virtual bool
		StartInternal(std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo> videoCompositionInfo);
	virtual void StopInternal();
};

class StreamElementsObsNativeStreamingOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeStreamingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition)
		: StreamElementsOutputBase(id, name, StreamingOutput, Streaming, videoComposition, CefDictionaryValue::Create())
	{
	}

	virtual ~StreamElementsObsNativeStreamingOutput()
	{
	}

	virtual bool CanRemove() override { return false; }
	virtual bool CanChange() override  { return false; }
	virtual bool CanDisable() override { return false; }

	virtual bool IsObsNativeOutput() override { return true; }

	virtual bool CanStart() override { return true; }

protected:
	virtual obs_output_t *GetOutput() override { return obs_frontend_get_streaming_output(); }

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;
};

class StreamElementsObsNativeRecordingOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeRecordingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition)
		: StreamElementsOutputBase(id, name, RecordingOutput, Recording,
					   videoComposition,
					   CefDictionaryValue::Create())
	{
	}

	virtual ~StreamElementsObsNativeRecordingOutput() {}

	virtual bool CanRemove() override { return false; }
	virtual bool CanChange() override { return false; }
	virtual bool CanDisable() override { return false; }

	virtual bool IsObsNativeOutput() override { return true; }

	virtual bool CanStart() override { return true; }

protected:
	virtual obs_output_t *GetOutput() override
	{
		return obs_frontend_get_recording_output();
	}

	virtual bool
	StartInternal(std::shared_ptr<
		      StreamElementsVideoCompositionBase::CompositionInfo>
			      videoCompositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;
};

class StreamElementsCustomRecordingOutput : public StreamElementsOutputBase {
private:
	std::recursive_mutex m_mutex;

	std::shared_ptr<StreamElementsVideoCompositionBase>
		m_videoComposition = nullptr;

	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo = nullptr;

	obs_output_t *m_output = nullptr;

	CefRefPtr<CefDictionaryValue> m_recordingSettings = CefDictionaryValue::Create();

public:
	StreamElementsCustomRecordingOutput(
		std::string id, std::string name,
		CefRefPtr<CefDictionaryValue> recordingSettings,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, RecordingOutput, None,
					   videoComposition, auxData),
		  m_recordingSettings(recordingSettings),
		  m_videoComposition(videoComposition)
	{
	}

	virtual ~StreamElementsCustomRecordingOutput()
	{
		Stop();
	}

	virtual bool CanDisable() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	static std::shared_ptr<StreamElementsCustomRecordingOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

	virtual bool CanSplitRecordingOutput() { return true; }
	virtual bool TriggerSplitRecordingOutput();

protected:
	virtual obs_output_t *GetOutput() override { return m_output; }

	virtual bool
	StartInternal(std::shared_ptr<
		      StreamElementsVideoCompositionBase::CompositionInfo>
			      videoCompositionInfo);
	virtual void StopInternal();
};
