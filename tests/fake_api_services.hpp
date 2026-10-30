#pragma once

//
// A fully populated SEApiServices, every service standing in for itself, so
// that the 208 host API method bodies in SEApiMessageHandler.cpp can be
// dispatched from a test and asserted on.
//
// What makes this possible is part 2 of CORE-757: that translation unit now
// reaches no obs and no Qt header by any path, so it compiles here. Compiling
// was never the whole price though -- the bodies still CALL things, and the
// linker wants a definition for every one. There are two kinds, and the
// difference is the whole shape of this file:
//
//   * Thirteen services the container carries as pure virtual interfaces.
//     A pure virtual needs no definition at all, so these are faked: an
//     ordinary derived class per interface, below, 137 methods in all.
//
//   * Fifteen services the container still carries concretely, 51 methods
//     between them. These have no interface to derive from, so the test
//     supplies the DEFINITIONS -- see stub_concrete_services.cpp, which is
//     the same link-seam trick api_handler_harness.cpp already uses for
//     blog and __QtPostTask_Impl, just at a larger scale.
//
// Both kinds report into one journal, so a test does not have to care which
// kind it is asserting against. The journal records every call with its
// arguments rendered to JSON, and answers each call with whatever the test
// programmed for it -- which is what makes a fake per test rather than one
// behaviour per binary, the trade-off the plan left open.
//
// A fake is complete because the compiler says so: an unimplemented pure
// virtual leaves the class abstract and FakeApiServices stops compiling. So
// an interface that grows a method breaks the build here rather than
// silently going untested.
//

#include "streamelements/SEApiServices.hpp"

#include "streamelements/ISEAudioCompositionManager.hpp"
#include "streamelements/ISEBandwidthTestManager.hpp"
#include "streamelements/ISEBrowserWidgetManager.hpp"
#include "streamelements/ISEConfig.hpp"
#include "streamelements/ISEExternalSceneDataProviderManager.hpp"
#include "streamelements/ISEHotkeyManager.hpp"
#include "streamelements/ISEMenuManager.hpp"
#include "streamelements/ISEMessageBus.hpp"
#include "streamelements/ISENativeObsUIControlsManager.hpp"
#include "streamelements/ISEObsSceneManager.hpp"
#include "streamelements/ISEPopupWindowManager.hpp"
#include "streamelements/ISESharedVideoCompositionManager.hpp"
#include "streamelements/ISEVideoComposition.hpp"
#include "streamelements/ISEVideoCompositionManager.hpp"
#include "streamelements/ISEVideoCompositionViewHost.hpp"
#include "streamelements/ISEWorkerManager.hpp"
#include "streamelements/SEDockBrowserWidgetInfo.hpp"

#include <cstdint>
#include <future>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace se_fakes {

// --- the journal ------------------------------------------------------

//
// One call, as it happened. Arguments are rendered rather than kept: a
// CefValue becomes its JSON, a string stays a string, a number its digits.
// Rendering is what lets a test assert on an argument without holding a
// reference to a tree the handler may still be mutating.
//
struct Call {
	std::string service;
	std::string method;
	std::vector<std::string> args;

	std::string Qualified() const { return service + "::" + method; }
};

//
// Everything the fakes read and write. One instance, because there is one
// services container per test and the fakes have no state of their own.
//
class Journal {
public:
	std::vector<Call> calls;

	// --- what happened -------------------------------------------

	std::size_t Count(const std::string &qualified) const;

	//
	// The first and last call to that method, or null. Last() is what a
	// test wants when a handler calls the same method more than once.
	//
	const Call *First(const std::string &qualified) const;
	const Call *Last(const std::string &qualified) const;

	//
	// Argument i of the first call, or "" if there was no such call or
	// no such argument. Deliberately not an assertion: a test that cares
	// asserts on Count() first, and gets a readable failure rather than
	// an out-of-range crash.
	//
	std::string Arg(const std::string &qualified, std::size_t i = 0) const;

	//
	// Every method called, in order, as "Service::Method". The cheapest
	// assertion for "this body talks to exactly these services".
	//
	std::vector<std::string> Sequence() const;

	// --- what the fakes answer -----------------------------------
	//
	// Nothing is programmed by default, and the defaults are the quiet
	// ones: an out-parameter is left exactly as the handler set it, a
	// bool is false, a string empty, a number zero, a dictionary empty
	// and a composition null. A test that cares about a return value
	// says so.
	//

	void SetJson(const std::string &qualified, const std::string &json);
	void SetValue(const std::string &qualified, CefRefPtr<CefValue> value);
	void SetBool(const std::string &qualified, bool value);

	//
	// A different answer each call, front first, falling back to SetBool
	// once the sequence runs out. Two bodies drain a service in a
	// `while (service->Method())` loop, and a single programmed answer
	// can only make that loop run zero times or for ever -- so the loop
	// itself was unreachable until this existed.
	//
	void SetBools(const std::string &qualified,
		      const std::vector<bool> &values);
	void SetString(const std::string &qualified, const std::string &value);
	void SetInt(const std::string &qualified, int64_t value);
	void SetDict(const std::string &qualified,
		     CefRefPtr<CefDictionaryValue> value);
	void SetStrings(const std::string &qualified,
			const std::vector<std::string> &value);
	void SetDockInfo(const std::string &qualified,
			 const SEDockBrowserWidgetInfo &value);
	void SetComposition(const std::string &qualified,
			    std::shared_ptr<ISEVideoComposition> value);

