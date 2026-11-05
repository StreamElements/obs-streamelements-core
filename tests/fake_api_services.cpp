//
// The journal: what the fakes record, and what they answer with.
//
// Separate from the header so the two kinds of stand-in -- the interface
// fakes here and the concrete-service definitions in
// stub_concrete_services.cpp -- share one journal rather than each getting
// its own copy.
//

#include "fake_api_services.hpp"

#include "streamelements/SEAuxiliaryControlsBuilder.hpp"
#include "streamelements/SEBackupManager.hpp"
#include "streamelements/SEBrowserDialogManager.hpp"
#include "streamelements/SEHostInfoProvider.hpp"
#include "streamelements/SEHostStreamingManager.hpp"
#include "streamelements/SEHostUpdateManager.hpp"
#include "streamelements/SEHttpClient.hpp"
#include "streamelements/SEObsApiTransactionManager.hpp"
#include "streamelements/SEOutputManager.hpp"
#include "streamelements/SEOutputSettingsManager.hpp"
#include "streamelements/SEProfilesManager.hpp"
#include "streamelements/SESessionManager.hpp"
#include "streamelements/SEShellServices.hpp"
#include "streamelements/SEStatePersistence.hpp"
#include "streamelements/SEStatusBarMessageManager.hpp"

#include <string>

namespace se_fakes {

Journal &journal()
{
	static Journal j;

	return j;
}

// --- what happened ----------------------------------------------------

std::size_t Journal::Count(const std::string &qualified) const
{
	std::size_t n = 0;

	for (const Call &c : calls) {
		if (c.Qualified() == qualified)
			++n;
	}

	return n;
}

const Call *Journal::First(const std::string &qualified) const
{
	for (const Call &c : calls) {
		if (c.Qualified() == qualified)
			return &c;
	}

	return nullptr;
}

const Call *Journal::Last(const std::string &qualified) const
{
	const Call *found = nullptr;

	for (const Call &c : calls) {
		if (c.Qualified() == qualified)
			found = &c;
	}

	return found;
}

std::string Journal::Arg(const std::string &qualified, std::size_t i) const
{
	const Call *c = First(qualified);

	if (!c || i >= c->args.size())
		return std::string();

	return c->args[i];
}

std::vector<std::string> Journal::Sequence() const
{
	std::vector<std::string> out;

	for (const Call &c : calls)
		out.push_back(c.Qualified());

	return out;
}

// --- what the fakes answer --------------------------------------------

void Journal::SetJson(const std::string &qualified, const std::string &json)
{
	values[qualified] =
		CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);
}

void Journal::SetValue(const std::string &qualified, CefRefPtr<CefValue> value)
{
	values[qualified] = value;
}

void Journal::SetBool(const std::string &qualified, bool value)
{
	bools[qualified] = value;
}

void Journal::SetBools(const std::string &qualified,
		       const std::vector<bool> &values)
{
	boolSequences[qualified] = values;
}

void Journal::SetPromise(
	const std::string &qualified,
	std::shared_ptr<std::promise<CefRefPtr<CefValue>>> value)
{
	promises[qualified] = value;
}

void Journal::SetString(const std::string &qualified, const std::string &value)
{
	strings[qualified] = value;
}

void Journal::SetInt(const std::string &qualified, int64_t value)
{
	ints[qualified] = value;
}

void Journal::SetDict(const std::string &qualified,
		      CefRefPtr<CefDictionaryValue> value)
{
	dicts[qualified] = value;
}

void Journal::SetStrings(const std::string &qualified,
			 const std::vector<std::string> &value)
{
	stringLists[qualified] = value;
}

void Journal::SetDockInfo(const std::string &qualified,
			  const SEDockBrowserWidgetInfo &value)
{
	dockInfos[qualified] = value;
}

void Journal::SetComposition(const std::string &qualified,
			     std::shared_ptr<ISEVideoComposition> value)
{
	compositions[qualified] = value;
}

void Journal::Reset()
{
	calls.clear();
	values.clear();
	bools.clear();
	strings.clear();
	ints.clear();
	dicts.clear();
	stringLists.clear();
	dockInfos.clear();
	compositions.clear();
	boolSequences.clear();
	promises.clear();
}

// --- argument rendering -----------------------------------------------

std::string RenderArg(CefRefPtr<CefValue> value)
{
	//
	// A null handle and a CefValue holding null are different things a
	// handler body can pass, and more than one of them does, so they
	// render differently.
	//
	if (!value)
		return "<nullptr>";

	return CefWriteJSON(value, JSON_WRITER_DEFAULT).ToString();
}

