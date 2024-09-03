#include <obs.hpp>
#include <obs-frontend-api.h>

#include "StreamElementsUtils.hpp"
#include "StreamElementsOutput.hpp"
#include "StreamElementsGlobalStateManager.hpp"

#include <ctime>

static void dispatch_list_change_event(StreamElementsOutputBase *output)
{
	if (output->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		DispatchClientJSEvent("hostStreamingOutputListChanged", "null");
	else
		DispatchClientJSEvent("hostRecordingOutputListChanged", "null");
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
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStarted");
	else
		dispatch_event(self, "hostRecordingOutputStarted");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_stop(void *my_data,
							 calldata_t *cd)
{
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
		else
			dispatch_event(self, "hostRecordingOutputError", args);
	}

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStopped");
	else
		dispatch_event(self, "hostRecordingOutputStopped");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_pause(void *my_data,
							  calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputPaused");
	else
		dispatch_event(self, "hostRecordingOutputPaused");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_unpause(void *my_data,
							    calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputUnpaused");
	else
		dispatch_event(self, "hostRecordingOutputUnpaused");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_starting(void *my_data,
							     calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStarting");
	else
		dispatch_event(self, "hostRecordingOutputStarting");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_stopping(void *my_data,
							     calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputStopping");
	else
		dispatch_event(self, "hostRecordingOutputStopping");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_activate(void *my_data,
							     calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputActivated");
	else
		dispatch_event(self, "hostRecordingOutputActivated");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_deactivate(void *my_data,
							       calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputDeactivated");
	else
		dispatch_event(self, "hostRecordingOutputDeactivated");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_output_reconnect(void *my_data,
							      calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputReconnecting");
	else
		dispatch_event(self, "hostRecordingOutputReconnecting");

	dispatch_list_change_event(self);
}

void
StreamElementsOutputBase::handle_output_reconnect_success(void *my_data,
							  calldata_t *cd)
{
	auto self = (StreamElementsOutputBase *)my_data;

	if (self->GetOutputType() == StreamElementsOutputBase::StreamingOutput)
		dispatch_event(self, "hostStreamingOutputReconnected");
	else
		dispatch_event(self, "hostRecordingOutputReconnected");

	dispatch_list_change_event(self);
}

void StreamElementsOutputBase::handle_obs_frontend_event(
	enum obs_frontend_event event,
	void* data)
{
	StreamElementsOutputBase *self = (StreamElementsOutputBase *)data;

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
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
		if (self->m_obsStateDependency == Streaming)
			self->Start();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
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

	case OBS_FRONTEND_EVENT_RECORDING_STOPPED:
		if (self->m_obsStateDependency == Recording)
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
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	auto d = CefDictionaryValue::Create();

	d->SetString("id", GetId());
	d->SetString("name", GetName());
	d->SetString("videoCompositionId", m_videoComposition->GetId());
	d->SetBool("isEnabled", IsEnabled());
	d->SetBool("isActive", IsActive());
	d->SetBool("canDisable", CanDisable());
	d->SetBool("canRemove", !IsObsNativeOutput());
	d->SetBool("isObsNativeVideoComposition", m_videoComposition->IsObsNativeComposition());
	d->SetBool("isObsNativeOutput", IsObsNativeOutput());

	if (m_error.size()) {
		auto error = CefDictionaryValue::Create();

		error->SetString("message", m_error);

		d->SetDictionary("error", error);
	} else {
		d->SetNull("error");
	}

	auto obs_output = GetOutput();

	if (output && IsActive()) {
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

	auto streamingSettings = CefValue::Create();

	SerializeOutputSettings(streamingSettings);

	switch (m_obsOutputType) {
	case StreamingOutput:
		d->SetString("type", "streaming");
		d->SetValue("streamingSettings", streamingSettings);
		break;
	case RecordingOutput:
		d->SetString("type", "recording");
		d->SetValue("recordingSettings", streamingSettings);
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
	CefRefPtr<CefDictionaryValue> auxData)
	: m_id(id),
	  m_name(name),
	  m_obsOutputType(obsOutputType),
	  m_obsStateDependency(obsStateDependency),
	  m_videoComposition(videoComposition),
	  m_enabled(false)
{
	m_auxData = auxData.get() ? auxData : CefDictionaryValue::Create();

	m_videoCompositionInfo = videoComposition->GetCompositionInfo(this);

	m_enabled = IsObsNativeOutput();

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
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_enabled;
}

bool StreamElementsOutputBase::IsActive()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	auto output = GetOutput();

	if (!output)
		return false;

	return obs_output_active(output);
}

void StreamElementsOutputBase::SetEnabled(bool enabled)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!enabled && !CanDisable())
		return;

	if (enabled == m_enabled)
		return;

	m_enabled = enabled;

	if (m_enabled) {
		if (CanStart()) {
			Start();
		}
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

	//if (!obs_frontend_streaming_active())
	//	return false;

	if (IsActive())
		return false;

	return true;
}

bool StreamElementsOutputBase::Start()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	SetError("");

	if (!CanStart())
		return false;

	return StartInternal(m_videoCompositionInfo);
}

void StreamElementsOutputBase::ConnectOutputEvents()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (m_outputEventsConnected)
		return;

	auto handler = obs_output_get_signal_handler(GetOutput());

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
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!m_outputEventsConnected)
		return;

	auto handler = obs_output_get_signal_handler(GetOutput());

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
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

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
		videoCompositionInfo)
{
	if (!videoCompositionInfo)
		return false;

	const char *output_type = obs_service_get_output_type(m_service);

	if (!output_type)
		output_type = "rtmp_output";

	obs_data_t *output_settings = obs_data_create();

	if (m_bindToIP.size()) {
		obs_data_set_string(output_settings, "bind_ip", m_bindToIP.c_str());
	}

	m_output = obs_output_create(output_type, GetId().c_str(),
						 output_settings, nullptr);

	if (m_output) {
		obs_output_set_video_encoder(
			m_output,
			videoCompositionInfo->GetStreamingVideoEncoder());

		for (size_t i = 0;; ++i) {
			auto encoder =
				videoCompositionInfo->GetStreamingAudioEncoder(i);

			if (!encoder)
				break;

			obs_output_set_audio_encoder(m_output, encoder, i);
		}

		obs_output_set_service(m_output, m_service);

		ConnectOutputEvents();

		if (obs_output_start(m_output)) {
			m_videoCompositionInfo = videoCompositionInfo;

			return true;
		}

		DisconnectOutputEvents();

		obs_output_release(m_output);
		m_output = nullptr;
	} else {
		obs_data_release(output_settings);
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
		     obs_data_get_string(service_settings, "server"));

	d->SetString("streamKey", obs_data_get_string(service_settings, "key"));

	bool useAuth = obs_data_get_bool(service_settings, "use_auth");

	d->SetBool("useAuth", useAuth);

	if (useAuth) {
		d->SetString("authUsername", obs_data_get_string(service_settings, "username"));
		d->SetString("authPassword", obs_data_get_string(service_settings, "password"));
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
	bool isEnabled = rootDict->HasKey("isEnabled") && rootDict->GetType("isEnabled") == VTYPE_BOOL
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

	if (!videoComposition && !videoCompositionId.size()) {
		videoComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetObsNativeVideoComposition();
	} else {
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

	auto result = std::make_shared<StreamElementsCustomStreamingOutput>(
		id, name, videoComposition, service, bindToIP, auxData);

	result->SetEnabled(isEnabled);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeStreamingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeStreamingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
	videoCompositionInfo)
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

	d->SetString("serverUrl",
		     obs_data_get_string(service_settings, "server"));

	d->SetString("streamKey", obs_data_get_string(service_settings, "key"));

	bool useAuth = obs_data_get_bool(service_settings, "use_auth");

	d->SetBool("useAuth", useAuth);

	if (useAuth) {
		d->SetString("authUsername",
			     obs_data_get_string(service_settings, "username"));
		d->SetString("authPassword",
			     obs_data_get_string(service_settings, "password"));
	}

	obs_data_release(service_settings);

	output->SetDictionary(d);
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeRecordingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeRecordingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo)
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

////////////////////////////////////////////////////////////////////////////////
// StreamElementsCustomRecordingOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsCustomRecordingOutput::CanDisable()
{
	return true;
}

bool StreamElementsCustomRecordingOutput::CanSplitRecordingOutput()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	auto output = GetOutput();

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
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!CanSplitRecordingOutput())
		return false;

	proc_handler_t *ph = obs_output_get_proc_handler(GetOutput());
	uint8_t stack[128];
	calldata cd;
	calldata_init_fixed(&cd, stack, sizeof(stack));
	proc_handler_call(ph, "split_file", &cd);
	bool result = calldata_bool(&cd, "split_file_enabled");

	return result;
}

bool StreamElementsCustomRecordingOutput::StartInternal(
	std::shared_ptr<StreamElementsVideoCompositionBase::CompositionInfo>
		videoCompositionInfo)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (m_output)
		return false;

	if (!videoCompositionInfo)
		return false;

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

	filenameFormat = std::string("SE.Live  ") + filenameFormat + GetName() + std::string(".mkv");

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

	if (m_recordingSettings->HasKey("fileNameFormat") &&
	    m_recordingSettings->GetType("fileNameFormat") == VTYPE_STRING) {
		filenameFormat =
			m_recordingSettings->GetString("fileNameFormat");
	}

	if (m_recordingSettings->HasKey("splitAtMaximumMegabytes") &&
	    m_recordingSettings->GetType("splitAtMaximumMegabytes") == VTYPE_INT) {
		splitFileSize =
			m_recordingSettings->GetInt("splitAtMaximumMegabytes");
	}

	if (m_recordingSettings->HasKey("splitAtMaximumDurationSeconds") &&
	    m_recordingSettings->GetType("splitAtMaximumDurationSeconds") ==
		    VTYPE_INT) {
		splitFileTime = m_recordingSettings->GetInt(
			"splitAtMaximumDurationSeconds");
	}

	if (m_recordingSettings->HasKey("overwriteExistingFiles") &&
	    m_recordingSettings->GetType("overwriteExistingFiles") ==
		    VTYPE_BOOL) {
		overwriteIfExists =
			m_recordingSettings->GetBool("overwriteExistingFiles");
	}

	if (m_recordingSettings->HasKey("allowSpacesInFileNames") &&
	    m_recordingSettings->GetType("allowSpacesInFileNames") ==
		    VTYPE_BOOL) {
		noSpace =
			!m_recordingSettings->GetBool("allowSpacesInFileNames");
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
	obs_data_set_bool(settings, "split_file", splitFile);
	obs_data_set_int(settings, "max_time_sec", splitFileTime);
	obs_data_set_int(settings, "max_size_mb", splitFileSize);

	m_output = obs_output_create("ffmpeg_muxer", GetId().c_str(), settings,
				     nullptr);

	if (m_output) {
		obs_output_set_video_encoder(
			m_output,
			videoCompositionInfo->GetRecordingVideoEncoder());

		for (size_t i = 0;; ++i) {
			auto encoder =
				videoCompositionInfo->GetRecordingAudioEncoder(
					i);

			if (!encoder)
				break;

			obs_output_set_audio_encoder(m_output, encoder, i);
		}

		ConnectOutputEvents();

		if (obs_output_start(m_output)) {
			m_videoCompositionInfo = videoCompositionInfo;

			return true;
		}

		DisconnectOutputEvents();

		obs_output_release(m_output);
		m_output = nullptr;
	}

	return false;
}

void StreamElementsCustomRecordingOutput::StopInternal()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

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
	/*
	auto d = CefDictionaryValue::Create();

	auto obs_output = GetOutput();

	if (obs_output) {
		auto obs_output_settings = obs_output_get_settings(obs_output);

		d->SetString("type", obs_output_get_id(obs_output));
		d->SetValue("settings", SerializeObsData(obs_output_settings));

		obs_data_release(obs_output_settings);
	}
	*/

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
	bool isEnabled = rootDict->HasKey("isEnabled") &&
					 rootDict->GetType("isEnabled") ==
						 VTYPE_BOOL
				 ? rootDict->GetBool("isEnabled")
				 : true;

	auto videoComposition =
		StreamElementsGlobalStateManager::GetInstance()
			->GetVideoCompositionManager()
			->GetVideoCompositionById(videoCompositionId);

	auto recordingSettings = CefDictionaryValue::Create();

	if (rootDict->HasKey("recordingSettings") &&
	    rootDict->GetType("recordingSettings") == VTYPE_DICTIONARY) {
		recordingSettings =
			rootDict->GetDictionary("recordingSettings");
	}

	if (!videoComposition && !videoCompositionId.size()) {
		videoComposition =
			StreamElementsGlobalStateManager::GetInstance()
				->GetVideoCompositionManager()
				->GetObsNativeVideoComposition();
	}

	auto result = std::make_shared<StreamElementsCustomRecordingOutput>(
		id, name, recordingSettings, videoComposition, auxData);

	result->SetEnabled(isEnabled);

	return result;
}