	//
	// The promise showNonModalDialog parks its detached thread on. A
	// test that programs one decides WHEN the page's call completes,
	// which is the only way to tell "has not answered yet" from "never
	// answers".
	//
	void
	SetPromise(const std::string &qualified,
		   std::shared_ptr<std::promise<CefRefPtr<CefValue>>> value);

	void Reset();

public:
	std::map<std::string, CefRefPtr<CefValue>> values;
	std::map<std::string, bool> bools;
	std::map<std::string, std::vector<bool>> boolSequences;
	std::map<std::string, std::shared_ptr<std::promise<CefRefPtr<CefValue>>>>
		promises;
	std::map<std::string, std::string> strings;
	std::map<std::string, int64_t> ints;
	std::map<std::string, CefRefPtr<CefDictionaryValue>> dicts;
	std::map<std::string, std::vector<std::string>> stringLists;
	std::map<std::string, SEDockBrowserWidgetInfo> dockInfos;
	std::map<std::string, std::shared_ptr<ISEVideoComposition>> compositions;
};

Journal &journal();

// --- argument rendering -----------------------------------------------

std::string RenderArg(CefRefPtr<CefValue> value);
std::string RenderArg(const char *value);
std::string RenderArg(const std::string &value);
std::string RenderArg(bool value);
std::string RenderArg(int value);
std::string RenderArg(unsigned int value);
std::string RenderArg(long long value);
std::string RenderArg(unsigned long long value);

// --- the primitives both kinds of stand-in use ------------------------

void RecordArgs(const std::string &service, const std::string &method,
		std::vector<std::string> args);

template<typename... Args>
void Record(const std::string &service, const std::string &method,
	    const Args &...args)
{
	RecordArgs(service, method, {RenderArg(args)...});
}

//
// Each leaves its target untouched when the test programmed nothing, which
// is why they are Fill rather than Get: a handler that pre-sets its result
// and hands it to a service keeps what it set.
//
void FillValue(const std::string &service, const std::string &method,
	       CefRefPtr<CefValue> &output);

//
// The same, for a service that takes its output by VALUE rather than by
// reference. Four of them do, which means they cannot replace the handle
// and can only mutate what it points at -- so this mutates in place, as
// the real service does. Replacing the handle here would write into a
// local and the handler would never see it.
//
void FillValueInPlace(const std::string &service, const std::string &method,
		      CefRefPtr<CefValue> output);
void FillStrings(const std::string &service, const std::string &method,
		 std::vector<std::string> &output);
void FillDockInfo(const std::string &service, const std::string &method,
		  SEDockBrowserWidgetInfo &output);

bool BoolFor(const std::string &service, const std::string &method);
std::string StringFor(const std::string &service, const std::string &method);
int64_t IntFor(const std::string &service, const std::string &method);
CefRefPtr<CefValue> ValueFor(const std::string &service,
			     const std::string &method);
CefRefPtr<CefDictionaryValue> DictFor(const std::string &service,
				      const std::string &method);
std::shared_ptr<ISEVideoComposition> CompositionFor(const std::string &service,
						    const std::string &method);
std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
PromiseFor(const std::string &service, const std::string &method);

// --- the base every interface fake shares -----------------------------

class FakeService {
public:
	explicit FakeService(std::string serviceName)
		: m_serviceName(std::move(serviceName))
	{
	}

	virtual ~FakeService() {}

	const std::string &ServiceName() const { return m_serviceName; }

protected:
	template<typename... Args>
	void Record(const char *method, const Args &...args) const
	{
		se_fakes::RecordArgs(m_serviceName, method,
				     {se_fakes::RenderArg(args)...});
	}

	void FillValue(const char *method, CefRefPtr<CefValue> &output) const
	{
		se_fakes::FillValue(m_serviceName, method, output);
	}

	void FillValueInPlace(const char *method,
			      CefRefPtr<CefValue> output) const
	{
		se_fakes::FillValueInPlace(m_serviceName, method, output);
	}

	void FillStrings(const char *method,
			 std::vector<std::string> &output) const
	{
		se_fakes::FillStrings(m_serviceName, method, output);
	}

	void FillDockInfo(const char *method,
			  SEDockBrowserWidgetInfo &output) const
	{
		se_fakes::FillDockInfo(m_serviceName, method, output);
	}

	bool BoolFor(const char *method) const
	{
		return se_fakes::BoolFor(m_serviceName, method);
	}

	std::string StringFor(const char *method) const
	{
		return se_fakes::StringFor(m_serviceName, method);
	}

	int64_t IntFor(const char *method) const
	{
		return se_fakes::IntFor(m_serviceName, method);
	}

	CefRefPtr<CefValue> ValueFor(const char *method) const
	{
		return se_fakes::ValueFor(m_serviceName, method);
	}

	CefRefPtr<CefDictionaryValue> DictFor(const char *method) const
	{
		return se_fakes::DictFor(m_serviceName, method);
	}

	std::shared_ptr<ISEVideoComposition>
	CompositionFor(const char *method) const
	{
		return se_fakes::CompositionFor(m_serviceName, method);
	}