std::string RenderArg(const char *value)
{
	return value ? std::string(value) : std::string("<nullptr>");
}

std::string RenderArg(const std::string &value)
{
	return value;
}

std::string RenderArg(bool value)
{
	return value ? "true" : "false";
}

std::string RenderArg(int value)
{
	return std::to_string(value);
}

std::string RenderArg(unsigned int value)
{
	return std::to_string(value);
}

std::string RenderArg(long long value)
{
	return std::to_string(value);
}

std::string RenderArg(unsigned long long value)
{
	return std::to_string(value);
}

// --- the primitives ---------------------------------------------------

void RecordArgs(const std::string &service, const std::string &method,
		std::vector<std::string> args)
{
	journal().calls.push_back({service, method, std::move(args)});
}

void FillValue(const std::string &service, const std::string &method,
	       CefRefPtr<CefValue> &output)
{
	auto it = journal().values.find(service + "::" + method);

	if (it != journal().values.end())
		output = it->second;
}

void FillValueInPlace(const std::string &service, const std::string &method,
		      CefRefPtr<CefValue> output)
{
	auto it = journal().values.find(service + "::" + method);

	if (it != journal().values.end() && output)
		output->SetValue(it->second);
}

void FillStrings(const std::string &service, const std::string &method,
		 std::vector<std::string> &output)
{
	auto it = journal().stringLists.find(service + "::" + method);

	if (it != journal().stringLists.end())
		output = it->second;
}

void FillDockInfo(const std::string &service, const std::string &method,
		  SEDockBrowserWidgetInfo &output)
{
	auto it = journal().dockInfos.find(service + "::" + method);

	if (it != journal().dockInfos.end())
		output = it->second;
}

bool BoolFor(const std::string &service, const std::string &method)
{
	const std::string qualified = service + "::" + method;

	//
	// A programmed sequence answers first, one entry per call, and the
	// single answer takes over once it is spent. That is what lets a
	// drain loop be asked to run a known number of times.
	//
	auto seq = journal().boolSequences.find(qualified);

	if (seq != journal().boolSequences.end() && !seq->second.empty()) {
		const bool value = seq->second.front();

		seq->second.erase(seq->second.begin());

		return value;
	}

	auto it = journal().bools.find(qualified);

	return it != journal().bools.end() ? it->second : false;
}

std::string StringFor(const std::string &service, const std::string &method)
{
	auto it = journal().strings.find(service + "::" + method);

	return it != journal().strings.end() ? it->second : std::string();
}

int64_t IntFor(const std::string &service, const std::string &method)
{
	auto it = journal().ints.find(service + "::" + method);

	return it != journal().ints.end() ? it->second : 0;
}

CefRefPtr<CefValue> ValueFor(const std::string &service,
			     const std::string &method)
{
	auto it = journal().values.find(service + "::" + method);

	if (it != journal().values.end())
		return it->second;

	//
	// A CefValue holding null rather than a null handle: a handler body
	// that reads the result of an unprogrammed call gets something it
	// can ask the type of, instead of dereferencing nothing.
	//
	return CefValue::Create();
}

CefRefPtr<CefDictionaryValue> DictFor(const std::string &service,
				      const std::string &method)
{
	auto it = journal().dicts.find(service + "::" + method);

	if (it != journal().dicts.end())
		return it->second;

	// Empty rather than null, for the same reason as above.
	return CefDictionaryValue::Create();
}

std::shared_ptr<ISEVideoComposition> CompositionFor(const std::string &service,
						    const std::string &method)
{
	auto it = journal().compositions.find(service + "::" + method);

	return it != journal().compositions.end() ? it->second : nullptr;
}

std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
PromiseFor(const std::string &service, const std::string &method)
{
	auto programmed = journal().promises.find(service + "::" + method);

	if (programmed != journal().promises.end())
		return programmed->second;

	//
	// showNonModalDialog parks a detached thread on the future of this,
	// so an unprogrammed call must still answer something satisfiable --
	// a null promise would hang the test rather than fail it. Satisfied
	// immediately with null, which is what the production manager's own
	// destructor does for a dialog closed without a result.
	//
	auto promise = std::make_shared<std::promise<CefRefPtr<CefValue>>>();

	promise->set_value(CefValue::Create());

	(void)service;
	(void)method;

	return promise;
}

// --- the container ----------------------------------------------------

