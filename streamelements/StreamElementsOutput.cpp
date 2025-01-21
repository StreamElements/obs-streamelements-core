#include <obs.hpp>
#include <obs-frontend-api.h>

#include "StreamElementsUtils.hpp"
#include "StreamElementsOutput.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <ctime>

static std::string safe_string(const char* input)
{
	if (!input)
		return "";

	return std::string(input);
}

static std::vector<uint32_t> DeserializeAudioTracks(CefRefPtr<CefDictionaryValue> rootDict)
{
	std::vector<uint32_t> result;

	if (rootDict->HasKey("audioTracks") &&
	    rootDict->GetType("audioTracks") == VTYPE_LIST) {
		auto list = rootDict->GetList("audioTracks");

		for (size_t i = 0; i < list->GetSize(); ++i) {
			if (list->GetType(i) != VTYPE_INT)
				continue;

			auto trackIndex = list->GetInt(i);

			if (trackIndex >= 0 && trackIndex < MAX_AUDIO_MIXES) {
				bool hasValue = false;

				for (auto it = result.cbegin();
				     it != result.cend(); ++it) {
					if (*it == trackIndex) {
						hasValue = true;
						break;
					}
				}

				if (!hasValue) {
					result.push_back(trackIndex);
				}
			}
		}
	}

	if (!result.size()) {
		result.push_back(0);
	}

	return result;
}

static void dispatch_list_change_event(StreamElementsOutputBase *output)
{
	if (output->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		DispatchClientJSEvent("hostStreamingOutputListChanged", "null");
	else if (output->GetOutputType() == StreamElementsOutputBase::RecordingOutput)
		DispatchClientJSEvent("hostRecordingOutputListChanged", "null");
	else
		DispatchClientJSEvent("hostReplayBufferOutputListChanged", "null");
}

static void dispatch_event(
	StreamElementsOutputBase *output, std::string eventName,
	CefRefPtr<CefDictionaryValue> args = CefDictionaryValue::Create())
{
	args->SetString("outputId", output->GetId());

	auto value = CefValue::Create();
	value->SetDictionary(args);

	std::string json = CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();

	DispatchClientJSEvent(eventName, json);
}

void StreamElementsOutputBase::handle_output_start(void *my_data,
							  calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStarted");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputStarted");
	else
		dispatch_event(self, "hostReplayBufferOutputStarted");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_stop(void *my_data,
							 calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	int code = calldata_int(cd, "code");

	if (code != OBS_OUTPUT_SUCCESS) {
		auto args = CefDictionaryValue::Create();

		switch (code) {
		case OBS_OUTPUT_SUCCESS:
			args->SetString("reason", "Successfully stopped");
			break;
		case OBS_OUTPUT_BAD_PATH:
			args->SetString("reason",
					"The specified path was invalid");
			break;
		case OBS_OUTPUT_CONNECT_FAILED:
			args->SetString("reason",
					"Failed to connect to a server");
			break;
		case OBS_OUTPUT_INVALID_STREAM:
			args->SetString("reason", "Invalid stream path");
			break;
		case OBS_OUTPUT_ERROR:
			args->SetString("reason", "Generic error");
			break;
		case OBS_OUTPUT_DISCONNECTED:
			args->SetString("reason", "Unexpectedly disconnected");
			break;
		case OBS_OUTPUT_UNSUPPORTED:
			args->SetString(
				"reason",
				"The settings, video/audio format, or codecs are unsupported by this output");
			break;
		case OBS_OUTPUT_NO_SPACE:
			args->SetString("reason", "Ran out of disk space");
			break;
		case OBS_OUTPUT_ENCODE_ERROR:
			args->SetString("reason", "Encoder error");
			break;

		default:
			char buffer[32];
			std::string reason = "Unknown reason code ";
			reason += itoa(code, buffer, 10);

			args->SetString("reason", reason);
			break;
		}

		self->SetError(args->GetString("reason"));

		if (self->GetOutputType() ==
		    StreamElementsOutputBase::StreamingOutput)
			dispatch_event(self, "hostStreamingOutputError", args);
		else if (self->GetOutputType() ==
			 StreamElementsOutputBase::RecordingOutput)
			dispatch_event(self, "hostRecordingOutputError", args);
		else
			dispatch_event(self, "hostReplayBufferOutputError", args);
	}

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStopped");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputStopped");
	else
		dispatch_event(self, "hostReplayBufferOutputStopped");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_pause(void *my_data,
							  calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputPaused");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputPaused");
	else
		dispatch_event(self, "hostReplayBufferOutputPaused");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_unpause(void *my_data,
							    calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputUnpaused");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputUnpaused");
	else
		dispatch_event(self, "hostReplayBufferOutputUnpaused");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_starting(void *my_data,
							     calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStarting");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputStarting");
	else
		dispatch_event(self, "hostReplayBufferOutputStarting");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_stopping(void *my_data,
							     calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStopping");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputStopping");
	else
		dispatch_event(self, "hostReplayBufferOutputStopping");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_activate(void *my_data,
							     calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputActivated");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputActivated");
	else
		dispatch_event(self, "hostReplayBufferOutputActivated");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_deactivate(void *my_data,
							       calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputDeactivated");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputDeactivated");
	else
		dispatch_event(self, "hostReplayBufferOutputDeactivated");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_reconnect(void *my_data,
							      calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputReconnecting");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputReconnecting");
	else
		dispatch_event(self, "hostReplayBufferOutputReconnecting");

	dispatch_list_change_event(self);
}

void
StreamElementsOutputBase::handle_output_reconnect_success(void *my_data,
							  calldata_t *cd)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputReconnected");
	else if (self->GetOutputType() ==
		 StreamElementsOutputBase::RecordingOutput)
		dispatch_event(self, "hostRecordingOutputReconnected");
	else
		dispatch_event(self, "hostReplayBufferOutputReconnected");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_obs_frontend_event(
	enum obs_frontend_event event,
	void* data)
{
	SEAsyncCallContextMarker asyncMarker(__FILE__, __LINE__);

	StreamElementsOutputBase *self = (StreamElementsOutputBase *)data;

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		//
		// OBS native output video encoder is already set up at this stage,
		// so we can start our outputs as well.
		//
		// State is important, just in case that we are pulling from the native
		// OBS video composition: in this case we want to re-use the encoder
		// which has already been set-up by OBS itself.
		//
		// This is *especially* important for audio: at this moment, we are
		// re-using the OBS native audio encoder for *all* our outputs -
		// there is no API to mix a different audio stream, or to apply
		// different encoding to the existing mix.
		//
		self->m_isObsStreaming = true;

		if (self->m_obsStateDependency == Streaming)
			self->Start();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
		self->m_isObsStreaming = false;

		if (self->m_obsStateDependency == Streaming)
			self->Stop();
		break;


	case OBS_FRONTEND_EVENT_RECORDING_STARTING:
		//
		// OBS native output video encoder is already set up at this stage,
		// so we can start our outputs as well.
		//
		// State is important, just in case that we are pulling from the native
		// OBS video composition: in this case we want to re-use the encoder
		// which has already been set-up by OBS itself.
		//
		// This is *especially* important for audio: at this moment, we are
		// re-using the OBS native audio encoder for *all* our outputs -
		// there is no API to mix a different audio stream, or to apply
		// different encoding to the existing mix.
		//
		if (self->m_obsStateDependency == Recording)
			self->Start();
		break;

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STARTING:
		if (self->m_obsStateDependency == ReplayBuffer)
			self->Start();
		break;

	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		if (self->m_obsStateDependency == Recording)
			self->Stop();
		break;

	case OBS_FRONTEND_EVENT_REPLAY_BUFFER_STOPPED:
		if (self->m_obsStateDependency == ReplayBuffer)
			self->Stop();
		break;

	case OBS_FRONTEND_EVENT_EXIT:
		self->Stop();
		break;
	default:
		return;
	}
}