	//
	// Record and answer in one expression, for the majority shape: a
	// method whose whole contract is its return value.
	//
	template<typename... Args>
	bool RespondBool(const char *method, const Args &...args) const
	{
		Record(method, args...);
		return BoolFor(method);
	}

	template<typename... Args>
	std::string RespondString(const char *method, const Args &...args) const
	{
		Record(method, args...);
		return StringFor(method);
	}

	template<typename... Args>
	int64_t RespondInt(const char *method, const Args &...args) const
	{
		Record(method, args...);
		return IntFor(method);
	}

	template<typename... Args>
	CefRefPtr<CefValue> RespondValue(const char *method,
					 const Args &...args) const
	{
		Record(method, args...);
		return ValueFor(method);
	}

	template<typename... Args>
	CefRefPtr<CefDictionaryValue> RespondDict(const char *method,
						  const Args &...args) const
	{
		Record(method, args...);
		return DictFor(method);
	}

	template<typename... Args>
	std::shared_ptr<ISEVideoComposition>
	RespondComposition(const char *method, const Args &...args) const
	{
		Record(method, args...);
		return CompositionFor(method);
	}

private:
	std::string m_serviceName;
};

// --- the sixteen interface fakes --------------------------------------
//
// Generated once from the ISE* headers and checked in; ordinary source
// afterwards. Each method records itself and then answers from the
// journal, so the whole of a fake's behaviour is what the test programs.
//

//
// ISEAudioCompositionManager -- 4 method(s).
//
class FakeAudioCompositionManager : public ISEAudioCompositionManager,
				    public FakeService {
public:
	FakeAudioCompositionManager()
		: FakeService("ISEAudioCompositionManager")
	{
	}

	void SerializeAllCompositions(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAllCompositions");
		FillValue("SerializeAllCompositions", output);
	}

	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeComposition", input);
		FillValue("DeserializeComposition", output);
	}

	void DeserializeExistingCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeExistingCompositionProperties", input);
		FillValue("DeserializeExistingCompositionProperties", output);
	}

	void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) override
	{
		Record("RemoveCompositionsByIds", input);
		FillValue("RemoveCompositionsByIds", output);
	}
};

//
// ISEBandwidthTestManager -- 3 method(s).
//
class FakeBandwidthTestManager : public ISEBandwidthTestManager,
				 public FakeService {
public:
	FakeBandwidthTestManager() : FakeService("ISEBandwidthTestManager") {}

	bool BeginBandwidthTest(CefRefPtr<CefValue> settingsValue,
				CefRefPtr<CefValue> serversValue,
				std::string target) override
	{
		return RespondBool("BeginBandwidthTest", settingsValue,
				   serversValue, target);
	}

	CefRefPtr<CefDictionaryValue>
	EndBandwidthTest(CefRefPtr<CefValue> options) override
	{
		return RespondDict("EndBandwidthTest", options);
	}

	CefRefPtr<CefDictionaryValue> GetBandwidthTestStatus() override
	{
		return RespondDict("GetBandwidthTestStatus");
	}
};

//
// ISEBrowserWidgetManager -- 25 method(s).
//
class FakeBrowserWidgetManager : public ISEBrowserWidgetManager,
				 public FakeService {
public:
	FakeBrowserWidgetManager() : FakeService("ISEBrowserWidgetManager") {}

	void PushCentralBrowserWidget(
		const char *const url,
		const char *const executeJavaScriptCodeOnLoad) override
	{
		Record("PushCentralBrowserWidget", url,
		       executeJavaScriptCodeOnLoad);
	}

	bool DestroyCurrentCentralBrowserWidget() override
	{
		return RespondBool("DestroyCurrentCentralBrowserWidget");
	}

	bool HasCentralBrowserWidget() override
	{
		return RespondBool("HasCentralBrowserWidget");
	}

	std::string AddDockBrowserWidget(CefRefPtr<CefValue> input,
					 std::string requestId) override
	{
		return RespondString("AddDockBrowserWidget", input, requestId);
	}

	bool ToggleWidgetFloatingStateById(const char *const id) override
	{
		return RespondBool("ToggleWidgetFloatingStateById", id);
	}

	bool SetWidgetDimensionsById(const char *const id, const int width,
				     const int height) override
	{
		return RespondBool("SetWidgetDimensionsById", id, width,
				   height);
	}

	bool SetWidgetPositionById(const char *const id, const int left,
				   const int top) override
	{
		return RespondBool("SetWidgetPositionById", id, left, top);
	}

	bool SetWidgetUrlById(const char *const id,
			      const char *const url) override
	{
		return RespondBool("SetWidgetUrlById", id, url);
	}

	bool SetWidgetTitleById(const char *const id,
				const char *const title) override
	{
		return RespondBool("SetWidgetTitleById", id, title);
	}

	bool ShowDockWidgetById(const char *const id) override
	{
		return RespondBool("ShowDockWidgetById", id);
	}

	bool HideDockWidgetById(const char *const id) override
	{
		return RespondBool("HideDockWidgetById", id);
	}

	bool RemoveDockWidget(const char *const id) override
	{
		return RespondBool("RemoveDockWidget", id);
	}

	void RemoveAllDockWidgets() override { Record("RemoveAllDockWidgets"); }

	void GetDockBrowserWidgetIdentifiers(
		std::vector<std::string> &result) override
	{
		Record("GetDockBrowserWidgetIdentifiers");
		FillStrings("GetDockBrowserWidgetIdentifiers", result);
	}

	bool GetDockBrowserWidgetInfo(const char *const id,
				      SEDockBrowserWidgetInfo &output) override
	{
		Record("GetDockBrowserWidgetInfo", id);
		FillDockInfo("GetDockBrowserWidgetInfo", output);
		return BoolFor("GetDockBrowserWidgetInfo");
	}

	QDockWidget *GetDockWidget(const char *const id) override
	{
		Record("GetDockWidget", id);
		return nullptr;
	}

	bool HasDockWidget(const char *const id) override
	{
		return RespondBool("HasDockWidget", id);
	}

	void SerializeDockingWidgets(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeDockingWidgets");
		FillValue("SerializeDockingWidgets", output);
	}

	bool GroupDockingWidgetPairByIds(const char *firstId,
					 const char *secondId) override
	{
		return RespondBool("GroupDockingWidgetPairByIds", firstId,
				   secondId);
	}

	bool InsertDockingWidgetRelativeToId(const char *firstId,
					     const char *secondId,
					     const bool isBefore) override
	{
		return RespondBool("InsertDockingWidgetRelativeToId", firstId,
				   secondId, isBefore);
	}

	//
	// By reference, but an input: the production signature takes
	// `CefRefPtr<CefValue> &` and reads it. Recording it rather than
	// overwriting it is what lets showNotificationBar's test assert
	// that the page's bar description arrived intact.
	//
	void DeserializeNotificationBar(CefRefPtr<CefValue> &input) override
	{
		Record("DeserializeNotificationBar", input);
	}

	void HideNotificationBar() override { Record("HideNotificationBar"); }

	bool HasNotificationBar() override
	{
		return RespondBool("HasNotificationBar");
	}

	void EnterCriticalSection() override { Record("EnterCriticalSection"); }

	void LeaveCriticalSection() override { Record("LeaveCriticalSection"); }
};

