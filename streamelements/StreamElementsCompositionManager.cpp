#include "StreamElementsCompositionManager.hpp"

StreamElementsCompositionManager::StreamElementsCompositionManager()
{
	auto composition = StreamElementsObsNativeComposition::Create();

	m_map[composition->GetId()] = composition;
}

StreamElementsCompositionManager::~StreamElementsCompositionManager()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	m_map.clear();
}

void StreamElementsCompositionManager::DeserializeComposition(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	// TODO: Implement
	output->SetBool(false);
}

void StreamElementsCompositionManager::SerializeAllCompositions(
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (auto kv : m_map) {
		CefRefPtr<CefValue> composition;

		kv.second->SerializeComposition(composition);

		d->SetValue(kv.second->GetId(), composition);
	}

	output->SetDictionary(d);
}

void StreamElementsCompositionManager::RemoveCompositionsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	output->SetBool(false);

	if (input->GetType() != VTYPE_LIST)
		return;

	auto list = input->GetList();

	std::map<std::string, bool> map;

	// Check if all IDs are valid
	for (size_t i = 0; i < list->GetSize(); ++i) {
		if (list->GetType(i) != VTYPE_STRING)
			return;

		std::string id = list->GetString(i);

		if (!id.size())
			return;

		if (!m_map.count(id))
			return;

		if (!m_map[id]->CanRemove())
			return;

		map[id] = true;
	}

	// Remove all valid IDs
	for (auto kv : map) {
		m_map.erase(kv.first);
	}

	output->SetBool(true);
}
