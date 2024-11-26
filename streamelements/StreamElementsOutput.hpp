#pragma once

#include <obs-frontend-api.h>
#include "StreamElementsVideoComposition.hpp"
#include "StreamElementsAudioComposition.hpp"

#include <shared_mutex>

class StreamElementsOutputBase
	: public StreamElementsVideoCompositionEventListener,
	  public StreamElementsAudioCompositionEventListener {
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

protected:
	bool m_isObsStreaming = false;

private:
	std::string m_id;
	std::string m_name;
	bool m_enabled;
	std::shared_ptr<StreamElementsVideoCompositionBase> m_videoComposition;
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo;
	std::shared_ptr<StreamElementsAudioCompositionBase> m_audioComposition;
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		m_audioCompositionInfo;
	std::shared_mutex m_mutex;

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
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition,
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

	virtual std::vector<uint32_t> GetAudioTracks() = 0;

	virtual bool CanSplitRecordingOutput() { return false; }
	virtual bool TriggerSplitRecordingOutput() { return false; }

protected:
	virtual obs_output_t *GetOutput() = 0;

	virtual bool CanStart();

	virtual bool StartInternal(
		std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) = 0;
	virtual void StopInternal() = 0;

protected:
	bool Start();
	void Stop();

	void ConnectOutputEvents();
	void DisconnectOutputEvents();

protected:
	void SetError(std::string error) { m_error = error; }
	void SetErrorIfEmpty(std::string error)
	{
		if (!m_error.size())
			m_error = error;
	}

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
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo = nullptr;

	obs_service_t *m_service;
	obs_output_t *m_output = nullptr;

	std::string m_bindToIP;

	std::vector<uint32_t> m_audioTracks = {0};

public:
	StreamElementsCustomStreamingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition,
		std::vector<uint32_t> audioTracks, obs_service_t *service,
		const char *bindToIP, CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, StreamingOutput, Streaming,
					   videoComposition, audioComposition,
					   auxData),
		  m_service(service),
		  m_audioTracks(audioTracks)
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

	virtual bool CanStart() override
	{
		if (!StreamElementsOutputBase::CanStart())
			return false;

		if (!m_isObsStreaming)
			return false;

		return true;
	}

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	virtual std::vector<uint32_t> GetAudioTracks() override
	{
		return m_audioTracks;
	}

	static std::shared_ptr<StreamElementsCustomStreamingOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

protected:
	virtual obs_output_t *GetOutput() override
	{
		return m_output ? obs_output_get_ref(m_output) : nullptr;
	}

	virtual bool StartInternal(
		std::shared_ptr<
			StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo);
	virtual void StopInternal();
};

class StreamElementsObsNativeStreamingOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeStreamingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition)
		: StreamElementsOutputBase(id, name, StreamingOutput, Streaming, videoComposition, audioComposition, CefDictionaryValue::Create())
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
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	virtual std::vector<uint32_t> GetAudioTracks() override;
};

class StreamElementsObsNativeRecordingOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeRecordingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition)
		: StreamElementsOutputBase(id, name, RecordingOutput, Recording,
					   videoComposition, audioComposition,
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
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	virtual std::vector<uint32_t> GetAudioTracks() override;
};

class StreamElementsCustomRecordingOutput : public StreamElementsOutputBase {
private:
	std::shared_mutex m_mutex;

	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo = nullptr;

	obs_output_t *m_output = nullptr;

	CefRefPtr<CefDictionaryValue> m_recordingSettings = CefDictionaryValue::Create();

	std::vector<uint32_t> m_audioTracks;

public:
	StreamElementsCustomRecordingOutput(
		std::string id, std::string name,
		CefRefPtr<CefDictionaryValue> recordingSettings,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition,
		std::vector<uint32_t> audioTracks,
		CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, RecordingOutput, None,
					   videoComposition, audioComposition,
					   auxData),
		  m_recordingSettings(recordingSettings),
		  m_audioTracks(audioTracks)
	{
	}

	virtual ~StreamElementsCustomRecordingOutput()
	{
		Stop();
	}

	virtual bool CanDisable() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	virtual std::vector<uint32_t> GetAudioTracks() override
	{
		return m_audioTracks;
	}

	static std::shared_ptr<StreamElementsCustomRecordingOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

	virtual bool CanSplitRecordingOutput();
	virtual bool TriggerSplitRecordingOutput();

protected:
	virtual obs_output_t *GetOutput() override
	{
		return m_output ? obs_output_get_ref(m_output) : nullptr;
	}

	virtual bool
	StartInternal(std::shared_ptr<
		      StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo);
	virtual void StopInternal();
};
