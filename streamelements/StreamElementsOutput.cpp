#include <obs-frontend-api.h>

#include "StreamElementsOutput.hpp"

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

	auto streamingSettings = CefValue::Create();

	SerializeStreamingSettings(streamingSettings);

	d->SetValue("streamingSettings", streamingSettings);

	output->SetDictionary(d);
}

StreamElementsOutputBase::StreamElementsOutputBase(
	std::string id, std::string name,
	std::shared_ptr<StreamElementsCompositionBase> composition)
	: m_id(id), m_name(name), m_composition(composition), m_enabled(false)
{
	m_compositionInfo = composition->GetCompositionInfo(this);

	m_enabled = !CanDisable();

	obs_frontend_add_event_callback(
		StreamElementsOutputBase::handle_obs_frontend_event, this);
}

StreamElementsOutputBase::~StreamElementsOutputBase()
{
	obs_frontend_remove_event_callback(
		StreamElementsOutputBase::handle_obs_frontend_event, this);

	if (IsActive()) {
		Stop();
	}
}

bool StreamElementsOutputBase::IsEnabled()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return m_enabled;
}

void StreamElementsOutputBase::SetEnabled(bool enabled)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!enabled && !CanDisable())
		return;

	if (enabled == m_enabled)
		return;

	m_enabled = enabled;

	if (CanStart()) {
		Start();
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

bool StreamElementsCustomOutput::IsActive()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (!m_output)
		return false;

	return obs_output_active(m_output);
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

////////////////////////////////////////////////////////////////////////////////
// StreamElementsObsNativeOutput
////////////////////////////////////////////////////////////////////////////////

bool StreamElementsObsNativeOutput::StartInternal(
	std::shared_ptr<StreamElementsCompositionBase::CompositionInfo>
	compositionInfo)
{
	if (IsActive())
		return false;

	obs_frontend_streaming_start();

	return true;
}

void StreamElementsObsNativeOutput::StopInternal()
{
	if (!IsActive())
		return;

	obs_frontend_streaming_stop();
}

bool StreamElementsObsNativeOutput::IsActive()
{
	return obs_frontend_streaming_active();
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
