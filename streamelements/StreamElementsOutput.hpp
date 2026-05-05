#pragma once

#include <obs-frontend-api.h>
#include "StreamElementsVideoComposition.hpp"
#include "StreamElementsAudioComposition.hpp"

#include <shared_mutex>


enum ObsOutputType { StreamingOutput, RecordingOutput, ReplayBufferOutput };

class SEVideoCompositionVideoEncoderProvider
	: public SELazyObjectProviderBase<obs_encoder_t> {
private:
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionBase;

	size_t m_index;
	CefRefPtr<CefValue> m_obsEncoderInfo;
	ObsOutputType m_outputType;

public:
	SEVideoCompositionVideoEncoderProvider(
		std::shared_ptr<
			StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionBase,
		size_t index,
		CefRefPtr<CefValue> obsEncoderInfo, ObsOutputType outputType)
	{
		m_index = index;
		m_videoCompositionBase = videoCompositionBase;
		m_obsEncoderInfo = obsEncoderInfo;
		m_outputType = outputType;
	}

	~SEVideoCompositionVideoEncoderProvider() {}

protected:
	virtual obs_encoder_t *AllocRef() override
	{
		obs_encoder_t *result = nullptr;

		if (m_obsEncoderInfo.get()) {
			result = DeserializeObsVideoEncoder(
				m_obsEncoderInfo);

			if (result) {
				switch (video_output_get_format(
					obs_get_video())) {
				case VIDEO_FORMAT_I420:
				case VIDEO_FORMAT_NV12:
				case VIDEO_FORMAT_I010:
				case VIDEO_FORMAT_P010:
					break;
				default:
					obs_encoder_set_preferred_video_format(
						result,
						VIDEO_FORMAT_NV12);
				}

				uint32_t width, height;
				m_videoCompositionBase->GetVideoBaseDimensions(
					&width, &height);

				//
				// This will prevent obs_encoder_get_width & obs_encoder_get_height from crashing due to video output being improperly initialized for SOME REASON
				// https://app.bugsplat.com/v2/crash?database=OBS_Live&id=1488897
				//
				obs_encoder_set_scaled_size(result,
							    width, height);

				obs_encoder_set_video(
					result, m_videoCompositionBase
							->GetVideo());
			}
		}

		std::shared_ptr<SELazyObjectReference<obs_encoder_t>> ref =
			nullptr;

		if (m_outputType != StreamingOutput)
		{
			ref = m_videoCompositionBase
				      ->GetRecordingVideoEncoderRef(m_index);

		}

		if (!ref) {
			ref = m_videoCompositionBase
				      ->GetStreamingVideoEncoderRef(m_index);
		}

		if (ref) {
			result = SETRACE_ADDREF(
				obs_encoder_get_ref(ref->Get()));
		}

		return result;
	}

	virtual obs_encoder_t *AddRef(obs_encoder_t *encoder) override
	{
		return SETRACE_ADDREF(obs_encoder_get_ref(encoder));
	}

	virtual void ReleaseRef(obs_encoder_t *encoder) override
	{
		return obs_encoder_release(SETRACE_DECREF(encoder));
	}
};