//
// ISEConfig -- 7 method(s).
//
class FakeConfig : public ISEConfig, public FakeService {
public:
	FakeConfig() : FakeService("ISEConfig") {}

	int GetStartupFlags() override { return RespondInt("GetStartupFlags"); }

	void SetStartupFlags(int value) override
	{
		Record("SetStartupFlags", value);
	}

	std::string GetUrlOnBoarding() override
	{
		return RespondString("GetUrlOnBoarding");
	}

	void ReadScopedJsonFile(CefRefPtr<CefValue> input,
				CefRefPtr<CefValue> &output) override
	{
		Record("ReadScopedJsonFile", input);
		FillValue("ReadScopedJsonFile", output);
	}

	void WriteScopedJsonFile(CefRefPtr<CefValue> input,
				 CefRefPtr<CefValue> &output) override
	{
		Record("WriteScopedJsonFile", input);
		FillValue("WriteScopedJsonFile", output);
	}

	void ReadScopedJsonFilesList(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) override
	{
		Record("ReadScopedJsonFilesList", input);
		FillValue("ReadScopedJsonFilesList", output);
	}

	void RemoveScopedJsonFile(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output) override
	{
		Record("RemoveScopedJsonFile", input);
		FillValue("RemoveScopedJsonFile", output);
	}
};

//
// ISEExternalSceneDataProviderManager -- 3 method(s).
//
class FakeExternalSceneDataProviderManager
	: public ISEExternalSceneDataProviderManager,
	  public FakeService {
public:
	FakeExternalSceneDataProviderManager()
		: FakeService("ISEExternalSceneDataProviderManager")
	{
	}

	//
	// All three take the result by VALUE, so they answer by mutating
	// what it points at. FillValueInPlace does the same, which is what
	// makes these three handlers' replies assertable at all.
	//
	void SerializeProviders(CefRefPtr<CefValue> result) override
	{
		Record("SerializeProviders");
		FillValueInPlace("SerializeProviders", result);
	}

	bool
	SerializeProviderSceneCollections(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> result) override
	{
		Record("SerializeProviderSceneCollections", input);
		FillValueInPlace("SerializeProviderSceneCollections", result);

		return BoolFor("SerializeProviderSceneCollections");
	}

	bool
	SerializeProviderSceneColletion(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> result) override
	{
		Record("SerializeProviderSceneColletion", input);
		FillValueInPlace("SerializeProviderSceneColletion", result);

		return BoolFor("SerializeProviderSceneColletion");
	}
};

//
// ISEHotkeyManager -- 5 method(s).
//
class FakeHotkeyManager : public ISEHotkeyManager, public FakeService {
public:
	FakeHotkeyManager() : FakeService("ISEHotkeyManager") {}

	bool DeserializeHotkeyTriggers(CefRefPtr<CefValue> input) override
	{
		return RespondBool("DeserializeHotkeyTriggers", input);
	}

	int64_t
	DeserializeSingleHotkeyBinding(CefRefPtr<CefValue> input) override
	{
		return RespondInt("DeserializeSingleHotkeyBinding", input);
	}

	bool RemoveHotkeyBindingById(uint64_t id) override
	{
		return RespondBool("RemoveHotkeyBindingById", id);
	}

	void RemoveAllManagedHotkeyBindings() override
	{
		Record("RemoveAllManagedHotkeyBindings");
	}

