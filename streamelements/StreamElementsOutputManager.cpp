#include "StreamElementsOutputManager.hpp"
StreamElementsOutputManager::StreamElementsOutputManager(
	std::shared_ptr<StreamElementsCompositionManager> compositionManager)
	: m_compositionManager(compositionManager)
{
	auto nativeOutput = std::make_shared<StreamElementsObsNativeOutput>(
		"native", "OBS Native",
		m_compositionManager->GetObsNativeComposition());

	m_map[nativeOutput->GetId()] = nativeOutput;
}

StreamElementsOutputManager::~StreamElementsOutputManager()
{
	m_map.clear();
}

void StreamElementsOutputManager::DeserializeOutput(CefRefPtr<CefValue> input,
						    CefRefPtr<CefValue> &output)
{
}

void StreamElementsOutputManager::SerializeAllOutputs(
	CefRefPtr<CefValue> &output)
{
}

void StreamElementsOutputManager::RemoveOutputsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
}

void StreamElementsOutputManager::EnableOutputsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
}

void StreamElementsOutputManager::DisableOutputsByIds(
	CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output)
{
}