class StreamElementsOutputBase
	: public StreamElementsVideoCompositionEventListener,
	  public StreamElementsAudioCompositionEventListener {
public:
	enum ObsStateDependencyType {
		Streaming,
		Recording,
		ReplayBuffer,
		None
	};

	class VideoEncoderTemplate {
	public:
		size_t m_index;
		CefRefPtr<CefValue> m_obsEncoderInfo;

	public:
		VideoEncoderTemplate(size_t index)
			: m_index(index), m_obsEncoderInfo(CefValue::Create())
		{
			m_obsEncoderInfo->SetNull();
		}

		VideoEncoderTemplate(CefRefPtr<CefValue> obsEncoderInfo)
			: m_index(0), m_obsEncoderInfo(obsEncoderInfo->Copy())
		{
		}

		VideoEncoderTemplate(VideoEncoderTemplate &other)
			: m_index(other.m_index), m_obsEncoderInfo(other.m_obsEncoderInfo->Copy())
		{
		}

		~VideoEncoderTemplate() {}

		bool IsMatchingIndex(size_t index) {
			if (m_obsEncoderInfo.get() &&
			    m_obsEncoderInfo->GetType() == VTYPE_DICTIONARY)
				return false;

			return m_index = index;
		}

		CefRefPtr<CefValue> Serialize() {
			CefRefPtr<CefValue> result = CefValue::Create();

			if (m_obsEncoderInfo.get() &&
			    m_obsEncoderInfo->GetType() == VTYPE_DICTIONARY) {
				result->SetValue(m_obsEncoderInfo->Copy());
			}
			else
				result->SetInt(m_index);

			return result;
		}

	public:
		std::shared_ptr<SELazyObjectProviderBase<obs_encoder_t>>
		CreateStreamingEncoderProvider(
			std::shared_ptr<StreamElementsVideoCompositionBase::
						CompositionInfo> videoCompositionBase)
		{
			return std::make_shared<SEVideoCompositionVideoEncoderProvider>(
				videoCompositionBase, m_index, m_obsEncoderInfo,
				StreamingOutput);
		}

		std::shared_ptr<SELazyObjectProviderBase<obs_encoder_t>>
		CreateRecordingEncoderProvider(
			std::shared_ptr<StreamElementsVideoCompositionBase::
						CompositionInfo>
				videoCompositionBase)
		{
			return std::make_shared<
				SEVideoCompositionVideoEncoderProvider>(
				videoCompositionBase, m_index, m_obsEncoderInfo,
				RecordingOutput);
		}
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
	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>> GetVideoEncoders() = 0;

	virtual bool CanSplitRecordingOutput() { return false; }
	virtual bool TriggerSplitRecordingOutput() { return false; }

	virtual bool CanSaveReplayBuffer() { return false; }
	virtual bool TriggerSaveReplayBuffer() { return false; }

	virtual void ConnectOutputEvents();
	virtual void DisconnectOutputEvents();

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
	std::vector<std::shared_ptr<VideoEncoderTemplate>> m_videoEncoderTemplates = {
		std::make_shared<VideoEncoderTemplate>(0)};

	std::vector<std::shared_ptr<SELazyObjectProviderBase<obs_encoder_t>>>
		m_videoEncoderProviders;
	std::vector<std::shared_ptr<SELazyObjectReference<obs_encoder_t>>> m_videoEncoders;


public:
	StreamElementsCustomStreamingOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition,
		std::vector<uint32_t> audioTracks,
		std::vector<std::shared_ptr<VideoEncoderTemplate>> videoEncoders, obs_service_t *service,
		const char *bindToIP, CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, StreamingOutput, Streaming,
					   videoComposition, audioComposition,
					   auxData),
		  m_service(service),
		  m_audioTracks(audioTracks),
		  m_videoEncoderTemplates(videoEncoders)
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

		obs_service_release(SETRACE_DECREF(m_service));
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

	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>> GetVideoEncoders() override
	{
		return m_videoEncoderTemplates;
	}

	static std::shared_ptr<StreamElementsCustomStreamingOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

protected:
	virtual obs_output_t *GetOutput() override
	{
		return m_output ? SETRACE_ADDREF(obs_output_get_ref(m_output)) : nullptr;
	}

	virtual bool StartInternal(
		std::shared_ptr<
			StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) override;
	virtual void StopInternal() override;
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
	virtual obs_output_t *GetOutput() override { return SETRACE_ADDREF(obs_frontend_get_streaming_output()); }

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
	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>> GetVideoEncoders() override;
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
		return SETRACE_ADDREF(obs_frontend_get_recording_output());
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
	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>>
	GetVideoEncoders() override;
};

class StreamElementsCustomRecordingOutput : public StreamElementsOutputBase {
private:
	std::shared_mutex m_mutex;

	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo = nullptr;

	obs_output_t *m_output = nullptr;

	CefRefPtr<CefDictionaryValue> m_recordingSettings = CefDictionaryValue::Create();

	std::vector<uint32_t> m_audioTracks;
	std::vector<std::shared_ptr<VideoEncoderTemplate>> m_videoEncoderTemplates = {
		std::make_shared<VideoEncoderTemplate>(0)};

	std::vector<std::shared_ptr<SELazyObjectProviderBase<obs_encoder_t>>>
		m_videoEncoderProviders;
	std::vector<std::shared_ptr<SELazyObjectReference<obs_encoder_t>>>
		m_videoEncoders;

public:
	StreamElementsCustomRecordingOutput(
		std::string id, std::string name,
		CefRefPtr<CefDictionaryValue> recordingSettings,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition,
		std::vector<uint32_t> audioTracks,
		std::vector<std::shared_ptr<VideoEncoderTemplate>> videoEncoders,
		CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, RecordingOutput, None,
					   videoComposition, audioComposition,
					   auxData),
		  m_recordingSettings(recordingSettings),
		  m_audioTracks(audioTracks),
		  m_videoEncoderTemplates(videoEncoders)
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

	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>> GetVideoEncoders() override
	{
		return m_videoEncoderTemplates;
	}

	static std::shared_ptr<StreamElementsCustomRecordingOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