	bool SerializeHotkeyBindings(CefRefPtr<CefValue> &result,
				     bool onlyManagedBindings) override
	{
		Record("SerializeHotkeyBindings", onlyManagedBindings);
		FillValue("SerializeHotkeyBindings", result);
		return BoolFor("SerializeHotkeyBindings");
	}
};

//
// ISEMenuManager -- 6 method(s).
//
class FakeMenuManager : public ISEMenuManager, public FakeService {
public:
	FakeMenuManager() : FakeService("ISEMenuManager") {}

	void Update() override { Record("Update"); }

	bool DeserializeAuxiliaryMenuItems(CefRefPtr<CefValue> input) override
	{
		return RespondBool("DeserializeAuxiliaryMenuItems", input);
	}

	void SerializeAuxiliaryMenuItems(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAuxiliaryMenuItems");
		FillValue("SerializeAuxiliaryMenuItems", output);
	}

	void SetShowBuiltInMenuItems(bool show) override
	{
		Record("SetShowBuiltInMenuItems", show);
	}

	bool GetShowBuiltInMenuItems() override
	{
		return RespondBool("GetShowBuiltInMenuItems");
	}

	void Reset() override { Record("Reset"); }
};

//
// ISEMessageBus -- 8 method(s).
//
class FakeMessageBus : public ISEMessageBus, public FakeService {
public:
	FakeMessageBus() : FakeService("ISEMessageBus") {}

	void AddListener(std::string target,
			 message_destination_filter_flags_t type) override
	{
		Record("AddListener", target, type);
	}

	void DeserializeBrowserHttpServer(std::string target,
					  CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeBrowserHttpServer", target, input);
		FillValue("DeserializeBrowserHttpServer", output);
	}

	void SerializeBrowserHttpServers(std::string target,
					 CefRefPtr<CefValue> &output) override
	{
		Record("SerializeBrowserHttpServers", target);
		FillValue("SerializeBrowserHttpServers", output);
	}

	void RemoveBrowserHttpServersByIds(std::string target,
					   CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("RemoveBrowserHttpServersByIds", target, input);
		FillValue("RemoveBrowserHttpServersByIds", output);
	}

	void
	DeserializeHttpRequestResponse(CefRefPtr<CefValue> idInput,
				       CefRefPtr<CefValue> responseInput,
				       CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeHttpRequestResponse", idInput,
		       responseInput);
		FillValue("DeserializeHttpRequestResponse", output);
	}

	void NotifyAllLocalEventListeners(
		message_destination_filter_flags_t types, std::string source,
		std::string sourceAddress, std::string event,
		CefRefPtr<CefValue> payload) override
	{
		Record("NotifyAllLocalEventListeners", types, source,
		       sourceAddress, event, payload);
	}

	void NotifyAllExternalEventListeners(
		message_destination_filter_flags_t types, std::string source,
		std::string sourceAddress, std::string event,
		CefRefPtr<CefValue> payload) override
	{
		Record("NotifyAllExternalEventListeners", types, source,
		       sourceAddress, event, payload);
	}

	void NotifyAllMessageListeners(message_destination_filter_flags_t types,
				       std::string source,
				       std::string sourceAddress,
				       CefRefPtr<CefValue> payload) override
	{
		Record("NotifyAllMessageListeners", types, source,
		       sourceAddress, payload);
	}
};

//
// ISENativeObsUIControlsManager -- 10 method(s).
//
class FakeNativeObsUIControlsManager : public ISENativeObsUIControlsManager,
				       public FakeService {
public:
	FakeNativeObsUIControlsManager()
		: FakeService("ISENativeObsUIControlsManager")
	{
	}

	bool DeserializeStartStreamingUIHandlerProperties(
		CefRefPtr<CefValue> input) override
	{
		return RespondBool(
			"DeserializeStartStreamingUIHandlerProperties", input);
	}

	void AdviseRequestStartStreamingAccepted() override
	{
		Record("AdviseRequestStartStreamingAccepted");
	}

	void AdviseRequestStartStreamingRejected() override
	{
		Record("AdviseRequestStartStreamingRejected");
	}

	void StartStreaming() override { Record("StartStreaming"); }

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	bool DeserializePreviewTitleBar(CefRefPtr<CefValue> input) override
	{
		return RespondBool("DeserializePreviewTitleBar", input);
	}

	void HidePreviewTitleBar() override { Record("HidePreviewTitleBar"); }

	bool DeserializePreviewFrame(CefRefPtr<CefValue> input) override
	{
		return RespondBool("DeserializePreviewFrame", input);
	}

	void HidePreviewFrame() override { Record("HidePreviewFrame"); }

#endif
	bool GetNativeManageBroadcastButtonVisibility() override
	{
		return RespondBool("GetNativeManageBroadcastButtonVisibility");
	}

	void Reset() override { Record("Reset"); }
};

//
// ISEObsSceneManager -- 37 method(s).
//
class FakeObsSceneManager : public ISEObsSceneManager, public FakeService {
public:
	FakeObsSceneManager() : FakeService("ISEObsSceneManager") {}

	void Reset() override { Record("Reset"); }

	void SerializeInputSourceClasses(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeInputSourceClasses");
		FillValue("SerializeInputSourceClasses", output);
	}

	void
	SerializeSourceClassProperties(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output) override
	{
		Record("SerializeSourceClassProperties", input);
		FillValue("SerializeSourceClassProperties", output);
	}