FakeApiServices::FakeApiServices()
	: services(std::make_shared<SEApiServices>()),
	  audioCompositionManager(
		  std::make_shared<FakeAudioCompositionManager>()),
	  bandwidthTestManager(std::make_shared<FakeBandwidthTestManager>()),
	  widgetManager(std::make_shared<FakeBrowserWidgetManager>()),
	  config(std::make_shared<FakeConfig>()),
	  externalSceneDataProviderManager(
		  std::make_shared<FakeExternalSceneDataProviderManager>()),
	  hotkeyManager(std::make_shared<FakeHotkeyManager>()),
	  menuManager(std::make_shared<FakeMenuManager>()),
	  messageBus(std::make_shared<FakeMessageBus>()),
	  nativeObsUIControlsManager(
		  std::make_shared<FakeNativeObsUIControlsManager>()),
	  obsSceneManager(std::make_shared<FakeObsSceneManager>()),
	  popupWindowManager(std::make_shared<FakePopupWindowManager>()),
	  sharedVideoCompositionManager(
		  std::make_shared<FakeSharedVideoCompositionManager>()),
	  videoCompositionManager(
		  std::make_shared<FakeVideoCompositionManager>()),
	  workerManager(std::make_shared<FakeWorkerManager>()),
	  videoCompositionViewHost(
		  std::make_shared<FakeVideoCompositionViewHost>()),
	  nativeVideoComposition(std::make_shared<FakeVideoComposition>())
{
	//
	// A fresh world: nothing a previous test programmed survives.
	//
	journal().Reset();

	//
	// The one production invariant the fakes have to carry. See the
	// comment on nativeVideoComposition.
	//
	journal().SetComposition(
		"ISEVideoCompositionManager::GetNativeVideoComposition",
		nativeVideoComposition);

	services->ready = true;

	services->audioCompositionManager = audioCompositionManager;
	services->bandwidthTestManager = bandwidthTestManager;
	services->widgetManager = widgetManager;
	services->config = config;
	services->externalSceneDataProviderManager =
		externalSceneDataProviderManager;
	services->hotkeyManager = hotkeyManager;
	services->menuManager = menuManager;
	services->messageBus = messageBus;
	services->nativeObsUIControlsManager = nativeObsUIControlsManager;
	services->obsSceneManager = obsSceneManager;
	services->popupWindowManager = popupWindowManager;
	services->sharedVideoCompositionManager = sharedVideoCompositionManager;
	services->videoCompositionManager = videoCompositionManager;
	services->workerManager = workerManager;

	//
	// The fifteen the container still carries concretely. Their methods
	// are defined in stub_concrete_services.cpp, including the
	// constructors reached here, so every dependency can be null.
	//
	services->auxiliaryControlsBuilder =
		std::make_shared<SEAuxiliaryControlsBuilder>(services);
	services->backupManager =
		std::make_shared<SEBackupManager>(nullptr, nullptr, nullptr);
	services->browserDialogManager =
		std::make_shared<SEBrowserDialogManager>(nullptr, services);
	services->hostInfoProvider =
		std::make_shared<SEHostInfoProvider>(nullptr);
	services->hostStreamingManager =
		std::make_shared<SEHostStreamingManager>();
	services->hostUpdateManager = std::make_shared<SEHostUpdateManager>();
	services->httpClient = std::make_shared<SEHttpClient>();
	services->obsApiTransactionManager =
		std::make_shared<SEObsApiTransactionManager>(nullptr);
	services->outputManager =
		std::make_shared<SEOutputManager>(nullptr, nullptr, nullptr);
	services->outputSettingsManager =
		std::make_shared<SEOutputSettingsManager>();
	services->profilesManager = std::make_shared<SEProfilesManager>();
	services->sessionManager =
		std::make_shared<SESessionManager>(nullptr, nullptr, services);
	services->shellServices = std::make_shared<SEShellServices>();
	services->statePersistence = std::make_shared<SEStatePersistence>(
		nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
	services->statusBarMessageManager =
		std::make_shared<SEStatusBarMessageManager>(nullptr);

#ifdef SE_ENABLE_WYVRN
	//
	// Only present in a WYVRN build, and the handler bodies treat null as
	// "not available yet" rather than as an error -- the real manager
	// appears a few seconds into the session, after a 3.3-second SDK
	// init. A test that wants the absent case clears this.
	//
	services->razerWyvrnManager =
		std::make_shared<SERazerWyvrnManager>(nullptr, nullptr);
#endif
}

FakeApiServices::~FakeApiServices() {}

} // namespace se_fakes
