#include "StreamElementsAudioComposition.hpp"
#include <util/config-file.h>

#include "deps/json11/json11.hpp"

#include "StreamElementsUtils.hpp"
#include "StreamElementsObsSceneManager.hpp"
#include "StreamElementsMessageBus.hpp"
#include "StreamElementsGlobalStateManager.hpp"

static void
SerializeObsAudioEncoders(StreamElementsAudioCompositionBase *composition,
			  CefRefPtr<CefDictionaryValue> &root)
{
	auto info = composition->GetCompositionInfo(nullptr);

	auto streamingAudioEncoders = CefListValue::Create();
	for (size_t i = 0;; ++i) {
		auto encoder = info->GetStreamingAudioEncoder(i);

		if (!encoder)
			break;

		streamingAudioEncoders->SetDictionary(
			i, SerializeObsEncoder(encoder));
	}

	root->SetList("streamingAudioEncoders", streamingAudioEncoders);

	// Recording

	bool hasDifferentAudioEncoder = false;
	auto recordingAudioEncoders = CefListValue::Create();
	for (size_t i = 0;; ++i) {
		auto encoder = info->GetRecordingAudioEncoder(i);

		if (!encoder)
			break;

		if (encoder != info->GetStreamingAudioEncoder(i))
			hasDifferentAudioEncoder = true;

		recordingAudioEncoders->SetDictionary(
			i, SerializeObsEncoder(encoder));
	}

	if (hasDifferentAudioEncoder)
		root->SetList("recordingAudioEncoders", recordingAudioEncoders);
}

////////////////////////////////////////////////////////////////////////////////
// Composition base
////////////////////////////////////////////////////////////////////////////////

void StreamElementsAudioCompositionBase::SetName(std::string name)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	if (m_name == name)
		return;

	m_name = name;

	auto jsonValue = CefValue::Create();
	SerializeComposition(jsonValue);

	auto json = CefWriteJSON(jsonValue, JSON_WRITER_DEFAULT);
}

////////////////////////////////////////////////////////////////////////////////
// OBS Main Composition
////////////////////////////////////////////////////////////////////////////////

class StreamElementsDefaultAudioCompositionInfo
	: public StreamElementsAudioCompositionBase::CompositionInfo {
private:
	StreamElementsAudioCompositionEventListener *m_listener;

public:
	StreamElementsDefaultAudioCompositionInfo(
		std::shared_ptr<StreamElementsAudioCompositionBase> owner,
		StreamElementsAudioCompositionEventListener *listener)
		: StreamElementsAudioCompositionBase::CompositionInfo(owner,
								      listener),
		  m_listener(listener)
	{
	}

	virtual ~StreamElementsDefaultAudioCompositionInfo() {}

public:
	virtual bool IsObsNative() { return true; }

	virtual obs_encoder_t *GetStreamingAudioEncoder(size_t index)
	{
		auto output = obs_frontend_get_streaming_output();

		auto result = obs_output_get_audio_encoder(output, index);

		obs_output_release(output);

		return result;
	}

	virtual obs_encoder_t *GetRecordingAudioEncoder(size_t index)
	{
		auto output = obs_frontend_get_recording_output();

		auto result = obs_output_get_audio_encoder(output, index);

		obs_output_release(output);

		return result;
	}

	virtual audio_t *GetAudio() { return obs_get_audio(); }
};

std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
StreamElementsObsNativeAudioComposition::GetCompositionInfo(
	StreamElementsAudioCompositionEventListener *listener)
{
	return std::make_shared<StreamElementsDefaultAudioCompositionInfo>(
		shared_from_this(), listener);
}

void StreamElementsObsNativeAudioComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	auto root = CefDictionaryValue::Create();

	root->SetString("id", GetId());
	root->SetString("name", GetName());

	root->SetBool("isObsNativeComposition", true);
	root->SetBool("canRemove", CanRemove());

	SerializeObsAudioEncoders(this, root);

	output->SetDictionary(root);
}

////////////////////////////////////////////////////////////////////////////////
// Custom audio encoding
////////////////////////////////////////////////////////////////////////////////