	virtual bool CanSplitRecordingOutput() override;
	virtual bool TriggerSplitRecordingOutput() override;

protected:
	virtual obs_output_t *GetOutput() override
	{
		return m_output ? SETRACE_ADDREF(obs_output_get_ref(m_output)) : nullptr;
	}

	virtual bool
	StartInternal(std::shared_ptr<
		      StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) override;
	virtual void StopInternal() override;
};

class StreamElementsObsNativeReplayBufferOutput : public StreamElementsOutputBase {
public:
	StreamElementsObsNativeReplayBufferOutput(
		std::string id, std::string name,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition)
		: StreamElementsOutputBase(id, name, ReplayBufferOutput,
					   ReplayBuffer,
					   videoComposition, audioComposition,
					   CefDictionaryValue::Create())
	{
	}

	virtual ~StreamElementsObsNativeReplayBufferOutput() {
		Stop();
	}

	virtual bool CanRemove() override { return false; }
	virtual bool CanChange() override { return false; }
	virtual bool CanDisable() override { return false; }

	virtual bool IsObsNativeOutput() override { return true; }

	virtual bool CanStart() override { return true; }

	virtual bool CanSaveReplayBuffer() override;
	virtual bool TriggerSaveReplayBuffer() override;

protected:
	virtual obs_output_t *GetOutput() override
	{
		return SETRACE_ADDREF(obs_frontend_get_replay_buffer_output());
	}

	virtual bool StartInternal(
		std::shared_ptr<
			StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) override;
	virtual void StopInternal() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	virtual std::vector<uint32_t> GetAudioTracks() override;
	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>>
	GetVideoEncoders() override;
};

class StreamElementsCustomReplayBufferOutput : public StreamElementsOutputBase {
private:
	std::shared_mutex m_mutex;

	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		m_videoCompositionInfo = nullptr;

	obs_output_t *m_output = nullptr;

	CefRefPtr<CefDictionaryValue> m_recordingSettings =
		CefDictionaryValue::Create();

	std::vector<uint32_t> m_audioTracks = {0};
	std::vector<std::shared_ptr<VideoEncoderTemplate>> m_videoEncoderTemplates = {
		std::make_shared<VideoEncoderTemplate>(0)};

	std::vector<std::shared_ptr<SELazyObjectProviderBase<obs_encoder_t>>>
		m_videoEncoderProviders;
	std::vector<std::shared_ptr<SELazyObjectReference<obs_encoder_t>>>
		m_videoEncoders;

public:
	StreamElementsCustomReplayBufferOutput(
		std::string id, std::string name,
		CefRefPtr<CefDictionaryValue> recordingSettings,
		std::shared_ptr<StreamElementsVideoCompositionBase>
			videoComposition,
		std::shared_ptr<StreamElementsAudioCompositionBase>
			audioComposition,
		std::vector<uint32_t> audioTracks,
		std::vector<std::shared_ptr<VideoEncoderTemplate>> videoEncoders,
		CefRefPtr<CefDictionaryValue> auxData)
		: StreamElementsOutputBase(id, name, ReplayBufferOutput, Streaming,
					   videoComposition, audioComposition,
					   auxData),
		  m_recordingSettings(recordingSettings),
		  m_audioTracks(audioTracks),
		  m_videoEncoderTemplates(videoEncoders)
	{
	}

	virtual ~StreamElementsCustomReplayBufferOutput() {
		Stop();
	}

	virtual bool CanDisable() override;

	virtual void
	SerializeOutputSettings(CefRefPtr<CefValue> &output) override;

	virtual std::vector<uint32_t> GetAudioTracks() override
	{
		return m_audioTracks;
	}

	virtual std::vector<std::shared_ptr<VideoEncoderTemplate>> GetVideoEncoders() override
	{
		return m_videoEncoderTemplates;
	}

	static std::shared_ptr<StreamElementsCustomReplayBufferOutput>
	Create(CefRefPtr<CefValue> input);

	virtual bool IsObsNativeOutput() override { return false; }

protected:
	virtual obs_output_t *GetOutput() override
	{
		return m_output ? SETRACE_ADDREF(obs_output_get_ref(m_output)) : nullptr;
	}

	virtual bool StartInternal(
		std::shared_ptr<
			StreamElementsVideoCompositionBase::CompositionInfo>
			videoCompositionInfo,
		std::shared_ptr<
			StreamElementsAudioCompositionBase::CompositionInfo>
			audioCompositionInfo) override;
	virtual void StopInternal() override;

	virtual void ConnectOutputEvents() override;
	virtual void DisconnectOutputEvents() override;

	virtual bool CanSaveReplayBuffer() override { return IsActive(); }
	virtual bool TriggerSaveReplayBuffer() override;

private:
	static void handle_output_saved(void *my_data, calldata_t *cd);
};