void StreamElementsOutputBase::SerializeOutput(CefRefPtr<CefValue>& output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	auto d = CefDictionaryValue::Create();

	d->SetString("id", GetId());
	d->SetString("name", GetName());
	d->SetString("videoCompositionId", m_videoComposition->GetId());
	d->SetString("audioCompositionId", m_audioComposition->GetId());
	d->SetBool("isEnabled", IsEnabled());
	d->SetBool("isActive", IsActive());
	d->SetBool("canDisable", CanDisable());
	d->SetBool("canRemove", !IsObsNativeOutput());
	d->SetBool("isObsNativeVideoComposition",
		   m_videoComposition->IsObsNativeComposition());
	d->SetBool("isObsNativeAudioComposition",
		   m_audioComposition->IsObsNativeComposition());
	d->SetBool("isObsNativeOutput", IsObsNativeOutput());

	if (m_error.size()) {
		auto error = CefDictionaryValue::Create();

		error->SetString("message", m_error);

		d->SetDictionary("error", error);
	} else {
		d->SetNull("error");
	}

	OBSOutputAutoRelease obs_output = GetOutput();

	if (obs_output && IsActive()) {
		auto stats = CefDictionaryValue::Create();

		stats->SetBool("canPause", obs_output_can_pause(obs_output));
		stats->SetBool("isReconnecting",
			   obs_output_reconnecting(obs_output));
		stats->SetInt("width", obs_output_get_width(obs_output));
		stats->SetInt("height", obs_output_get_height(obs_output));

		stats->SetInt("framesDropped",
			   obs_output_get_frames_dropped(obs_output));

		stats->SetDouble("congestion",
				 obs_output_get_congestion(obs_output));

		auto last_error = obs_output_get_last_error(obs_output);

		if (last_error)
			stats->SetString("lastErrorMessage", last_error);
		else
			stats->SetNull("lastErrorMessage");

		stats->SetInt("totalBytes",
			      obs_output_get_total_bytes(obs_output));
		stats->SetInt("totalFrames",
			  obs_output_get_total_frames(obs_output));

		stats->SetBool("isPaused", obs_output_paused(obs_output));

		d->SetDictionary("stats", stats);
	} else {
		d->SetNull("stats");
	}

	d->SetDictionary("auxiliaryData", m_auxData);

	auto audioTracks = CefListValue::Create();
	for (auto audioTrackIndex : GetAudioTracks()) {
		audioTracks->SetInt(audioTracks->GetSize(), audioTrackIndex);
	}
	d->SetList("audioTracks", audioTracks);

	auto outputSettings = CefValue::Create();

	SerializeOutputSettings(outputSettings);

	switch (m_obsOutputType) {
	case StreamingOutput:
		d->SetString("type", "streaming");
		d->SetValue("streamingSettings", outputSettings);
		break;
	case RecordingOutput:
		d->SetString("type", "recording");
		d->SetValue("recordingSettings", outputSettings);
		break;
	case ReplayBufferOutput:
		d->SetString("type", "replayBuffer");
		d->SetValue("recordingSettings", outputSettings);
		break;
	default:
		throw std::invalid_argument("Unknown obsOutputType value");
	}

	output->SetDictionary(d);
}

StreamElementsOutputBase::StreamElementsOutputBase(
	std::string id, std::string name, ObsOutputType obsOutputType,
	ObsStateDependencyType obsStateDependency,
	std::shared_ptr<StreamElementsVideoCompositionBase> videoComposition,
	std::shared_ptr<StreamElementsAudioCompositionBase> audioComposition,
	CefRefPtr<CefDictionaryValue> auxData)
	: m_id(id),
	  m_name(name),
	  m_obsOutputType(obsOutputType),
	  m_obsStateDependency(obsStateDependency),
	  m_videoComposition(videoComposition),
	  m_audioComposition(audioComposition),
	  m_enabled(false)
{
	m_auxData = auxData.get() ? auxData : CefDictionaryValue::Create();

	m_videoCompositionInfo = videoComposition->GetCompositionInfo(this, std::string("StreamElementsOutputBase(") + m_id + std::string(")"));
	m_audioCompositionInfo = audioComposition->GetCompositionInfo(this);

	m_enabled = IsObsNativeOutput();
	m_isObsStreaming = obs_frontend_streaming_active();

	dispatch_list_change_event(this);

	obs_frontend_add_event_callback(
		StreamElementsOutputBase::handle_obs_frontend_event, this);
}

StreamElementsOutputBase::~StreamElementsOutputBase()
{
	obs_frontend_remove_event_callback(
		StreamElementsOutputBase::handle_obs_frontend_event, this);

	dispatch_list_change_event(this);
}

bool StreamElementsOutputBase::IsEnabled()
{
	return m_enabled;
}

bool StreamElementsOutputBase::IsActive()
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	OBSOutputAutoRelease output = GetOutput();

	if (!output)
		return false;

	return obs_output_active(output);
}

void StreamElementsOutputBase::SetEnabled(bool enabled)
{
	if (!enabled && !CanDisable())
		return;

	{
		std::unique_lock<decltype(m_mutex)> lock(m_mutex);

		if (enabled == m_enabled)
			return;

		m_enabled = enabled;
	}

	if (enabled) {
		Start();
	} else {
		Stop();
	}
}

bool StreamElementsOutputBase::CanStart()
{
	if (!CanDisable()) {
		return false;
	}

	if (!m_videoCompositionInfo)
		return false;

	if (!IsEnabled())
		return false;

	if (IsActive())
		return false;

	return true;
}

bool StreamElementsOutputBase::Start()
{
	SetError("");

	if (!CanStart())
		return false;

	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	return StartInternal(m_videoCompositionInfo, m_audioCompositionInfo);
}