	void DeserializeObsBrowserSource(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsBrowserSource", input);
		FillValue("DeserializeObsBrowserSource", output);
	}

	void
	DeserializeObsGameCaptureSource(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsGameCaptureSource", input);
		FillValue("DeserializeObsGameCaptureSource", output);
	}

	void
	DeserializeObsVideoCaptureSource(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsVideoCaptureSource", input);
		FillValue("DeserializeObsVideoCaptureSource", output);
	}

	void DeserializeObsNativeSource(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsNativeSource", input);
		FillValue("DeserializeObsNativeSource", output);
	}

	void DeserializeObsSceneItemGroup(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsSceneItemGroup", input);
		FillValue("DeserializeObsSceneItemGroup", output);
	}

	void SerializeObsSceneItems(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output,
				    bool serializeProperties) override
	{
		Record("SerializeObsSceneItems", input, serializeProperties);
		FillValue("SerializeObsSceneItems", output);
	}

	void RemoveObsSceneItemsByIds(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) override
	{
		Record("RemoveObsSceneItemsByIds", input);
		FillValue("RemoveObsSceneItemsByIds", output);
	}

	void SetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("SetObsSceneItemPropertiesById", input);
		FillValue("SetObsSceneItemPropertiesById", output);
	}

	void GetObsSceneItemPropertiesById(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("GetObsSceneItemPropertiesById", input);
		FillValue("GetObsSceneItemPropertiesById", output);
	}

	void UngroupObsSceneItemsByGroupId(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("UngroupObsSceneItemsByGroupId", input);
		FillValue("UngroupObsSceneItemsByGroupId", output);
	}

	void InvokeCurrentSceneItemDefaultActionById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("InvokeCurrentSceneItemDefaultActionById", input);
		FillValue("InvokeCurrentSceneItemDefaultActionById", output);
	}

	void InvokeCurrentSceneItemDefaultContextMenuById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("InvokeCurrentSceneItemDefaultContextMenuById", input);
		FillValue("InvokeCurrentSceneItemDefaultContextMenuById",
			  output);
	}

	void DeserializeSceneItemsAuxiliaryActions(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeSceneItemsAuxiliaryActions", input);
		FillValue("DeserializeSceneItemsAuxiliaryActions", output);
	}

	void SerializeSceneItemsAuxiliaryActions(
		CefRefPtr<CefValue> &output) override
	{
		Record("SerializeSceneItemsAuxiliaryActions");
		FillValue("SerializeSceneItemsAuxiliaryActions", output);
	}

	void SerializeObsScenes(CefRefPtr<CefValue> input,
				CefRefPtr<CefValue> &output) override
	{
		Record("SerializeObsScenes", input);
		FillValue("SerializeObsScenes", output);
	}

	void SerializeObsCurrentScene(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) override
	{
		Record("SerializeObsCurrentScene", input);
		FillValue("SerializeObsCurrentScene", output);
	}

	void DeserializeObsScene(CefRefPtr<CefValue> input,
				 CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsScene", input);
		FillValue("DeserializeObsScene", output);
	}

	void SetCurrentObsSceneById(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) override
	{
		Record("SetCurrentObsSceneById", input);
		FillValue("SetCurrentObsSceneById", output);
	}

	void RemoveObsScenesByIds(CefRefPtr<CefValue> input,
				  CefRefPtr<CefValue> &output) override
	{
		Record("RemoveObsScenesByIds", input);
		FillValue("RemoveObsScenesByIds", output);
	}

	void SetObsScenePropertiesById(CefRefPtr<CefValue> input,
				       CefRefPtr<CefValue> &output) override
	{
		Record("SetObsScenePropertiesById", input);
		FillValue("SetObsScenePropertiesById", output);
	}

	void
	DeserializeScenesAuxiliaryActions(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeScenesAuxiliaryActions", input);
		FillValue("DeserializeScenesAuxiliaryActions", output);
	}

	void
	SerializeScenesAuxiliaryActions(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeScenesAuxiliaryActions");
		FillValue("SerializeScenesAuxiliaryActions", output);
	}

	void SerializeObsSceneCollections(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeObsSceneCollections");
		FillValue("SerializeObsSceneCollections", output);
	}

	void
	SerializeObsCurrentSceneCollection(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeObsCurrentSceneCollection");
		FillValue("SerializeObsCurrentSceneCollection", output);
	}

	void DeserializeObsSceneCollection(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsSceneCollection", input);
		FillValue("DeserializeObsSceneCollection", output);
	}

	void DeserializeObsCurrentSceneCollectionById(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeObsCurrentSceneCollectionById", input);
		FillValue("DeserializeObsCurrentSceneCollectionById", output);
	}

	void OpenSceneItemPropertiesById(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) override
	{
		Record("OpenSceneItemPropertiesById", input);
		FillValue("OpenSceneItemPropertiesById", output);
	}

	void OpenSceneItemFiltersById(CefRefPtr<CefValue> input,
				      CefRefPtr<CefValue> &output) override
	{
		Record("OpenSceneItemFiltersById", input);
		FillValue("OpenSceneItemFiltersById", output);
	}

	void OpenSceneItemInteractionById(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) override
	{
		Record("OpenSceneItemInteractionById", input);
		FillValue("OpenSceneItemInteractionById", output);
	}

	void
	OpenSceneItemTransformEditorById(CefRefPtr<CefValue> input,
					 CefRefPtr<CefValue> &output) override
	{
		Record("OpenSceneItemTransformEditorById", input);
		FillValue("OpenSceneItemTransformEditorById", output);
	}

	void
	SerializeSceneItemViewportRotation(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("SerializeSceneItemViewportRotation", input);
		FillValue("SerializeSceneItemViewportRotation", output);
	}

	void DeserializeSceneItemViewportRotation(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeSceneItemViewportRotation", input);
		FillValue("DeserializeSceneItemViewportRotation", output);
	}

	void SerializeSceneItemViewportBoundingRectangle(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("SerializeSceneItemViewportBoundingRectangle", input);
		FillValue("SerializeSceneItemViewportBoundingRectangle",
			  output);
	}

	void DeserializeSceneItemViewportPosition(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeSceneItemViewportPosition", input);
		FillValue("DeserializeSceneItemViewportPosition", output);
	}
};

