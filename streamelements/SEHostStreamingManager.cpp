#include "SEHostStreamingManager.hpp"

#include <obs-frontend-api.h>

SEHostStreamingManager::SEHostStreamingManager() {}

SEHostStreamingManager::~SEHostStreamingManager() {}

void SEHostStreamingManager::SerializeStreamingStatus(
	CefRefPtr<CefValue> &output)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetBool("isStreamingActive", obs_frontend_streaming_active());

	output->SetDictionary(d);
}

void SEHostStreamingManager::RequestStop()
{
	obs_frontend_streaming_stop();
}

bool SEHostStreamingManager::IsStreaming()
{
	return obs_frontend_streaming_active();
}

void SEHostStreamingManager::RequestStart()
{
	obs_frontend_streaming_start();
}