void StreamElementsOutputBase::ConnectOutputEvents()
{
	if (m_outputEventsConnected)
		return;

	OBSOutputAutoRelease output = GetOutput();

	if (!output)
		return;

	auto handler = obs_output_get_signal_handler(output);

	signal_handler_connect(handler, "start", handle_output_start, this);
	signal_handler_connect(handler, "stop", handle_output_stop, this);
	signal_handler_connect(handler, "pause", handle_output_pause, this);
	signal_handler_connect(handler, "unpause", handle_output_unpause, this);
	signal_handler_connect(handler, "starting", handle_output_starting,
			       this);
	signal_handler_connect(handler, "stopping", handle_output_stopping,
			       this);
	signal_handler_connect(handler, "activate", handle_output_activate,
			       this);
	signal_handler_connect(handler, "deactivate", handle_output_deactivate,
			       this);
	signal_handler_connect(handler, "reconnect", handle_output_reconnect,
			       this);
	signal_handler_connect(handler, "reconnect_success",
			       handle_output_reconnect_success, this);

	m_outputEventsConnected = true;
}

void StreamElementsOutputBase::DisconnectOutputEvents()
{
	if (!m_outputEventsConnected)
		return;

	OBSOutputAutoRelease output = GetOutput();

	if (!output)
		return;

	auto handler = obs_output_get_signal_handler(output);

	signal_handler_disconnect(handler, "start", handle_output_start, this);
	signal_handler_disconnect(handler, "stop", handle_output_stop, this);
	signal_handler_disconnect(handler, "pause", handle_output_pause, this);
	signal_handler_disconnect(handler, "unpause", handle_output_unpause,
				  this);
	signal_handler_disconnect(handler, "starting", handle_output_starting,
				  this);
	signal_handler_disconnect(handler, "stopping", handle_output_stopping,
				  this);
	signal_handler_disconnect(handler, "activate", handle_output_activate,
				  this);
	signal_handler_disconnect(handler, "deactivate",
				  handle_output_deactivate, this);
	signal_handler_disconnect(handler, "reconnect", handle_output_reconnect,
				  this);
	signal_handler_disconnect(handler, "reconnect_success",
				  handle_output_reconnect_success, this);

	m_outputEventsConnected = false;
}

void StreamElementsOutputBase::Stop()
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	StopInternal();

	SetError("");
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsCustomStreamingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsCustomStreamingOutput::CanDisable()
{
	return true;
}

bool StreamElementsCustomStreamingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo,
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		audioCompositionInfo)
{
	if (!videoCompositionInfo)
		return false;

	OBSEncoderAutoRelease streamingVideoEncoder =
		videoCompositionInfo->GetStreamingVideoEncoderRef();

	if (!streamingVideoEncoder && videoCompositionInfo->IsObsNative()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: OBS Native streaming video encoder does not exist yet on streaming output '%s'",
		     GetId().c_str());

		SetError(
			"OBS Native streaming video encoder does not exist yet");

		auto args = CefDictionaryValue::Create();
		args->SetString("reason", "OBS Native streaming video encoder does not exist yet");

		dispatch_event(this, "hostStreamingOutputError", args);
		dispatch_event(this, "hostStreamingOutputStopped");
		dispatch_list_change_event(this);

		return false;
	}

	const char *output_type =
		obs_service_get_preferred_output_type(m_service);

	if (!output_type)
		output_type = "rtmp_output";

	obs_data_t *output_settings = obs_data_create();

	if (m_bindToIP.size()) {
		obs_data_set_string(output_settings, "bind_ip", m_bindToIP.c_str());
	}

	m_output = obs_output_create(output_type, GetId().c_str(),
						 output_settings, nullptr);

	obs_data_release(output_settings);

	if (m_output) {
		OBSEncoderAutoRelease streamingVideoEncoder =
			videoCompositionInfo->GetStreamingVideoEncoderRef();

		obs_output_set_video_encoder(
			m_output, streamingVideoEncoder);

		size_t audioEncodersCount = 0;

		for (size_t i = 0; i < m_audioTracks.size(); ++i) {
			OBSEncoderAutoRelease encoder =
				audioCompositionInfo
					->GetStreamingAudioEncoderRef(
						m_audioTracks[i]);

			if (!encoder) {
				blog(LOG_WARNING,
				     "obs-streamelements-core: audio encoder for audio track %d does not exist on streaming output '%s'",
				     m_audioTracks[i], GetId().c_str());

				break;
			}

			obs_output_set_audio_encoder(m_output, encoder, i);

			++audioEncodersCount;
		}

		if (audioEncodersCount) {
			obs_output_set_service(m_output, m_service);

			ConnectOutputEvents();

			if (obs_output_start(m_output)) {
				m_videoCompositionInfo = videoCompositionInfo;

				return true;
			} else {
				SetErrorIfEmpty("Failed to start output");
			}
		} else {
			blog(LOG_ERROR,
			     "obs-streamelements-core: no audio encoders were set up on streaming output '%s'",
			     GetId().c_str());

			SetError(
				"No audio encoders were set up for requested audio tracks");
		}

		DisconnectOutputEvents();

		obs_output_release(m_output);
		m_output = nullptr;
	} else {
		blog(LOG_ERROR,
		     "obs-streamelements-core: failed to create streaming output '%s' of type '%s'",
		     GetId().c_str(), output_type);

		SetError(
			"Failed to create output");
	}

	return false;
}

void StreamElementsCustomStreamingOutput::StopInternal()
{
	if (!m_videoCompositionInfo)
		return;

	// obs_output_stop(m_output);

	obs_output_force_stop(m_output);

	DisconnectOutputEvents();

	obs_output_release(m_output);
	m_output = nullptr;
}

void StreamElementsCustomStreamingOutput::SerializeOutputSettings(
	CefRefPtr<CefValue>& output)
{
	auto d = CefDictionaryValue::Create();

	obs_data_t *service_settings = obs_service_get_settings(m_service);

	d->SetString("type", obs_service_get_type(m_service));

	d->SetString("serverUrl",
		     safe_string(obs_data_get_string(service_settings, "server")));

	d->SetString("streamKey", safe_string(obs_data_get_string(service_settings, "key")));

	bool useAuth = obs_data_get_bool(service_settings, "use_auth");

	d->SetBool("useAuth", useAuth);

	if (useAuth) {
		d->SetString("authUsername",
			     safe_string(obs_data_get_string(service_settings,
							    "username")));
		d->SetString("authPassword",
			     safe_string(obs_data_get_string(service_settings,
							    "password")));
	}

	obs_data_release(service_settings);

	output->SetDictionary(d);
}

