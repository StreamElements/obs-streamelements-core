#include "StreamElementsOutputManager.hpp"
#include "StreamElementsUtils.hpp"

StreamElementsOutputManager::StreamElementsOutputManager(
	std::shared_ptr<StreamElementsVideoCompositionManager> compositionManager)
	: m_compositionManager(compositionManager)
{
	auto nativeOutput = std::make_shared<StreamElementsObsNativeOutput>(
		"native", "OBS Native",
		m_compositionManager->GetObsNativeVideoComposition());

	m_map[nativeOutput->GetId()] = nativeOutput;
}

StreamElementsOutputManager::~StreamElementsOutputManager()
{
	Reset();
}

void StreamElementsOutputManager::Reset()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	m_map.clear();
}

void StreamElementsOutputManager::DeserializeOutput(CefRefPtr<CefValue> input,
						    CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

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

	while (!id.size() || m_map.count(id)) {
		id = CreateGloballyUniqueIdString();
	}

	d->SetString("id", id);

	input->SetDictionary(d);

	auto customOutput = StreamElementsCustomOutput::Create(input);

	if (!customOutput.get())
		return;

	m_map[customOutput->GetId()] = customOutput;

	customOutput->SerializeOutput(output);
}

void StreamElementsOutputManager::SerializeAllOutputs(
	CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	auto d = CefDictionaryValue::Create();

	for (auto kv : m_map) {
		auto serializedOutput = CefValue::Create();

		kv.second->SerializeOutput(serializedOutput);

		d->SetValue(kv.first, serializedOutput);
	}

	output->SetDictionary(d);
}

void StreamElementsOutputManager::RemoveOutputsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	std::map<std::string, bool> map;

	if (!GetValidIds(input, map, true, false))
		return;

	// Remove all valid IDs
	for (auto kv : map) {
		m_map.erase(kv.first);
	}

	output->SetBool(true);
}

void StreamElementsOutputManager::EnableOutputsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	std::map<std::string, bool> map;

	if (!GetValidIds(input, map, false, false))
		return;

	// Remove all valid IDs
	for (auto kv : map) {
		m_map[kv.first]->SetEnabled(true);
	}

	output->SetBool(true);
}

void StreamElementsOutputManager::DisableOutputsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	std::map<std::string, bool> map;

	if (!GetValidIds(input, map, false, true))
		return;

	// Remove all valid IDs
	for (auto kv : map) {
		m_map[kv.first]->SetEnabled(false);
	}

	output->SetBool(true);
}

bool StreamElementsOutputManager::GetValidIds(
	CefRefPtr<CefValue> input,
	std::map<std::string, bool>& output, bool testRemove, bool testDisable)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

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

		if (!m_map.count(id))
			return false;

		if (testRemove && !m_map[id]->CanRemove())
			return false;

		if (testDisable && !m_map[id]->CanDisable())
			return false;

		output[id] = true;
	}

	return true;
}
