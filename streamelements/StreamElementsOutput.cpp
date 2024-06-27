#include <obs-frontend-api.h>

#include "StreamElementsOutput.hpp"
#include "StreamElementsGlobalStateManager.hpp"

void StreamElementsOutputBase::handle_obs_frontend_event(
	enum obs_frontend_event event,
	void* data)
{
	StreamElementsOutputBase *self = (StreamElementsOutputBase *)data;

	switch (event) {
	case OBS_FRONTEND_EVENT_STREAMING_STARTING:
	case OBS_FRONTEND_EVENT_STREAMING_STARTED:
		self->Start();
		break;
	case OBS_FRONTEND_EVENT_STREAMING_STOPPING:
	case OBS_FRONTEND_EVENT_STREAMING_STOPPED:
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
	d->SetString("compositionId", m_composition->GetId());
	d->SetBool("isEnabled", IsEnabled());
	d->SetBool("isActive", IsActive());
	d->SetBool("canDisable", CanDisable());
	d->SetBool("canRemove", !IsObsNative());
	d->SetBool("isObsNative", IsObsNative());

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

	SerializeStreamingSettings(streamingSettings);

	d->SetValue("streamingSettings", streamingSettings);

	output->SetDictionary(d);
}

StreamElementsOutputBase::StreamElementsOutputBase(
	std::string id, std::string name,
	std::shared_ptr<StreamElementsCompositionBase> composition,
	CefRefPtr<CefDictionaryValue> auxData)
	: m_id(id), m_name(name), m_composition(composition), m_enabled(false)
{
	m_auxData = auxData.get() ? auxData : CefDictionaryValue::Create();

	m_compositionInfo = composition->GetCompositionInfo(this);

	m_enabled = !CanDisable();

	obs_frontend_add_event_callback(
		StreamElementsOutputBase::handle_obs_frontend_event, this);
}

StreamElementsOutputBase::~StreamElementsOutputBase()
{
	obs_frontend_remove_event_callback(
		StreamElementsOutputBase::handle_obs_frontend_event, this);
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

	if (!m_compositionInfo)
		return false;

	if (!IsEnabled())
		return false;

	if (!obs_frontend_streaming_active())
		return false;

	if (IsActive())
		return false;

	return true;
}

bool StreamElementsOutputBase::Start()
{
	if (!CanStart())
		return false;

	return StartInternal(m_compositionInfo);
}

void StreamElementsOutputBase::Stop()
{
	StopInternal();
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsCustomOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsCustomOutput::CanDisable()
{
	return true;
}

bool StreamElementsCustomOutput::StartInternal(
	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
		compositionInfo)
{
	if (!compositionInfo)
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
			compositionInfo->GetStreamingVideoEncoder());

		for (size_t i = 0;; ++i) {
			auto encoder =
				compositionInfo->GetStreamingAudioEncoder(i);

			if (!encoder)
				break;

			obs_output_set_audio_encoder(m_output, encoder, i);
		}

		obs_output_set_service(m_output, m_service);

		if (obs_output_start(m_output)) {
			m_compositionInfo = compositionInfo;

			return true;
		}

		obs_output_release(m_output);
		m_output = nullptr;
	} else {
		obs_data_release(output_settings);
	}

	return false;
}

void StreamElementsCustomOutput::StopInternal()
{
	if (!m_compositionInfo)
		return;

	// obs_output_stop(m_output);

	obs_output_force_stop(m_output);

	obs_output_release(m_output);
	m_output = nullptr;
}

void StreamElementsCustomOutput::SerializeStreamingSettings(
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

std::shared_ptr <StreamElementsCustomOutput> StreamElementsCustomOutput::Create(CefRefPtr<CefValue> input)
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
	std::string compositionId =
		(rootDict->HasKey("compositionId") && rootDict->GetType("compositionId") ==
					       VTYPE_STRING
			? rootDict->GetString("compositionId")
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

	// TODO: Get composition from composition manager

	auto composition = StreamElementsGlobalStateManager::GetInstance()
		->GetCompositionManager()
		->GetCompositionById(id);

	if (!composition) {
		composition = StreamElementsGlobalStateManager::GetInstance()
				      ->GetCompositionManager()
				      ->GetObsNativeComposition();
	}

	auto result = std::make_shared<StreamElementsCustomOutput>(
		id, name, composition, service, bindToIP, auxData);

	result->SetEnabled(isEnabled);

	return result;
}

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeOutput::StartInternal(
	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
	compositionInfo)
{
	// NOP: This is managed by the OBS front-end
	return true;
}

void StreamElementsObsNativeOutput::StopInternal()
{
	// NOP
}

void StreamElementsObsNativeOutput::SerializeStreamingSettings(
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