std::shared_ptr <StreamElementsCustomStreamingOutput> StreamElementsCustomStreamingOutput::Create(CefRefPtr<CefValue> input)
{
	if (!input.get())
		return nullptr;

	if (input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	auto rootDict = input->GetDictionary();

	if (!rootDict->HasKey("id") || !rootDict->HasKey("name"))
		return nullptr;

	if (rootDict->GetType("id") != VTYPE_STRING ||
	    rootDict->GetType("name") != VTYPE_STRING)
		return nullptr;

	if (!rootDict->HasKey("streamingSettings") || rootDict->GetType("streamingSettings") != VTYPE_DICTIONARY)
		return nullptr;

	auto auxData = (rootDict->HasKey("auxiliaryData") && rootDict->GetType("auxiliaryData") == VTYPE_DICTIONARY)
			       ? rootDict->GetDictionary("auxiliaryData")
			       : CefDictionaryValue::Create();

	std::string id = rootDict->GetString("id");
	std::string name = rootDict->GetString("name");
	std::string videoCompositionId =
		(rootDict->HasKey("videoCompositionId") && rootDict->GetType("videoCompositionId") ==
					       VTYPE_STRING
			? rootDict->GetString("videoCompositionId")
			: "");
	std::string audioCompositionId =
		(rootDict->HasKey("audioCompositionId") &&
				 rootDict->GetType("audioCompositionId") ==
					 VTYPE_STRING
			 ? rootDict->GetString("audioCompositionId")
			 : "");
	bool isEnabled = rootDict->HasKey("isEnabled") &&
					 rootDict->GetType("isEnabled") ==
						 VTYPE_BOOL
				 ? rootDict->GetBool("isEnabled")
				 : true;

	auto d = rootDict->GetDictionary("streamingSettings");

	if (!d.get() || !d->HasKey("serverUrl") || !d->HasKey("streamKey")) {
		return nullptr;
	}

	std::string serverUrl = d->GetString("serverUrl");
	std::string streamKey = d->GetString("streamKey");
	bool useAuth = d->HasKey("useAuth") && d->GetBool("useAuth");
	std::string authUsername = (useAuth && d->HasKey("authUsername"))
					   ? d->GetString("authUsername")
					   : "";
	std::string authPassword = (useAuth && d->HasKey("authPassword"))
					   ? d->GetString("authPassword")
					   : "";

	auto videoComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager()
			->GetVideoCompositionById(videoCompositionId);

	auto audioComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetAudioCompositionManager()
			->GetAudioCompositionById(audioCompositionId);

	if (!videoComposition.get() && !videoCompositionId.size()) {
		videoComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetObsNativeVideoComposition();
	}

	if (!audioComposition.get() && !audioCompositionId.size()) {
		audioComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetAudioCompositionManager()
				->GetObsNativeAudioComposition();
	}

	if (!videoComposition) {
		return nullptr;
	}

	if (!audioComposition) {
		return nullptr;
	}

	// Streaming service
	obs_service_t *service = obs_service_create(
		"rtmp_custom", "default_service", NULL, NULL);
	obs_data_t *service_settings = obs_service_get_settings(service);

	obs_data_set_string(service_settings, "server", serverUrl.c_str());
	obs_data_set_string(service_settings, "key", streamKey.c_str());

	obs_data_set_bool(service_settings, "use_auth", useAuth);
	if (useAuth) {
		obs_data_set_string(service_settings, "username",
				    authUsername.c_str());
		obs_data_set_string(service_settings, "password",
				    authPassword.c_str());
	}

	obs_service_update(service, service_settings);

	obs_data_release(service_settings);

	auto bindToIP = "";

	auto audioTracks = DeserializeAudioTracks(rootDict);

	auto result = std::make_shared<StreamElementsCustomStreamingOutput>(
		id, name, videoComposition, audioComposition, audioTracks,
		service, bindToIP, auxData);

	result->SetEnabled(isEnabled);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeStreamingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeStreamingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo,
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		audioCompositionInfo)
{
	// NOP: This is managed by the OBS front-end

	ConnectOutputEvents();

	return true;
}

void StreamElementsObsNativeStreamingOutput::StopInternal()
{
	// NOP: This is managed by the OBS front-end

	DisconnectOutputEvents();
}

void StreamElementsObsNativeStreamingOutput::SerializeOutputSettings(
	CefRefPtr<CefValue> &output)
{
	auto d = CefDictionaryValue::Create();

	obs_service_t *service = obs_frontend_get_streaming_service(); // No refcount increment

	obs_data_t *service_settings = obs_service_get_settings(service);

	d->SetString("type", obs_service_get_type(service));

	d->SetString("serverUrl", safe_string(obs_data_get_string(
					  service_settings, "server")));

	d->SetString("streamKey",
		     safe_string(obs_data_get_string(service_settings, "key")));

	bool useAuth = obs_data_get_bool(service_settings, "use_auth");

	d->SetBool("useAuth", useAuth);

	if (useAuth) {
		d->SetString("authUsername",
			     safe_string(obs_data_get_string(service_settings,
							     "username")));
		d->SetString("authPassword",
			     safe_string(obs_data_get_string(service_settings,
							     "password")));
	}

	obs_data_release(service_settings);

	output->SetDictionary(d);
}

std::vector<uint32_t> StreamElementsObsNativeStreamingOutput::GetAudioTracks()
{
	std::vector<uint32_t> result;

	const char *mode = config_get_string(obs_frontend_get_profile_config(),
					     "Output", "Mode");
	bool advOut = stricmp(mode, "Advanced") == 0;

	if (advOut) {
		int streamTrack =
			config_get_int(obs_frontend_get_profile_config(),
				       "AdvOut", "TrackIndex") -
			1;

		result.push_back(streamTrack);
	} else {
		result.push_back(0);
	}


	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeRecordingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeRecordingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo,
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		audioCompositionInfo)
{
	// NOP: This is managed by the OBS front-end

	ConnectOutputEvents();

	return true;
}

void StreamElementsObsNativeRecordingOutput::StopInternal()
{
	// NOP: This is managed by the OBS front-end

	DisconnectOutputEvents();
}

void StreamElementsObsNativeRecordingOutput::SerializeOutputSettings(
	CefRefPtr<CefValue> &output)
{
	auto d = CefDictionaryValue::Create();

	obs_output_t *obs_output =
		obs_frontend_get_recording_output();

	obs_data_t *obs_output_settings = obs_output_get_settings(obs_output);

	d->SetString("type", obs_output_get_id(obs_output));
	d->SetValue("settings", SerializeObsData(obs_output_settings));

	obs_data_release(obs_output_settings);

	obs_output_release(obs_output);

	output->SetDictionary(d);
}

std::vector<uint32_t> StreamElementsObsNativeRecordingOutput::GetAudioTracks()
{
	std::vector<uint32_t> result;

	const char *mode = config_get_string(obs_frontend_get_profile_config(),
					     "Output", "Mode");
	bool advOut = stricmp(mode, "Advanced") == 0;

	int tracks = config_get_int(obs_frontend_get_profile_config(),
				    advOut ? "AdvOut" : "SimpleOutput",
				    "RecTracks");

	for (size_t i = 0; i < MAX_AUDIO_MIXES; ++i) {
		if (tracks & (1 << i)) {
			result.push_back(i);
		}
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsCustomRecordingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsCustomRecordingOutput::CanDisable()
{
	return true;
}

bool StreamElementsCustomRecordingOutput::CanSplitRecordingOutput()
{
	OBSOutputAutoRelease output = GetOutput();

	if (!output)
		return false;

	if (!obs_output_active(output))
		return false;

	if (!obs_output_paused(output))
		return false;

	return true;
}

bool StreamElementsCustomRecordingOutput::TriggerSplitRecordingOutput()
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	if (!CanSplitRecordingOutput())
		return false;

	OBSOutputAutoRelease output = GetOutput();

	proc_handler_t *ph = obs_output_get_proc_handler(output);
	uint8_t stack[128];
	calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	proc_handler_call(ph, "split_file", &cd);
	bool result = calldata_bool(&cd, "split_file_enabled");

	return result;
}

bool StreamElementsCustomRecordingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo,
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		audioCompositionInfo)
{
	if (m_output)
		return false;

	if (!videoCompositionInfo)
		return false;

	OBSEncoderAutoRelease recordingVideoEncoder =
		videoCompositionInfo->GetRecordingVideoEncoderRef();

	if (!recordingVideoEncoder &&
	    videoCompositionInfo->IsObsNative()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: OBS Native recording video encoder does not exist yet on recording output '%s'",
		     GetId().c_str());

		SetError(
			"OBS Native recording video encoder does not exist yet");

		auto args = CefDictionaryValue::Create();
		args->SetString(
			"reason",
			"OBS Native recording video encoder does not exist yet");

		dispatch_event(this, "hostRecordingOutputError", args);
		dispatch_event(this, "hostRecordingOutputStopped");
		dispatch_list_change_event(this);

		return false;
	}

	bool ffmpegOutput =
		strcmpi(config_get_string(obs_frontend_get_profile_config(),
					  "AdvOut", "RecType"),
			"FFmpeg") == 0;

	bool ffmpegRecording =
		ffmpegOutput &&
		config_get_bool(obs_frontend_get_profile_config(), "AdvOut",
				"FFOutputToFile");

	//////////////////////////////////
	// Parse OBS settings (defaults)
	//////////////////////////////////

	std::string basePath = "";

	auto advBasePath = config_get_string(
		obs_frontend_get_profile_config(), "AdvOut",
		ffmpegRecording ? "FFFilePath" : "RecFilePath");
	if (advBasePath) {
		basePath = advBasePath;
	} else {
		basePath = config_get_string(obs_frontend_get_profile_config(),
					     "SimpleOutput", "FilePath");
	}

	std::string recFormat = config_get_string(
		obs_frontend_get_profile_config(), "AdvOut",
		ffmpegRecording ? "FFExtension" : "RecFormat2");
	std::string filenameFormat =
		config_get_string(obs_frontend_get_profile_config(), "Output",
				  "FilenameFormatting");
	if (!filenameFormat.size())
		filenameFormat = "%CCYY-%MM-%DD %hh-%mm-%ss";

	filenameFormat = std::string("SE.Live  ") + filenameFormat + std::string(" ") + GetName() + std::string(".mkv");

	bool overwriteIfExists =
		config_get_bool(obs_frontend_get_profile_config(), "Output",
				"OverwriteIfExists");
	bool noSpace =
		config_get_bool(obs_frontend_get_profile_config(), "AdvOut",
				ffmpegRecording ? "FFFileNameWithoutSpace"
						: "RecFileNameWithoutSpace");
	bool splitFile = config_get_bool(obs_frontend_get_profile_config(),
					 "AdvOut", "RecSplitFile");

	auto splitFileType =
		splitFile ? config_get_string(obs_frontend_get_profile_config(),
					      "AdvOut", "RecSplitFileType")
			  : "";

	auto splitFileTime =
		(strcmpi(splitFileType, "Time") == 0)
			? config_get_int(obs_frontend_get_profile_config(),
					 "AdvOut", "RecSplitFileTime")
			: 0;

	auto splitFileSize = (strcmpi(splitFileType, "Size") == 0)
			? config_get_int(obs_frontend_get_profile_config(),
					 "AdvOut",
						 "RecSplitFileSize")
				: 0;

	splitFile = true; // Always enable file splitting, even if only manual

	//////////////////////////////
	// m_recordingSettings parse
	//////////////////////////////

	if (m_recordingSettings->HasKey("settings") &&
	    m_recordingSettings->GetType("settings") == VTYPE_DICTIONARY) {
		auto d = m_recordingSettings->GetDictionary("settings");

		if (d->HasKey("fileNameFormat") &&
		    d->GetType("fileNameFormat") == VTYPE_STRING) {
			filenameFormat = d->GetString("fileNameFormat");
		}

		if (d->HasKey("splitAtMaximumMegabytes") &&
		    d->GetType("splitAtMaximumMegabytes") == VTYPE_INT) {
			splitFileSize = d->GetInt("splitAtMaximumMegabytes");
		}

		if (d->HasKey("splitAtMaximumDurationSeconds") &&
		    d->GetType("splitAtMaximumDurationSeconds") == VTYPE_INT) {
			splitFileTime =
				d->GetInt("splitAtMaximumDurationSeconds");
		}

		if (d->HasKey("overwriteExistingFiles") &&
		    d->GetType("overwriteExistingFiles") ==
			    VTYPE_BOOL) {
			overwriteIfExists =
				d->GetBool("overwriteExistingFiles");
		}

		if (d->HasKey("allowSpacesInFileNames") &&
		    d->GetType("allowSpacesInFileNames") ==
			    VTYPE_BOOL) {
			noSpace = !d->GetBool("allowSpacesInFileNames");
		}
	}

	splitFile = splitFileSize > 0 || splitFileTime > 0;

	////////////////////////////////////////////////
	// Parse actual filename format and extenstion
	////////////////////////////////////////////////

	std::string filenameExtension = "mkv";
	{
		auto ext = os_get_path_extension(filenameFormat.c_str());
		if (ext && strlen(ext) > 1) {
			filenameExtension = (ext + 1);

			filenameFormat = filenameFormat.substr(
				0, filenameFormat.size() - strlen(ext));
		}
	}

	////////////////////////////////////////////////
	// Setup output settings
	////////////////////////////////////////////////

	OBSDataAutoRelease settings = obs_data_create();

	std::string formattedFilename = os_generate_formatted_filename(
		filenameExtension.c_str(), !noSpace, filenameFormat.c_str());

	std::string formattedPath = basePath + "/" + formattedFilename;

	obs_data_set_string(settings, "muxer_settings", "");
	obs_data_set_string(settings, "path", formattedPath.c_str());

	obs_data_set_string(settings, "directory", basePath.c_str());
	obs_data_set_string(settings, "format", filenameFormat.c_str());
	obs_data_set_string(settings, "extension", filenameExtension.c_str());
	obs_data_set_bool(settings, "allow_spaces", !noSpace);
	obs_data_set_bool(settings, "allow_overwrite", overwriteIfExists);
	obs_data_set_bool(settings, "split_file", true);
	obs_data_set_int(settings, "max_time_sec", splitFileTime);
	obs_data_set_int(settings, "max_size_mb", splitFileSize);

	OBSDataAutoRelease hotkeyData = obs_data_create();

	const char *output_type = "ffmpeg_muxer";

	m_output = obs_output_create(output_type, GetId().c_str(), settings,
				     hotkeyData);

	if (m_output) {
		OBSEncoderAutoRelease recordingVideoEncoder =
			videoCompositionInfo->GetRecordingVideoEncoderRef();

		obs_output_set_video_encoder(
			m_output, recordingVideoEncoder);

		size_t audioEncodersCount = 0;

		for (size_t i = 0; i < m_audioTracks.size(); ++i) {
			OBSEncoderAutoRelease encoder =
				audioCompositionInfo
					->GetRecordingAudioEncoderRef(
						m_audioTracks[i]);

			if (!encoder) {
				blog(LOG_WARNING,
				     "obs-streamelements-core: audio encoder for audio track %d does not exist on recording output '%s'",
				     m_audioTracks[i], GetId().c_str());

				break;
			}

			obs_output_set_audio_encoder(m_output, encoder, i);

			++audioEncodersCount;
		}

		if (audioEncodersCount) {
			ConnectOutputEvents();

			if (obs_output_start(m_output)) {
				m_videoCompositionInfo = videoCompositionInfo;

				return true;
			} else {
				SetErrorIfEmpty("Failed to start output");
			}
		} else {
			blog(LOG_ERROR,
			     "obs-streamelements-core: no audio encoders were set up on recording output '%s'",
			     GetId().c_str());

			SetError(
				"No audio encoders were set up for requested audio tracks");
		}

		DisconnectOutputEvents();

		obs_output_release(m_output);
		m_output = nullptr;
	} else {
		blog(LOG_ERROR,
		     "obs-streamelements-core: failed to create recording output '%s' of type '%s'",
		     GetId().c_str(), output_type);

		SetError("Failed to create output");
	}

	return false;
}

