#include "StreamElementsOutputManager.hpp"
#include "StreamElementsUtils.hpp"

#define StreamingOutput StreamElementsOutputBase::StreamingOutput
#define RecordingOutput StreamElementsOutputBase::RecordingOutput
#define ReplayBufferOutput StreamElementsOutputBase::ReplayBufferOutput

StreamElementsOutputManager::StreamElementsOutputManager(
	std::shared_ptr<StreamElementsVideoCompositionManager>
		videoCompositionManager,
	std::shared_ptr<StreamElementsAudioCompositionManager>
		audioCompositionManager)
	: m_videoCompositionManager(videoCompositionManager), m_audioCompositionManager(audioCompositionManager)
{
	auto nativeStreamingOutput = std::make_shared<
		StreamElementsObsNativeStreamingOutput>(
		"default", "OBS Native Streaming",
		m_videoCompositionManager->GetObsNativeVideoComposition(),
		m_audioCompositionManager->GetObsNativeAudioComposition());

	m_map[StreamingOutput][nativeStreamingOutput->GetId()] = nativeStreamingOutput;

	auto nativeRecordingOutput =
		std::make_shared<StreamElementsObsNativeRecordingOutput>(
			"default", "OBS Native Recording",
		m_videoCompositionManager->GetObsNativeVideoComposition(),
		m_audioCompositionManager->GetObsNativeAudioComposition());

	m_map[RecordingOutput][nativeRecordingOutput->GetId()] = nativeRecordingOutput;

	auto nativeReplayBufferOutput = std::make_shared<
		StreamElementsObsNativeReplayBufferOutput>(
		"default", "OBS Native Replay Buffer",
		m_videoCompositionManager->GetObsNativeVideoComposition(),
		m_audioCompositionManager->GetObsNativeAudioComposition());

	m_map[ReplayBufferOutput][nativeReplayBufferOutput->GetId()] =
		nativeReplayBufferOutput;
}

StreamElementsOutputManager::~StreamElementsOutputManager()
{
	//Reset();

	m_map.clear();
}

void StreamElementsOutputManager::Reset()
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	for (auto kv : m_map) {
		std::list<std::string> keys_to_delete;
		
		for (auto pair : kv.second) {
			if (!pair.second->IsObsNativeOutput())
				keys_to_delete.push_back(pair.first);
		}
		
		for (auto key : keys_to_delete) {
			kv.second.erase(key);
		}
	}
}

void StreamElementsOutputManager::DeserializeOutput(
	StreamElementsOutputBase::ObsOutputType outputType,
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	if (!input.get())
		return;

	if (input->GetType() != VTYPE_DICTIONARY)
		return;

	auto d = input->GetDictionary();

	std::string id = "";
	if (d->HasKey("id") && d->GetType("id") == VTYPE_STRING) {
		id = d->GetString("id");
	}

	while (!id.size() || m_map[outputType].count(id)) {
		id = CreateGloballyUniqueIdString();
	}

	d->SetString("id", id);

	input->SetDictionary(d);

	if (outputType == StreamingOutput) {
		auto customOutput =
			StreamElementsCustomStreamingOutput::Create(input);

		if (!customOutput.get())
			return;

		m_map[outputType][customOutput->GetId()] = customOutput;

		customOutput->SerializeOutput(output);
	}
	else if (outputType == RecordingOutput) {
		auto customOutput =
			StreamElementsCustomRecordingOutput::Create(input);

		if (!customOutput.get())
			return;

		m_map[outputType][customOutput->GetId()] = customOutput;

		customOutput->SerializeOutput(output);
	} else {
		auto customOutput =
			StreamElementsCustomReplayBufferOutput::Create(input);

		if (!customOutput.get())
			return;

		m_map[outputType][customOutput->GetId()] = customOutput;

		customOutput->SerializeOutput(output);
	}
}

void StreamElementsOutputManager::SerializeAllOutputs(
	StreamElementsOutputBase::ObsOutputType outputType,
	CefRefPtr<CefValue> &output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	auto d = CefDictionaryValue::Create();

	for (auto kv : m_map[outputType]) {
		auto serializedOutput = CefValue::Create();

		kv.second->SerializeOutput(serializedOutput);

		d->SetValue(kv.first, serializedOutput);
	}

	output->SetDictionary(d);
}

void StreamElementsOutputManager::RemoveOutputsByIds(
	StreamElementsOutputBase::ObsOutputType outputType,
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::unique_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	std::map<std::string, bool> map;

	if (!GetValidIds(outputType, input, map, true, false))
		return;

	// Remove all valid IDs
	for (auto kv : map) {
		m_map[outputType].erase(kv.first);
	}

	output->SetBool(true);
}

void StreamElementsOutputManager::EnableOutputsByIds(
	StreamElementsOutputBase::ObsOutputType outputType,
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	std::map<std::string, bool> map;

	if (!GetValidIds(outputType, input, map, false, false))
		return;

	// Remove all valid IDs
	for (auto kv : map) {
		m_map[outputType][kv.first]->SetEnabled(true);
	}

	output->SetBool(true);
}

void StreamElementsOutputManager::DisableOutputsByIds(
	StreamElementsOutputBase::ObsOutputType outputType,
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	std::map<std::string, bool> map;

	if (!GetValidIds(outputType, input, map, false, true))
		return;

	// Remove all valid IDs
	for (auto kv : map) {
		m_map[outputType][kv.first]->SetEnabled(false);
	}

	output->SetBool(true);
}

void StreamElementsOutputManager::TriggerSplitRecordingOutputById(
	CefRefPtr<CefValue> input,
	CefRefPtr<CefValue>& output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	if (input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	if (m_map[RecordingOutput].count(id)) {
		output->SetBool(m_map[RecordingOutput][id]
					->TriggerSplitRecordingOutput());
	}
}

void StreamElementsOutputManager::TriggerSaveReplayBufferById(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::shared_lock<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	if (input->GetType() != VTYPE_STRING)
		return;

	std::string id = input->GetString();

	if (m_map[ReplayBufferOutput].count(id)) {
		output->SetBool(m_map[ReplayBufferOutput][id]
					->TriggerSaveReplayBuffer());
	}
}

bool StreamElementsOutputManager::GetValidIds(
	StreamElementsOutputBase::ObsOutputType outputType, CefRefPtr<CefValue> input,
	std::map<std::string, bool>& output, bool testRemove, bool testDisable)
{
	if (input->GetType() != VTYPE_LIST)
		return false;

	auto list = input->GetList();

	std::map<std::string, bool> map;

	// Check if all IDs are valid
	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return false;

		std::string id = list->GetString(i);

		if (!id.size())
			return false;

		if (!m_map[outputType].count(id))
			return false;

		if (testRemove && !m_map[outputType][id]->CanRemove())
			return false;

		if (testDisable && !m_map[outputType][id]->CanDisable())
			return false;

		output[id] = true;
	}

	return true;
}