class StreamElementsCustomAudioCompositionInfo
	: public StreamElementsAudioCompositionBase::CompositionInfo {
private:
	StreamElementsAudioCompositionEventListener *m_listener;

	audio_t *m_audio = nullptr;
	obs_encoder_t *m_streamingAudioEncoders[MAX_AUDIO_MIXES] = {nullptr};
	obs_encoder_t *m_recordingAudioEncoders[MAX_AUDIO_MIXES] = {nullptr};

public:
	StreamElementsCustomAudioCompositionInfo(
		std::shared_ptr<StreamElementsAudioCompositionBase> owner,
		StreamElementsAudioCompositionEventListener *listener,
		audio_t *audio, obs_encoder_t *streamingAudioEncoders[MAX_AUDIO_MIXES],
		obs_encoder_t *recordingAudioEncoders[MAX_AUDIO_MIXES])
		: StreamElementsAudioCompositionBase::CompositionInfo(owner,
								      listener),
		  m_audio(audio),
		  m_listener(listener)
	{
		for (size_t i = 0; i < MAX_AUDIO_MIXES; ++i) {
			m_streamingAudioEncoders[i] = streamingAudioEncoders[i];
			m_recordingAudioEncoders[i] = recordingAudioEncoders[i];
		}
	}

	virtual ~StreamElementsCustomAudioCompositionInfo() {}

public:
	virtual bool IsObsNative() { return false; }

	virtual obs_encoder_t *GetStreamingAudioEncoder(size_t index)
	{
		if (index > MAX_AUDIO_MIXES)
			return nullptr;

		return m_streamingAudioEncoders[index];
	}

	virtual obs_encoder_t *GetRecordingAudioEncoder(size_t index)
	{
		if (index > MAX_AUDIO_MIXES)
			return nullptr;

		return m_recordingAudioEncoders[index];
	}

	virtual audio_t *GetAudio() { return m_audio; }
};

// ctor only usable by this class
StreamElementsCustomAudioComposition::StreamElementsCustomAudioComposition(
	StreamElementsCustomAudioComposition::Private, std::string id,
	std::string name, std::string streamingAudioEncoderId,
	obs_data_t *streamingAudioEncoderSettings,
	obs_data_t *streamingAudioEncoderHotkeyData)
	: StreamElementsAudioCompositionBase(id, name)
{
	m_audio = obs_get_audio();

	for (size_t i = 0; i < MAX_AUDIO_MIXES; ++i) {
		m_streamingAudioEncoders[i] = obs_audio_encoder_create(
			streamingAudioEncoderId.c_str(),
			(name + ": streaming audio encoder").c_str(),
			streamingAudioEncoderSettings, i,
			streamingAudioEncoderHotkeyData);

		if (!m_streamingAudioEncoders[i]) {
			for (size_t di = 0; di < i; ++di) {
				// Destroy previously created encoders
				obs_encoder_release(
					m_streamingAudioEncoders[di]);
			}

			throw std::exception(
				"obs_audio_encoder_create() failed", 2);
		}

		obs_encoder_set_audio(m_streamingAudioEncoders[i], m_audio);
	}
}

void StreamElementsCustomAudioComposition::SetRecordingEncoder(
	std::string recordingAudioEncoderId,
	obs_data_t *recordingAudioEncoderSettings,
	obs_data_t *recordingAudioEncoderHotkeyData)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	for (size_t i = 0; i < MAX_AUDIO_MIXES; ++i) {
		m_recordingAudioEncoders[i] = obs_audio_encoder_create(
			recordingAudioEncoderId.c_str(),
			(GetName() + ": recording audio encoder").c_str(),
			recordingAudioEncoderSettings, i,
			recordingAudioEncoderHotkeyData);

		if (!m_recordingAudioEncoders[i]) {
			for (size_t di = 0; di < i; ++di) {
				// Destroy previously created encoders
				obs_encoder_release(
					m_recordingAudioEncoders[di]);

				m_recordingAudioEncoders[di] = nullptr;
			}

			throw std::exception(
				"obs_audio_encoder_create() failed", 2);
		}

		obs_encoder_set_audio(m_recordingAudioEncoders[i],
				      m_audio);
	}
}

StreamElementsCustomAudioComposition::~StreamElementsCustomAudioComposition()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	for (size_t i = 0; i < MAX_AUDIO_MIXES; ++i) {
		if (m_streamingAudioEncoders[i]) {
			obs_encoder_release(m_streamingAudioEncoders[i]);
			m_streamingAudioEncoders[i] = nullptr;
		}

		if (m_recordingAudioEncoders[i]) {
			obs_encoder_release(m_recordingAudioEncoders[i]);
			m_recordingAudioEncoders[i] = nullptr;
		}
	}

	m_audio = nullptr;
}

