#include "SEHostInfoProvider.hpp"

#include "SEAnalyticsEventsManager.hpp"

#include "SEUtils/SEObsSourceCatalogUtils.hpp"
#include "SEUtils/SESystemInfoUtils.hpp"

#include <obs.h>

SEHostInfoProvider::SEHostInfoProvider(
	std::shared_ptr<SEAnalyticsEventsManager> analyticsEventsManager)
	: m_analyticsEventsManager(analyticsEventsManager)
{
}

SEHostInfoProvider::~SEHostInfoProvider() {}

void SEHostInfoProvider::SerializeHostProperties(CefRefPtr<CefValue> &output)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetString("obsVersionString", obs_get_version_string());
	d->SetString("cefVersionString", GetCefVersionString());
	d->SetString("cefPlatformApiHash", GetCefPlatformApiHash());
	d->SetString("cefUniversalApiHash", GetCefUniversalApiHash());
	d->SetString("hostPluginVersionString", GetSEPluginVersionString());
	d->SetString("hostApiVersionString", GetSEApiVersionString());

	//
	// Moved as it was, including that it dereferences the analytics
	// manager without checking. The root constructs this class after it,
	// and both die with the container.
	//
	d->SetString("hostMachineUniqueId",
		     m_analyticsEventsManager->identity());
	d->SetString("hostSessionUniqueId",
		     m_analyticsEventsManager->sessionId());

#ifdef WIN32
	d->SetString("platform", "windows");
#elif defined(__APPLE__)
	d->SetString("platform", "macos");
#elif defined(__linux__)
	d->SetString("platform", "linux");
#endif

	if (sizeof(void *) == 8) {
		d->SetString("platformArch", "64bit");
	} else {
		d->SetString("platformArch", "32bit");
	}

	output->SetDictionary(d);
}

void SEHostInfoProvider::SerializeSystemCPUUsageTimes(
	CefRefPtr<CefValue> &output)
{
	SerializeSystemTimes(output);
}

void SEHostInfoProvider::SerializeSystemMemoryUsage(CefRefPtr<CefValue> &output)
{
	::SerializeSystemMemoryUsage(output);
}

void SEHostInfoProvider::SerializeSystemHardwareProperties(
	CefRefPtr<CefValue> &output)
{
	::SerializeSystemHardwareProperties(output);
}

void SEHostInfoProvider::SerializeLoadedHostModules(CefRefPtr<CefValue> &output)
{
	SerializeLoadedObsModules(output);
}