void StreamElementsCustomRecordingOutput::StopInternal()
{
	if (!m_videoCompositionInfo)
		return;

	if (!m_output)
		return;

	// obs_output_stop(m_output);

	obs_output_force_stop(m_output);

	DisconnectOutputEvents();

	obs_output_release(m_output);
	m_output = nullptr;
}

void StreamElementsCustomRecordingOutput::SerializeOutputSettings(
	CefRefPtr<CefValue> &output)
{
	output->SetDictionary(m_recordingSettings);
}

std::shared_ptr<StreamElementsCustomRecordingOutput>
StreamElementsCustomRecordingOutput::Create(CefRefPtr<CefValue> input)
{
	if (!input.get())
		return nullptr;

	if (input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	auto rootDict = input->GetDictionary();

	if (!rootDict->HasKey("id") || !rootDict->HasKey("name"))
		return nullptr;

	if (rootDict->GetType("id") != VTYPE_STRING ||
	    rootDict->GetType("name") != VTYPE_STRING)
		return nullptr;

	auto auxData = (rootDict->HasKey("auxiliaryData") &&
			rootDict->GetType("auxiliaryData") == VTYPE_DICTIONARY)
			       ? rootDict->GetDictionary("auxiliaryData")
			       : CefDictionaryValue::Create();

	std::string id = rootDict->GetString("id");
	std::string name = rootDict->GetString("name");
	std::string videoCompositionId =
		(rootDict->HasKey("videoCompositionId") &&
				 rootDict->GetType("videoCompositionId") ==
					 VTYPE_STRING
			 ? rootDict->GetString("videoCompositionId")
			 : "");
	std::string audioCompositionId =
		(rootDict->HasKey("audioCompositionId") &&
				 rootDict->GetType("audioCompositionId") ==
					 VTYPE_STRING
			 ? rootDict->GetString("audioCompositionId")
			 : "");
	bool isEnabled = rootDict->HasKey("isEnabled") &&
					 rootDict->GetType("isEnabled") ==
						 VTYPE_BOOL
				 ? rootDict->GetBool("isEnabled")
				 : true;

	auto videoComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager()
			->GetVideoCompositionById(videoCompositionId);

	auto audioComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetAudioCompositionManager()
			->GetAudioCompositionById(audioCompositionId);

	auto recordingSettings = CefDictionaryValue::Create();

	if (rootDict->HasKey("recordingSettings") &&
	    rootDict->GetType("recordingSettings") == VTYPE_DICTIONARY) {
		recordingSettings =
			rootDict->GetDictionary("recordingSettings");
	}

	if (!videoComposition.get() && !videoCompositionId.size()) {
		videoComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetObsNativeVideoComposition();
	}

	if (!audioComposition.get() && !audioCompositionId.size()) {
		audioComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetAudioCompositionManager()
				->GetObsNativeAudioComposition();
	}

	if (!videoComposition.get()) {
		return nullptr;
	}

	if (!audioComposition.get()) {
		return nullptr;
	}

	auto audioTracks = DeserializeAudioTracks(rootDict);

	auto result = std::make_shared<StreamElementsCustomRecordingOutput>(
		id, name, recordingSettings, videoComposition, audioComposition,
		audioTracks,
		auxData);

	result->SetEnabled(isEnabled);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeReplayBufferOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeReplayBufferOutput::CanSaveReplayBuffer()
{
	return IsActive() && obs_frontend_replay_buffer_active();
}

bool StreamElementsObsNativeReplayBufferOutput::TriggerSaveReplayBuffer()
{
	if (!CanSaveReplayBuffer())
		return;

	obs_frontend_replay_buffer_save();

	return true;
}

bool StreamElementsObsNativeReplayBufferOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo,
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		audioCompositionInfo)
{
	// NOP: This is managed by the OBS front-end

	ConnectOutputEvents();

	return true;
}

void StreamElementsObsNativeReplayBufferOutput::StopInternal()
{
	// NOP: This is managed by the OBS front-end

	DisconnectOutputEvents();
}

void StreamElementsObsNativeReplayBufferOutput::SerializeOutputSettings(
	CefRefPtr<CefValue> &output)
{
	auto d = CefDictionaryValue::Create();

	obs_output_t *obs_output = obs_frontend_get_replay_buffer_output();

	obs_data_t *obs_output_settings = obs_output_get_settings(obs_output);

	d->SetString("type", obs_output_get_id(obs_output));
	d->SetValue("settings", SerializeObsData(obs_output_settings));

	obs_data_release(obs_output_settings);

	obs_output_release(obs_output);

	output->SetDictionary(d);
}

std::vector<uint32_t>
StreamElementsObsNativeReplayBufferOutput::GetAudioTracks()
{
	std::vector<uint32_t> result;

	const char *mode = config_get_string(obs_frontend_get_profile_config(),
					     "Output", "Mode");
	bool advOut = stricmp(mode, "Advanced") == 0;

	int tracks = config_get_int(obs_frontend_get_profile_config(),
				    advOut ? "AdvOut" : "SimpleOutput",
				    "RecTracks");

	for (size_t i = 0; i < MAX_AUDIO_MIXES; ++i) {
		if (tracks & (1 << i)) {
			result.push_back(i);
		}
	}

	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsCustomReplayBufferOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsCustomReplayBufferOutput::CanDisable()
{
	return true;
}

bool StreamElementsCustomReplayBufferOutput::CanSplitRecordingOutput()
{
	OBSOutputAutoRelease output = GetOutput();

	if (!output)
		return false;

	if (!obs_output_active(output))
		return false;

	if (!obs_output_paused(output))
		return false;

	return true;
}

bool StreamElementsCustomReplayBufferOutput::TriggerSplitRecordingOutput()
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	if (!CanSplitRecordingOutput())
		return false;

	OBSOutputAutoRelease output = GetOutput();

	proc_handler_t *ph = obs_output_get_proc_handler(output);
	uint8_t stack[128];
	calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	proc_handler_call(ph, "split_file", &cd);
	bool result = calldata_bool(&cd, "split_file_enabled");

	return result;
}

bool StreamElementsCustomReplayBufferOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo,
	std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
		audioCompositionInfo)
{
	if (m_output)
		return false;

	if (!videoCompositionInfo)
		return false;

	OBSEncoderAutoRelease recordingVideoEncoder =
		videoCompositionInfo->GetRecordingVideoEncoderRef();

	if (!recordingVideoEncoder && videoCompositionInfo->IsObsNative()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: OBS Native recording video encoder does not exist yet on replay buffer output '%s'",
		     GetId().c_str());

		SetError(
			"OBS Native recording video encoder does not exist yet");

		auto args = CefDictionaryValue::Create();
		args->SetString(
			"reason",
			"OBS Native recording video encoder does not exist yet");

		dispatch_event(this, "hostReplayBufferOutputError", args);
		dispatch_event(this, "hostReplayBufferOutputStopped");
		dispatch_list_change_event(this);

		return false;
	}

	bool ffmpegOutput =
		strcmpi(config_get_string(obs_frontend_get_profile_config(),
					  "AdvOut", "RecType"),
			"FFmpeg") == 0;

	bool ffmpegRecording =
		ffmpegOutput &&
		config_get_bool(obs_frontend_get_profile_config(), "AdvOut",
				"FFOutputToFile");

	//////////////////////////////////
	// Parse OBS settings (defaults)
	//////////////////////////////////

	std::string basePath = "";

	auto advBasePath = config_get_string(
		obs_frontend_get_profile_config(), "AdvOut",
		ffmpegRecording ? "FFFilePath" : "RecFilePath");
	if (advBasePath) {
		basePath = advBasePath;
	} else {
		basePath = config_get_string(obs_frontend_get_profile_config(),
					     "SimpleOutput", "FilePath");
	}

	std::string recFormat = config_get_string(
		obs_frontend_get_profile_config(), "AdvOut",
		ffmpegRecording ? "FFExtension" : "RecFormat2");
	std::string filenameFormat =
		config_get_string(obs_frontend_get_profile_config(), "Output",
				  "FilenameFormatting");
	if (!filenameFormat.size())
		filenameFormat = "Replay %CCYY-%MM-%DD %hh-%mm-%ss";

	filenameFormat = std::string("SE.Live  ") + filenameFormat +
			 std::string(" ") + GetName() + std::string(".mkv");

	bool overwriteIfExists =
		config_get_bool(obs_frontend_get_profile_config(), "Output",
				"OverwriteIfExists");
	bool noSpace =
		config_get_bool(obs_frontend_get_profile_config(), "AdvOut",
				ffmpegRecording ? "FFFileNameWithoutSpace"
						: "RecFileNameWithoutSpace");
	bool splitFile = config_get_bool(obs_frontend_get_profile_config(),
					 "AdvOut", "RecSplitFile");

	auto splitFileType =
		splitFile ? config_get_string(obs_frontend_get_profile_config(),
					      "AdvOut", "RecSplitFileType")
			  : "";

	auto splitFileTime =
		(strcmpi(splitFileType, "Time") == 0)
			? config_get_int(obs_frontend_get_profile_config(),
					 "AdvOut", "RecSplitFileTime")
			: 0;

	auto splitFileSize =
		(strcmpi(splitFileType, "Size") == 0)
			? config_get_int(obs_frontend_get_profile_config(),
					 "AdvOut", "RecSplitFileSize")
			: 0;

	splitFile = true; // Always enable file splitting, even if only manual

	//////////////////////////////
	// m_recordingSettings parse
	//////////////////////////////

	if (m_recordingSettings->HasKey("settings") &&
	    m_recordingSettings->GetType("settings") == VTYPE_DICTIONARY) {
		auto d = m_recordingSettings->GetDictionary("settings");

		if (d->HasKey("fileNameFormat") &&
		    d->GetType("fileNameFormat") == VTYPE_STRING) {
			filenameFormat = d->GetString("fileNameFormat");
		}

		if (d->HasKey("splitAtMaximumMegabytes") &&
		    d->GetType("splitAtMaximumMegabytes") == VTYPE_INT) {
			splitFileSize = d->GetInt("splitAtMaximumMegabytes");
		}

		if (d->HasKey("splitAtMaximumDurationSeconds") &&
		    d->GetType("splitAtMaximumDurationSeconds") == VTYPE_INT) {
			splitFileTime =
				d->GetInt("splitAtMaximumDurationSeconds");
		}

		if (d->HasKey("overwriteExistingFiles") &&
		    d->GetType("overwriteExistingFiles") == VTYPE_BOOL) {
			overwriteIfExists =
				d->GetBool("overwriteExistingFiles");
		}

		if (d->HasKey("allowSpacesInFileNames") &&
		    d->GetType("allowSpacesInFileNames") == VTYPE_BOOL) {
			noSpace = !d->GetBool("allowSpacesInFileNames");
		}
	}

	splitFile = splitFileSize > 0 || splitFileTime > 0;

	////////////////////////////////////////////////
	// Parse actual filename format and extenstion
	////////////////////////////////////////////////

	std::string filenameExtension = "mkv";
	{
		auto ext = os_get_path_extension(filenameFormat.c_str());
		if (ext && strlen(ext) > 1) {
			filenameExtension = (ext + 1);

			filenameFormat = filenameFormat.substr(
				0, filenameFormat.size() - strlen(ext));
		}
	}

	////////////////////////////////////////////////
	// Setup output settings
	////////////////////////////////////////////////

	OBSDataAutoRelease settings = obs_data_create();

	//std::string formattedFilename = os_generate_formatted_filename(
	//	filenameExtension.c_str(), !noSpace, filenameFormat.c_str());
	//
	//std::string formattedPath = basePath + "/" + formattedFilename;

	obs_data_set_string(settings, "muxer_settings", "");
	//obs_data_set_string(settings, "path", formattedPath.c_str());

	obs_data_set_string(settings, "directory", basePath.c_str());
	obs_data_set_string(settings, "format", filenameFormat.c_str());
	obs_data_set_string(settings, "extension", filenameExtension.c_str());
	obs_data_set_bool(settings, "allow_spaces", !noSpace);
	obs_data_set_bool(settings, "allow_overwrite", overwriteIfExists);
	obs_data_set_bool(settings, "split_file", false);
	obs_data_set_int(settings, "max_time_sec", splitFileTime);
	obs_data_set_int(settings, "max_size_mb", splitFileSize);

	OBSDataAutoRelease hotkeyData = obs_data_create();

	const char *output_type = "replay_buffer";

	m_output = obs_output_create(output_type, GetId().c_str(), settings,
				     hotkeyData);

	if (m_output) {
		OBSEncoderAutoRelease recordingVideoEncoder =
			videoCompositionInfo->GetRecordingVideoEncoderRef();

		obs_output_set_video_encoder(m_output, recordingVideoEncoder);

		size_t audioEncodersCount = 0;

		for (size_t i = 0; i < m_audioTracks.size(); ++i) {
			OBSEncoderAutoRelease encoder =
				audioCompositionInfo
					->GetRecordingAudioEncoderRef(
						m_audioTracks[i]);

			if (!encoder) {
				blog(LOG_WARNING,
				     "obs-streamelements-core: audio encoder for audio track %d does not exist on replay buffer output '%s'",
				     m_audioTracks[i], GetId().c_str());

				break;
			}

			obs_output_set_audio_encoder(m_output, encoder, i);

			++audioEncodersCount;
		}

		if (audioEncodersCount) {
			ConnectOutputEvents();

			if (obs_output_start(m_output)) {
				m_videoCompositionInfo = videoCompositionInfo;

				return true;
			} else {
				SetErrorIfEmpty("Failed to start output");
			}
		} else {
			blog(LOG_ERROR,
			     "obs-streamelements-core: no audio encoders were set up on replay buffer output '%s'",
			     GetId().c_str());

			SetError(
				"No audio encoders were set up for requested audio tracks");
		}

		DisconnectOutputEvents();

		obs_output_release(m_output);
		m_output = nullptr;
	} else {
		blog(LOG_ERROR,
		     "obs-streamelements-core: failed to create replay buffer output '%s' of type '%s'",
		     GetId().c_str(), output_type);

		SetError("Failed to create output");
	}

	return false;
}