std::shared_ptr<StreamElementsAudioCompositionBase::CompositionInfo>
StreamElementsCustomAudioComposition::GetCompositionInfo(
	StreamElementsAudioCompositionEventListener *listener)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	return std::make_shared<StreamElementsCustomAudioCompositionInfo>(
		shared_from_this(), listener, m_audio, m_streamingAudioEncoders,
		m_recordingAudioEncoders);
}

void StreamElementsCustomAudioComposition::SerializeComposition(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	auto root = CefDictionaryValue::Create();

	root->SetString("id", GetId());
	root->SetString("name", GetName());

	root->SetBool("isObsNativeComposition", false);
	root->SetBool("canRemove", this->CanRemove());

	SerializeObsAudioEncoders(this, root);

	output->SetDictionary(root);
}

std::shared_ptr<StreamElementsCustomAudioComposition>
StreamElementsCustomAudioComposition::Create(
	std::string id, std::string name,
	CefRefPtr<CefValue> streamingAudioEncoder,
	CefRefPtr<CefValue> recordingAudioEncoder)
{
	if (!streamingAudioEncoder.get() ||
	    streamingAudioEncoder->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	std::string streamingAudioEncoderId;
	obs_data_t *streamingAudioEncoderSettings;
	obs_data_t *streamingAudioEncoderHotkeyData = nullptr;

	// For each encoder
	{
		auto encoderRoot = streamingAudioEncoder->GetDictionary();

		if (!encoderRoot->HasKey("class") ||
		    encoderRoot->GetType("class") != VTYPE_STRING)
			return nullptr;

		if (!encoderRoot->HasKey("settings") ||
		    encoderRoot->GetType("settings") != VTYPE_DICTIONARY)
			return nullptr;

		streamingAudioEncoderId = encoderRoot->GetString("class");

		streamingAudioEncoderSettings = obs_data_create();

		if (!DeserializeObsData(encoderRoot->GetValue("settings"),
					streamingAudioEncoderSettings)) {
			obs_data_release(streamingAudioEncoderSettings);

			return nullptr;
		}
	}

	std::exception_ptr exception = nullptr;
	std::shared_ptr<StreamElementsCustomAudioComposition> result = nullptr;

	try {
		result = Create(id, name, streamingAudioEncoderId,
				streamingAudioEncoderSettings,
				streamingAudioEncoderHotkeyData);
	} catch (...) {
		result = nullptr;

		exception = std::current_exception();
	}

	if (streamingAudioEncoderSettings)
		obs_data_release(streamingAudioEncoderSettings);

	if (streamingAudioEncoderHotkeyData)
		obs_data_release(streamingAudioEncoderHotkeyData);

	if (exception)
		std::rethrow_exception(exception);

	// Recording encoders
	if (recordingAudioEncoder.get() &&
	    recordingAudioEncoder->GetType() == VTYPE_DICTIONARY) {
		std::string recordingAudioEncoderId;
		obs_data_t *recordingAudioEncoderSettings = nullptr;
		obs_data_t *recordingAudioEncoderHotkeyData = nullptr;

		auto encoderRoot = recordingAudioEncoder->GetDictionary();

		if (!encoderRoot->HasKey("class") ||
			encoderRoot->GetType("class") != VTYPE_STRING)
			return nullptr;

		if (!encoderRoot->HasKey("settings") ||
			encoderRoot->GetType("settings") !=
				VTYPE_DICTIONARY)
			return nullptr;

		recordingAudioEncoderId =
			encoderRoot->GetString("class");

		recordingAudioEncoderSettings = obs_data_create();

		if (DeserializeObsData(encoderRoot->GetValue("settings"),
				       recordingAudioEncoderSettings)) {
			try {
				result->SetRecordingEncoder(
					recordingAudioEncoderId,
					recordingAudioEncoderSettings,
					recordingAudioEncoderHotkeyData);
			} catch (...) {
				result = nullptr;

				exception = std::current_exception();
			}
		}

		if (recordingAudioEncoderSettings)
			obs_data_release(recordingAudioEncoderSettings);

		if (recordingAudioEncoderHotkeyData)
			obs_data_release(recordingAudioEncoderHotkeyData);

		if (exception)
			std::rethrow_exception(exception);
	}

	return result;
}