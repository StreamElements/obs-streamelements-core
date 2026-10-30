//
// The link seam for SERazerWyvrnManager, needed only by the build that
// defines SE_ENABLE_WYVRN -- see test_api_handler_optional.cpp for why that
// build exists.
//
// It is a concrete class with a clean header (stdlib, its own config types
// and the CefValue model, nothing else), so the same trick as
// stub_concrete_services.cpp applies: the test supplies the definitions and
// they report through the one journal. Its constructor's two parameters are
// forward-declared types, so FakeApiServices passes null for both.
//
// Only what the handler bodies call is defined here. The real class has a
// dedicated SDK thread, a 3.3-second init and a 30 FPS cap, none of which a
// handler can see.
//

#include "fake_api_services.hpp"

#include "streamelements/SERazerWyvrnManager.hpp"

#include <string>

SERazerWyvrnManager::SERazerWyvrnManager(
	std::shared_ptr<SEHostEventDispatcher>,
	std::shared_ptr<SELocalFilesystemHttpServer>)
{
}

SERazerWyvrnManager::~SERazerWyvrnManager() {}

bool SERazerWyvrnManager::SetEventName(const std::string &name)
{
	se_fakes::Record("SERazerWyvrnManager", "SetEventName", name);

	return se_fakes::BoolFor("SERazerWyvrnManager", "SetEventName");
}

std::string SERazerWyvrnManager::ResolveEventId(const std::string &id)
{
	se_fakes::Record("SERazerWyvrnManager", "ResolveEventId", id);

	return se_fakes::StringFor("SERazerWyvrnManager", "ResolveEventId");
}

CefRefPtr<CefValue>
SERazerWyvrnManager::SerializeEvents(const std::string &groupFilter,
				     const std::string &idPrefix,
				     bool components)
{
	se_fakes::Record("SERazerWyvrnManager", "SerializeEvents", groupFilter,
			 idPrefix, components);

	CefRefPtr<CefValue> answer =
		se_fakes::ValueFor("SERazerWyvrnManager", "SerializeEvents");

	return answer ? answer : CefValue::Create();
}

CefRefPtr<CefValue> SERazerWyvrnManager::SerializeStatus()
{
	se_fakes::Record("SERazerWyvrnManager", "SerializeStatus");

	CefRefPtr<CefValue> answer =
		se_fakes::ValueFor("SERazerWyvrnManager", "SerializeStatus");

	return answer ? answer : CefValue::Create();
}