//
// ISEPopupWindowManager -- 1 method(s).
//
class FakePopupWindowManager : public ISEPopupWindowManager,
			       public FakeService {
public:
	FakePopupWindowManager() : FakeService("ISEPopupWindowManager") {}

	bool DeserializePopupWindow(CefRefPtr<CefValue> input) override
	{
		return RespondBool("DeserializePopupWindow", input);
	}
};

//
// ISESharedVideoCompositionManager -- 7 method(s).
//
class FakeSharedVideoCompositionManager
	: public ISESharedVideoCompositionManager,
	  public FakeService {
public:
	FakeSharedVideoCompositionManager()
		: FakeService("ISESharedVideoCompositionManager")
	{
	}

	void SerializeAllSharedVideoCompositions(
		CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAllSharedVideoCompositions");
		FillValue("SerializeAllSharedVideoCompositions", output);
	}

	void
	DeserializeSharedVideoComposition(CefRefPtr<CefValue> input,
					  CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeSharedVideoComposition", input);
		FillValue("DeserializeSharedVideoComposition", output);
	}

	void DeserializeSharedVideoCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeSharedVideoCompositionProperties", input);
		FillValue("DeserializeSharedVideoCompositionProperties",
			  output);
	}

	void
	RemoveSharedVideoCompositionsByIds(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output) override
	{
		Record("RemoveSharedVideoCompositionsByIds", input);
		FillValue("RemoveSharedVideoCompositionsByIds", output);
	}

	void ConnectVideoCompositionToSharedVideoComposition(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("ConnectVideoCompositionToSharedVideoComposition",
		       input);
		FillValue("ConnectVideoCompositionToSharedVideoComposition",
			  output);
	}

	void DisconnectVideoCompositionsFromSharedVideoCompositionsByIds(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DisconnectVideoCompositionsFromSharedVideoCompositionsByIds",
		       input);
		FillValue(
			"DisconnectVideoCompositionsFromSharedVideoCompositionsByIds",
			output);
	}

	void Reset() override { Record("Reset"); }
};

//
// ISEVideoComposition -- 4 method(s).
//
class FakeVideoComposition : public ISEVideoComposition, public FakeService {
public:
	FakeVideoComposition() : FakeService("ISEVideoComposition") {}

	void TakeScreenshot() override { Record("TakeScreenshot"); }

	void SerializeTransition(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeTransition");
		FillValue("SerializeTransition", output);
	}

	void DeserializeTransition(CefRefPtr<CefValue> input,
				   CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeTransition", input);
		FillValue("DeserializeTransition", output);
	}

	bool ShowTransitionPropertiesDialog() override
	{
		return RespondBool("ShowTransitionPropertiesDialog");
	}
};

//
// ISEVideoCompositionManager -- 13 method(s).
//
class FakeVideoCompositionManager : public ISEVideoCompositionManager,
				    public FakeService {
public:
	FakeVideoCompositionManager()
		: FakeService("ISEVideoCompositionManager")
	{
	}

	void SerializeAllCompositions(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAllCompositions");
		FillValue("SerializeAllCompositions", output);
	}

	void DeserializeComposition(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeComposition", input);
		FillValue("DeserializeComposition", output);
	}

	void DeserializeExistingCompositionProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeExistingCompositionProperties", input);
		FillValue("DeserializeExistingCompositionProperties", output);
	}

	void RemoveCompositionsByIds(CefRefPtr<CefValue> input,
				     CefRefPtr<CefValue> &output) override
	{
		Record("RemoveCompositionsByIds", input);
		FillValue("RemoveCompositionsByIds", output);
	}

	void SerializeAvailableVideoEncoderClasses(
		CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAvailableVideoEncoderClasses");
		FillValue("SerializeAvailableVideoEncoderClasses", output);
	}

	void SerializeAvailableAudioEncoderClasses(
		CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAvailableAudioEncoderClasses");
		FillValue("SerializeAvailableAudioEncoderClasses", output);
	}

	void SerializeAvailableVideoEncoderClassProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAvailableVideoEncoderClassProperties", input);
		FillValue("SerializeAvailableVideoEncoderClassProperties",
			  output);
	}

	void SerializeAvailableAudioEncoderClassProperties(
		CefRefPtr<CefValue> input, CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAvailableAudioEncoderClassProperties", input);
		FillValue("SerializeAvailableAudioEncoderClassProperties",
			  output);
	}

	void SerializeAvailableTransitionClasses(
		CefRefPtr<CefValue> &output) override
	{
		Record("SerializeAvailableTransitionClasses");
		FillValue("SerializeAvailableTransitionClasses", output);
	}

	std::shared_ptr<ISEVideoComposition>
	GetVideoComposition(CefRefPtr<CefValue> input) override
	{
		return RespondComposition("GetVideoComposition", input);
	}

	std::shared_ptr<ISEVideoComposition>
	GetVideoComposition(std::string id) override
	{
		return RespondComposition("GetVideoComposition", id);
	}

	std::shared_ptr<ISEVideoComposition>
	GetNativeVideoComposition() override
	{
		return RespondComposition("GetNativeVideoComposition");
	}

	void Reset() override { Record("Reset"); }
};

