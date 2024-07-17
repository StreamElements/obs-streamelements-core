#include "StreamElementsVideoCompositionManager.hpp"

#include <QDialog>
#include <QVBoxLayout>
#include "StreamElementsVideoCompositionViewWidget.hpp"
#include "StreamElementsUtils.hpp"

StreamElementsVideoCompositionManager::StreamElementsVideoCompositionManager()
{
	m_nativeVideoComposition = StreamElementsObsNativeVideoComposition::Create();

	m_videoCompositionsMap[m_nativeVideoComposition->GetId()] = m_nativeVideoComposition;

	// TODO: Remove debug code
	auto testComposition =
		StreamElementsCustomVideoComposition::Create(
			"test1", "Test 1", 1920, 1080, "x264",
			obs_data_create(), obs_data_create());

	m_videoCompositionsMap[testComposition->GetId()] =
		testComposition;

	auto composition = m_nativeVideoComposition;

	auto widget = new StreamElementsVideoCompositionViewWidget(nullptr,
								   composition);

	auto dlg = new QDialog();
	dlg->setFixedSize(1024, 768);
	auto topLayout = new QVBoxLayout(dlg);
	topLayout->addWidget(widget);
	dlg->show();
}

StreamElementsVideoCompositionManager::~StreamElementsVideoCompositionManager()
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	m_videoCompositionsMap.clear();
}

void StreamElementsVideoCompositionManager::DeserializeComposition(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	// TODO: Implement
	output->SetBool(false);
}

void StreamElementsVideoCompositionManager::SerializeAllCompositions(
	CefRefPtr<CefValue>& output)
{
	std::lock_guard<decltype(m_mutex)> lock(m_mutex);

	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	for (auto kv : m_videoCompositionsMap) {
		CefRefPtr<CefValue> composition;

		kv.second->SerializeComposition(composition);

		d->SetValue(kv.second->GetId(), composition);
	}

	output->SetDictionary(d);
}

void StreamElementsVideoCompositionManager::RemoveCompositionsByIds(
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

		if (!m_videoCompositionsMap.count(id))
			return;

		if (!m_videoCompositionsMap[id]->CanRemove())
			return;

		map[id] = true;
	}

	// Remove all valid IDs
	for (auto kv : map) {
		m_videoCompositionsMap.erase(kv.first);
	}

	output->SetBool(true);
}