void StreamElementsCustomReplayBufferOutput::StopInternal()
{
	if (!m_videoCompositionInfo)
		return;

	if (!m_output)
		return;

	// obs_output_stop(m_output);

	obs_output_force_stop(m_output);

	DisconnectOutputEvents();

	obs_output_release(m_output);
	m_output = nullptr;
}

void StreamElementsCustomReplayBufferOutput::SerializeOutputSettings(
	CefRefPtr<CefValue> &output)
{
	output->SetDictionary(m_recordingSettings);
}

std::shared_ptr<StreamElementsCustomReplayBufferOutput>
StreamElementsCustomReplayBufferOutput::Create(CefRefPtr<CefValue> input)
{
	if (!input.get())
		return nullptr;

	if (input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	auto rootDict = input->GetDictionary();

	if (!rootDict->HasKey("id") || !rootDict->HasKey("name"))
		return nullptr;

	if (rootDict->GetType("id") != VTYPE_STRING ||
	    rootDict->GetType("name") != VTYPE_STRING)
		return nullptr;

	auto auxData = (rootDict->HasKey("auxiliaryData") &&
			rootDict->GetType("auxiliaryData") == VTYPE_DICTIONARY)
			       ? rootDict->GetDictionary("auxiliaryData")
			       : CefDictionaryValue::Create();

	std::string id = rootDict->GetString("id");
	std::string name = rootDict->GetString("name");
	std::string videoCompositionId =
		(rootDict->HasKey("videoCompositionId") &&
				 rootDict->GetType("videoCompositionId") ==
					 VTYPE_STRING
			 ? rootDict->GetString("videoCompositionId")
			 : "");
	std::string audioCompositionId =
		(rootDict->HasKey("audioCompositionId") &&
				 rootDict->GetType("audioCompositionId") ==
					 VTYPE_STRING
			 ? rootDict->GetString("audioCompositionId")
			 : "");
	bool isEnabled = rootDict->HasKey("isEnabled") &&
					 rootDict->GetType("isEnabled") ==
						 VTYPE_BOOL
				 ? rootDict->GetBool("isEnabled")
				 : true;

	auto videoComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager()
			->GetVideoCompositionById(videoCompositionId);

	auto audioComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetAudioCompositionManager()
			->GetAudioCompositionById(audioCompositionId);

	auto recordingSettings = CefDictionaryValue::Create();

	if (rootDict->HasKey("recordingSettings") &&
	    rootDict->GetType("recordingSettings") == VTYPE_DICTIONARY) {
		recordingSettings =
			rootDict->GetDictionary("recordingSettings");
	}

	if (!videoComposition.get() && !videoCompositionId.size()) {
		videoComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetObsNativeVideoComposition();
	}

	if (!audioComposition.get() && !audioCompositionId.size()) {
		audioComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetAudioCompositionManager()
				->GetObsNativeAudioComposition();
	}

	if (!videoComposition.get()) {
		return nullptr;
	}

	if (!audioComposition.get()) {
		return nullptr;
	}

	auto audioTracks = DeserializeAudioTracks(rootDict);

	auto result = std::make_shared<StreamElementsCustomReplayBufferOutput>(
		id, name, recordingSettings, videoComposition, audioComposition,
		audioTracks, auxData);

	result->SetEnabled(isEnabled);

	return result;
}

void StreamElementsCustomReplayBufferOutput::ConnectOutputEvents()
{
	StreamElementsOutputBase::ConnectOutputEvents();

	if (!m_output)
		return;

	auto handler = obs_output_get_signal_handler(m_output);

	signal_handler_connect(handler, "saved", handle_output_saved, this);
}

void StreamElementsCustomReplayBufferOutput::DisconnectOutputEvents()
{
	if (!m_output)
		return;

	auto handler = obs_output_get_signal_handler(m_output);

	signal_handler_disconnect(handler, "saved", handle_output_saved, this);

	StreamElementsOutputBase::DisconnectOutputEvents();
}

void StreamElementsCustomReplayBufferOutput::handle_output_saved(
	void* my_data, calldata_t* cd)
{
	auto self =
		static_cast<StreamElementsCustomReplayBufferOutput *>(my_data);

	if (!self->GetOutput())
		return;

	auto eventArgs = CefDictionaryValue::Create();

	calldata_t *proc_cd = calldata_create();
	auto proc_handler = obs_output_get_proc_handler(self->GetOutput());
	proc_handler_call(proc_handler, "get_last_replay", proc_cd);

	const char *path = nullptr;
	if (calldata_get_string(proc_cd, "path", &path)) {
		eventArgs->SetString("filePath", path);

		dispatch_event(self, "hostReplayBufferOutputSavedToLocalFile", eventArgs);
	}

	calldata_destroy(proc_cd);
}

bool StreamElementsCustomReplayBufferOutput::TriggerSaveReplayBuffer()
{
	if (!IsActive() || !GetOutput())
		return false;

	calldata_t *cd = calldata_create();
	auto proc_handler = obs_output_get_proc_handler(GetOutput());

	proc_handler_call(proc_handler, "save", cd);

	calldata_destroy(cd);

	return true;
}