//
// ISEVideoCompositionViewHost -- 3 method(s).
//
class FakeVideoCompositionViewHost : public ISEVideoCompositionViewHost,
				     public FakeService {
public:
	FakeVideoCompositionViewHost()
		: FakeService("ISEVideoCompositionViewHost")
	{
	}

	void RemoveVideoCompositionView() override
	{
		Record("RemoveVideoCompositionView");
	}

	void SerializeVideoCompositionView(CefRefPtr<CefValue> &output) override
	{
		Record("SerializeVideoCompositionView");
		FillValue("SerializeVideoCompositionView", output);
	}

	void
	DeserializeVideoCompositionView(CefRefPtr<CefValue> input,
					CefRefPtr<CefValue> &output) override
	{
		Record("DeserializeVideoCompositionView", input);
		FillValue("DeserializeVideoCompositionView", output);
	}
};

//
// ISEWorkerManager -- 5 method(s).
//
class FakeWorkerManager : public ISEWorkerManager, public FakeService {
public:
	FakeWorkerManager() : FakeService("ISEWorkerManager") {}

	std::string DeserializeOne(CefRefPtr<CefValue> input) override
	{
		return RespondString("DeserializeOne", input);
	}

	void Remove(std::string id) override { Record("Remove", id); }

	void Serialize(CefRefPtr<CefValue> &output) override
	{
		Record("Serialize");
		FillValue("Serialize", output);
	}

	void RemoveAll() override { Record("RemoveAll"); }

	void GetIdentifiers(std::vector<std::string> &result) override
	{
		Record("GetIdentifiers");
		FillStrings("GetIdentifiers", result);
	}
};
// --- the container ----------------------------------------------------

//
// Every field SEApiServices carries, populated. The fakes are held by their
// own types as well as through the container, so a test can program one
// without casting back down.
//
// The fifteen concrete services are constructed with null dependencies --
// every one of their constructor parameters is a pointer or a shared_ptr to
// a forward-declared type, and the stub constructors in
// stub_concrete_services.cpp do not look at them. Nothing in a handler body
// reaches a concrete service's own members, because the stubs are the only
// bodies that exist here.
//
struct FakeApiServices {
	std::shared_ptr<SEApiServices> services;

	std::shared_ptr<FakeAudioCompositionManager> audioCompositionManager;
	std::shared_ptr<FakeBandwidthTestManager> bandwidthTestManager;
	std::shared_ptr<FakeBrowserWidgetManager> widgetManager;
	std::shared_ptr<FakeConfig> config;
	std::shared_ptr<FakeExternalSceneDataProviderManager>
		externalSceneDataProviderManager;
	std::shared_ptr<FakeHotkeyManager> hotkeyManager;
	std::shared_ptr<FakeMenuManager> menuManager;
	std::shared_ptr<FakeMessageBus> messageBus;
	std::shared_ptr<FakeNativeObsUIControlsManager>
		nativeObsUIControlsManager;
	std::shared_ptr<FakeObsSceneManager> obsSceneManager;
	std::shared_ptr<FakePopupWindowManager> popupWindowManager;
	std::shared_ptr<FakeSharedVideoCompositionManager>
		sharedVideoCompositionManager;
	std::shared_ptr<FakeVideoCompositionManager> videoCompositionManager;
	std::shared_ptr<FakeWorkerManager> workerManager;

	//
	// Not a service: the handler's own browser widget, reached through
	// SEApiMessageHandlerBase::GetBrowserWidget(). Three handler bodies
	// go through it, and it is the interface that took this file to zero
	// obs and zero Qt.
	//
	std::shared_ptr<FakeVideoCompositionViewHost> videoCompositionViewHost;

	//
	// OBS's own composition, which the real manager creates in its
	// constructor and holds for its whole life. It is installed as the
	// answer to GetNativeVideoComposition because that non-nullness is a
	// production invariant, and modelling it as null would be modelling a
	// state that cannot happen: getVideoCompositionTransition's no-argument
	// branch dereferences it without a check, where its by-id branch two
	// lines above checks -- correctly, since a lookup by id can miss.
	//
	std::shared_ptr<FakeVideoComposition> nativeVideoComposition;

	//
	// Constructing one of these RESETS the journal and then installs the
	// invariants above, so a fresh FakeApiServices is a fresh world. A
	// test that programmed something before building one would lose it.
	//
	FakeApiServices();
	~FakeApiServices();
};

} // namespace se_fakes
