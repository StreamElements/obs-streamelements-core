//
// The contract of every host API method: given these arguments, a body calls
// these services with these values, in this order, and the page gets this
// JSON back.
//
// This is the broad half of the handler coverage, and it exists because the
// sweep in test_api_handler_bodies.cpp is shallower than it looks. The sweep
// calls every registered method with NO arguments and asserts only that each
// answers exactly once. For the 134 bodies that read an argument that means
// the only path it reaches is the refusal -- the guard at the top, and then
// nothing. The work the handler exists to do was untested on all but a dozen
// of them.
//
// So every row here supplies a well-formed argument and asserts what the body
// then did. Three things per row, each of which a live CDP probe can see at
// most one of:
//
//   * the reply, which a probe does see;
//   * the exact sequence of service calls, which it cannot -- two handlers
//     that answer `true` can differ in whether they persisted the layout;
//   * the arguments those calls received, which it cannot either, and which
//     is where the argument-shaping bugs live.
//
// Six shape tables carry the families where the bodies genuinely repeat
// themselves -- that repetition is a finding in its own right, so the tables
// name it rather than hiding it -- and a table of individual rows carries
// the rest. A completeness check at the end asserts that every registered
// method is in one table or the other, or is on a short list of names
// covered by a narrative case next door. A new host API method therefore
// fails this suite until somebody says what it is supposed to do.
//
// Readability note: `'` stands in for `"` throughout the tables, so a JSON
// argument reads as {'url':'x'} rather than {\"url\":\"x\"}. quoted() below
// does the substitution.
//

#include "api_handler_fixture.hpp"

#include "streamelements/ISEConfig.hpp"
#include "streamelements/ISEMessageBus.hpp"
#include "streamelements/SEObsOutputType.hpp"

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

using namespace se_harness;
using namespace se_bodies;

namespace {

// --- a row, and what each field means ---------------------------------

struct Row {
	//
	// The host API method, and the arguments the page sends it as a JSON
	// array. "[]" is a call with no arguments at all.
	//
	std::string method;
	std::string args;

	//
	// What the fakes answer, as `Service::Method=<kind>:<value>`,
	// semicolon-separated. Kinds: json, bool, int, str, dict, comp.
	// Nothing programmed means the quiet default -- an out-parameter is
	// left as the handler set it, a bool is false, a string empty.
	//
	std::string program;

	//
	// The calls the body must make, in order, as `Service::Method`,
	// optionally with the arguments it must pass in parentheses,
	// pipe-separated. An empty string asserts that NOTHING was called,
	// which is the whole assertion for a refused argument. A `*` in an
	// argument position skips that one.
	//
	std::string expect;

	//
	// The JSON the page receives. "<no reply>" is a real outcome and
	// worth being able to write down.
	//
	std::string reply;
};

// --- the mini-format --------------------------------------------------

std::string quoted(const std::string &text)
{
	std::string out = text;

	std::replace(out.begin(), out.end(), '\'', '"');

	return out;
}

std::vector<std::string> split(const std::string &text, char sep)
{
	std::vector<std::string> out;

	if (text.empty())
		return out;

	std::size_t at = 0;

	for (;;) {
		const std::size_t next = text.find(sep, at);

		if (next == std::string::npos) {
			out.push_back(text.substr(at));
			break;
		}

		out.push_back(text.substr(at, next - at));
		at = next + 1;
	}

	return out;
}

//
// Compare values rather than spellings. A recorded argument is whatever
// CefWriteJSON produced, and the model writes dictionary keys in its own
// order with its own spacing, so a hand-written expectation would otherwise
// be pinning the writer instead of the handler. Anything that is not JSON --
// a bare std::string argument, "<no reply>" -- is returned unchanged and
// compared literally.
//
std::string normalize(const std::string &text)
{
	if (text.empty())
		return text;

	CefRefPtr<CefValue> parsed = CefParseJSON(text, JSON_PARSER_RFC);

	if (!parsed)
		return text;

	return CefWriteJSON(parsed, JSON_WRITER_DEFAULT).ToString();
}

//
// The page's arguments. Parsed and re-serialized element by element, so the
// table can write one JSON array instead of escaping each argument
// separately.
//
std::vector<std::string> arguments_of(const std::string &jsonArray)
{
	std::vector<std::string> out;

	CefRefPtr<CefValue> parsed = CefParseJSON(jsonArray, JSON_PARSER_RFC);
	CefRefPtr<CefListValue> list = parsed ? parsed->GetList() : nullptr;

	if (!list)
		return out;

	for (std::size_t i = 0; i < list->GetSize(); ++i) {
		out.push_back(
			CefWriteJSON(list->GetValue(i), JSON_WRITER_DEFAULT)
				.ToString());
	}

	return out;
}

void program_journal(const std::string &program)
{
	for (const std::string &item : split(program, ';')) {
		if (item.empty())
			continue;

		const std::size_t eq = item.find('=');

		check(eq != std::string::npos,
		      "contract: malformed program entry '" + item + "'");

		if (eq == std::string::npos)
			continue;

		const std::string qualified = item.substr(0, eq);
		const std::string spec = item.substr(eq + 1);
		const std::size_t colon = spec.find(':');

		check(colon != std::string::npos,
		      "contract: malformed program spec '" + spec + "'");

		if (colon == std::string::npos)
			continue;

		const std::string kind = spec.substr(0, colon);
		const std::string value = spec.substr(colon + 1);

		se_fakes::Journal &j = se_fakes::journal();

		if (kind == "json") {
			j.SetJson(qualified, value);
		} else if (kind == "bool") {
			j.SetBool(qualified, value == "true");
		} else if (kind == "int") {
			j.SetInt(qualified, std::stoll(value));
		} else if (kind == "str") {
			j.SetString(qualified, value);
		} else if (kind == "dict") {
			CefRefPtr<CefValue> parsed =
				CefParseJSON(value, JSON_PARSER_RFC);

			j.SetDict(qualified,
				  parsed ? parsed->GetDictionary() : nullptr);
		} else if (kind == "comp") {
			//
			// A composition to hand back from
			// GetVideoComposition. Its own calls land in the
			// journal under ISEVideoComposition, which is how the
			// second level of the interface graph is asserted.
			//
			j.SetComposition(
				qualified,
				std::make_shared<
					se_fakes::FakeVideoComposition>());
		} else {
			check(false,
			      "contract: unknown program kind '" + kind + "'");
		}
	}
}

struct Expected {
	std::string qualified;
	bool checkArgs = false;
	std::vector<std::string> args;
};

std::vector<Expected> parse_expect(const std::string &expect)
{
	std::vector<Expected> out;

	for (const std::string &item : split(expect, ';')) {
		if (item.empty())
			continue;

		Expected e;

		const std::size_t open = item.find('(');

		if (open == std::string::npos) {
			e.qualified = item;
			out.push_back(e);
			continue;
		}

		e.qualified = item.substr(0, open);
		e.checkArgs = true;

		const std::size_t close = item.rfind(')');

		check(close != std::string::npos && close > open,
		      "contract: malformed expectation '" + item + "'");

		if (close == std::string::npos || close <= open) {
			out.push_back(e);
			continue;
		}

		const std::string inside =
			item.substr(open + 1, close - open - 1);

		if (!inside.empty())
			e.args = split(inside, '|');

		out.push_back(e);
	}

	return out;
}

std::string join(const std::vector<std::string> &items)
{
	std::string out;

	for (const std::string &item : items) {
		if (!out.empty())
			out += ";";

		out += item;
	}

	return out;
}

void run_row(const Row &row)
{
	//
	// The fixture first: FakeApiServices' constructor resets the journal
	// and installs the invariants the fakes carry, so programming before
	// it would be thrown away.
	//
	Fixture f;

	program_journal(quoted(row.program));

	const std::string reply =
		f.Call(row.method, arguments_of(quoted(row.args)));
	const std::string label =
		"contract: " + row.method + " " + quoted(row.args);

	check_eq(normalize(reply), normalize(quoted(row.reply)),
		 label + " -- reply");

	const std::vector<Expected> expected = parse_expect(quoted(row.expect));

	std::vector<std::string> wanted;

	for (const Expected &e : expected)
		wanted.push_back(e.qualified);

	//
	// The whole sequence in one assertion, so a failure prints what the
	// body did against what it should have done rather than naming one
	// missing call.
	//
	check_eq(join(f.journal().Sequence()), join(wanted),
		 label + " -- calls");

	if (f.journal().calls.size() != expected.size())
		return;

	for (std::size_t i = 0; i < expected.size(); ++i) {
		const Expected &e = expected[i];
		const se_fakes::Call &got = f.journal().calls[i];

		if (!e.checkArgs)
			continue;

		check_eq(got.args.size(), e.args.size(),
			 label + " -- " + e.qualified + " argument count");

		if (got.args.size() != e.args.size())
			continue;

		for (std::size_t a = 0; a < e.args.size(); ++a) {
			if (e.args[a] == "*")
				continue;

			check_eq(normalize(got.args[a]), normalize(e.args[a]),
				 label + " -- " + e.qualified + " argument " +
					 std::to_string(a));
		}
	}
}

// --- shape 1: a page argument in, the service's answer out ------------
//
// `if (args->GetSize()) service->Method(args->GetValue(0), result);` and
// nothing else -- 69 of the 200 bodies, by far the largest family. The body
// shapes nothing: it is a name in window.host bound to a method on a
// service, and what it adds is the argument check.
//
// Two rows each. The first proves the argument arrives unchanged and the
// service's answer is handed back; the second proves that with no argument
// the service is not called at all, which is the half the sweep could not
// distinguish from "called and did nothing".
//
struct PassThrough {
	const char *method;
	const char *call;

	//
	// The bodies that open with result->SetNull(), so their refusal is
	// `null` rather than the dispatcher's pre-set `false`. Twelve do,
	// and nothing documents which.
	//
	bool nullWhenRefused;
};

const PassThrough passThrough[] = {
	// scene items
	{"addCurrentSceneItemBrowserSource",
	 "ISEObsSceneManager::DeserializeObsBrowserSource", true},
	{"addSceneItemBrowserSource",
	 "ISEObsSceneManager::DeserializeObsBrowserSource", true},
	{"addCurrentSceneItemGameCaptureSource",
	 "ISEObsSceneManager::DeserializeObsGameCaptureSource", true},
	{"addSceneItemGameCaptureSource",
	 "ISEObsSceneManager::DeserializeObsGameCaptureSource", true},
	{"addCurrentSceneItemVideoCaptureSource",
	 "ISEObsSceneManager::DeserializeObsVideoCaptureSource", true},
	{"addSceneItemVideoCaptureSource",
	 "ISEObsSceneManager::DeserializeObsVideoCaptureSource", true},
	{"addCurrentSceneItemObsNativeSource",
	 "ISEObsSceneManager::DeserializeObsNativeSource", true},
	{"addSceneItemObsNativeSource",
	 "ISEObsSceneManager::DeserializeObsNativeSource", true},
	{"addCurrentSceneItemGroup",
	 "ISEObsSceneManager::DeserializeObsSceneItemGroup", true},
	{"addSceneItemGroup",
	 "ISEObsSceneManager::DeserializeObsSceneItemGroup", true},
	{"removeCurrentSceneItemsByIds",
	 "ISEObsSceneManager::RemoveObsSceneItemsByIds", false},
	{"removeSceneItemsByIds",
	 "ISEObsSceneManager::RemoveObsSceneItemsByIds", false},
	{"invokeCurrentSceneItemDefaultActionById",
	 "ISEObsSceneManager::InvokeCurrentSceneItemDefaultActionById", false},
	{"invokeCurrentSceneItemDefaultContextMenuById",
	 "ISEObsSceneManager::InvokeCurrentSceneItemDefaultContextMenuById",
	 false},
	{"setCurrentSceneItemsAuxiliaryActions",
	 "ISEObsSceneManager::DeserializeSceneItemsAuxiliaryActions", false},
	{"setScenesAuxiliaryActions",
	 "ISEObsSceneManager::DeserializeScenesAuxiliaryActions", false},
	{"setCurrentSceneItemPropertiesById",
	 "ISEObsSceneManager::SetObsSceneItemPropertiesById", false},
	{"setSceneItemPropertiesById",
	 "ISEObsSceneManager::SetObsSceneItemPropertiesById", false},
	{"getCurrentSceneItemPropertiesById",
	 "ISEObsSceneManager::GetObsSceneItemPropertiesById", false},
	{"getSceneItemPropertiesById",
	 "ISEObsSceneManager::GetObsSceneItemPropertiesById", false},
	{"ungroupCurrentSceneItemGroupById",
	 "ISEObsSceneManager::UngroupObsSceneItemsByGroupId", false},
	{"ungroupSceneItemGroupById",
	 "ISEObsSceneManager::UngroupObsSceneItemsByGroupId", false},
	{"openSceneItemPropertiesDialogById",
	 "ISEObsSceneManager::OpenSceneItemPropertiesById", false},
	{"openSceneItemFiltersDialogById",
	 "ISEObsSceneManager::OpenSceneItemFiltersById", false},
	{"openSceneItemInteractionDialogById",
	 "ISEObsSceneManager::OpenSceneItemInteractionById", false},
	{"openSceneItemTransformEditorDialogById",
	 "ISEObsSceneManager::OpenSceneItemTransformEditorById", false},
	{"getSceneItemRotationInViewport",
	 "ISEObsSceneManager::SerializeSceneItemViewportRotation", false},
	{"setSceneItemRotationInViewport",
	 "ISEObsSceneManager::DeserializeSceneItemViewportRotation", false},
	{"getSceneItemBoundingBoxInViewport",
	 "ISEObsSceneManager::SerializeSceneItemViewportBoundingRectangle",
	 false},
	{"setSceneItemPositionInViewport",
	 "ISEObsSceneManager::DeserializeSceneItemViewportPosition", false},

	// scenes, collections, source classes
	{"addScene", "ISEObsSceneManager::DeserializeObsScene", false},
	{"setCurrentSceneById", "ISEObsSceneManager::SetCurrentObsSceneById",
	 false},
	{"removeScenesByIds", "ISEObsSceneManager::RemoveObsScenesByIds",
	 false},
	{"setScenePropertiesById",
	 "ISEObsSceneManager::SetObsScenePropertiesById", false},
	{"addSceneCollection",
	 "ISEObsSceneManager::DeserializeObsSceneCollection", false},
	{"setCurrentSceneCollectionById",
	 "ISEObsSceneManager::DeserializeObsCurrentSceneCollectionById", false},
	{"getSourceClassProperties",
	 "ISEObsSceneManager::SerializeSourceClassProperties", false},

	// the source catalog's two property readers, free functions rather
	// than services -- the same shape, which is the point of the
	// comparison
	{"getInputSourceProperties",
	 "SEObsSourceCatalogUtils::SerializeObsSourceProperties", false},
	{"getFilterSourceProperties",
	 "SEObsSourceCatalogUtils::SerializeObsSourceProperties", false},

	// compositions
	{"removeVideoCompositionsByIds",
	 "ISEVideoCompositionManager::RemoveCompositionsByIds", false},
	{"addVideoComposition",
	 "ISEVideoCompositionManager::DeserializeComposition", false},
	{"setVideoCompositionProperties",
	 "ISEVideoCompositionManager::DeserializeExistingCompositionProperties",
	 false},
	{"getAvailableAudioEncoderClassProperties",
	 "ISEVideoCompositionManager::"
	 "SerializeAvailableAudioEncoderClassProperties",
	 false},
	{"getAvailableVideoEncoderClassProperties",
	 "ISEVideoCompositionManager::"
	 "SerializeAvailableVideoEncoderClassProperties",
	 false},
	{"removeAudioCompositionsByIds",
	 "ISEAudioCompositionManager::RemoveCompositionsByIds", false},
	{"addAudioComposition",
	 "ISEAudioCompositionManager::DeserializeComposition", false},
	{"setAudioCompositionProperties",
	 "ISEAudioCompositionManager::DeserializeExistingCompositionProperties",
	 false},
	{"addSharedVideoComposition",
	 "ISESharedVideoCompositionManager::DeserializeSharedVideoComposition",
	 true},
	{"setSharedVideoCompositionProperties",
	 "ISESharedVideoCompositionManager::"
	 "DeserializeSharedVideoCompositionProperties",
	 true},
	{"removeSharedVideoCompositionsByIds",
	 "ISESharedVideoCompositionManager::RemoveSharedVideoCompositionsByIds",
	 true},
	{"connectVideoCompositionToSharedVideoComposition",
	 "ISESharedVideoCompositionManager::"
	 "ConnectVideoCompositionToSharedVideoComposition",
	 true},
	{"disconnectVideoCompositionsFromSharedVideoCompositionsByIds",
	 "ISESharedVideoCompositionManager::"
	 "DisconnectVideoCompositionsFromSharedVideoCompositionsByIds",
	 true},

	// scoped storage
	{"readScopedStorageJsonItem", "ISEConfig::ReadScopedJsonFile", false},
	{"writeScopedStorageJsonItem", "ISEConfig::WriteScopedJsonFile", false},
	{"removeScopedStorageJsonItem", "ISEConfig::RemoveScopedJsonFile",
	 false},
	{"getAllScopedStorageJsonItems", "ISEConfig::ReadScopedJsonFilesList",
	 false},

	// backup
	{"queryUserEnvironmentBackupReferencedFiles",
	 "SEBackupManager::QueryLocalBackupPackageReferencedFiles", false},
	{"createUserEnvironmentBackupPackage",
	 "SEBackupManager::CreateLocalBackupPackage", false},
	{"queryUserEnvironmentBackupPackageContent",
	 "SEBackupManager::QueryBackupPackageContent", false},
	{"restoreUserEnvironmentBackupPackageContent",
	 "SEBackupManager::RestoreBackupPackageContent", false},

	// the rest
	{"getEncoderProperties",
	 "SEOutputSettingsManager::GetEncoderProperties", false},
	{"showModalDialog", "SEBrowserDialogManager::DeserializeModalDialog",
	 false},
	{"setHostReleaseGroupProperties",
	 "SEHostUpdateManager::DeserializeReleaseGroupProperties", false},
	{"getExternalSceneDataSceneCollections",
	 "ISEExternalSceneDataProviderManager::"
	 "SerializeProviderSceneCollections",
	 false},
	{"getExternalSceneDataSceneCollectionContent",
	 "ISEExternalSceneDataProviderManager::SerializeProviderSceneColletion",
	 false},
	{"httpRequestText", "SEHttpClient::DeserializeHttpRequestText", false},
	{"triggerRecordingOutputSplitById",
	 "SEOutputManager::TriggerSplitRecordingOutputById", false},
	{"triggerReplayBufferOutputSaveById",
	 "SEOutputManager::TriggerSaveReplayBufferById", false},
	{"openFileLocationInHostFileManager",
	 "SEShellServices::RevealFileInGraphicalShell", false},
};

// --- shape 2: no page input, the service serializes the answer --------
//
// `service->Method(result);` -- 40 bodies. One row each: the service is
// called exactly once, with whatever leading arguments the body supplies,
// and what it writes is what the page gets.
//
struct Serializer {
	const char *method;
	const char *call;

	//
	// The arguments the body passes ahead of the result, if any. Mostly
	// empty; the exceptions are the ones worth having written down --
	// getAllManagedHotkeyBindings is getAllHotkeyBindings with `true`.
	//
	const char *args;
};

const Serializer serializers[] = {
	{"getAllDockingWidgets",
	 "ISEBrowserWidgetManager::SerializeDockingWidgets", ""},
	{"getAllBackgroundWorkers", "ISEWorkerManager::Serialize", ""},
	{"getAllNonModalDialogs",
	 "SEBrowserDialogManager::SerializeAllNonModalDialogs", ""},
	{"getAvailableEncoders",
	 "SEOutputSettingsManager::GetAvailableEncoders", ""},
	{"getAvailableVideoEncoders",
	 "SEOutputSettingsManager::GetAvailableVideoEncoders", ""},
	{"getAvailableAudioEncoders",
	 "SEOutputSettingsManager::GetAvailableAudioEncoders", ""},
	{"getEncodingSettings", "SEOutputSettingsManager::GetEncodingSettings",
	 ""},
	{"getSystemCPUUsageTimes",
	 "SEHostInfoProvider::SerializeSystemCPUUsageTimes", ""},
	{"getSystemMemoryUsage",
	 "SEHostInfoProvider::SerializeSystemMemoryUsage", ""},
	{"getSystemHardwareProperties",
	 "SEHostInfoProvider::SerializeSystemHardwareProperties", ""},
	{"getHostProperties", "SEHostInfoProvider::SerializeHostProperties",
	 ""},
	{"getAllLoadedHostModules",
	 "SEHostInfoProvider::SerializeLoadedHostModules", ""},
	{"getAvailableFilterSourceTypes",
	 "SEObsSourceCatalogUtils::SerializeAvailableFilterSourceTypes", ""},
	{"getAvailableAudioFilterSourceTypes",
	 "SEObsSourceCatalogUtils::SerializeAvailableAudioFilterSourceTypes",
	 ""},
	{"getAvailableVideoFilterSourceTypes",
	 "SEObsSourceCatalogUtils::SerializeAvailableVideoFilterSourceTypes",
	 ""},
	{"getAvailableInputSourceTypes",
	 "SEObsSourceCatalogUtils::SerializeAvailableInputSourceTypes", ""},
	{"getAvailableVideoInputSourceTypes",
	 "SEObsSourceCatalogUtils::SerializeAvailableVideoInputSourceTypes",
	 ""},
	{"getAvailableAudioInputSourceTypes",
	 "SEObsSourceCatalogUtils::SerializeAvailableAudioInputSourceTypes",
	 ""},
	{"getAllExistingVideoInputSources",
	 "SEObsSourceCatalogUtils::SerializeExistingVideoInputSources", ""},
	{"getAllExistingAudioInputSources",
	 "SEObsSourceCatalogUtils::SerializeExistingAudioInputSources", ""},
	{"getAllExistingInputSources",
	 "SEObsSourceCatalogUtils::SerializeExistingInputSources", ""},
	{"getAllHotkeyBindings", "ISEHotkeyManager::SerializeHotkeyBindings",
	 "false"},
	{"getAllManagedHotkeyBindings",
	 "ISEHotkeyManager::SerializeHotkeyBindings", "true"},
	{"getCurrentSceneItemsAuxiliaryActions",
	 "ISEObsSceneManager::SerializeSceneItemsAuxiliaryActions", ""},
	{"getScenesAuxiliaryActions",
	 "ISEObsSceneManager::SerializeScenesAuxiliaryActions", ""},
	{"getAvailableInputSourceClasses",
	 "ISEObsSceneManager::SerializeInputSourceClasses", ""},
	{"getAllSceneCollections",
	 "ISEObsSceneManager::SerializeObsSceneCollections", ""},
	{"getCurrentSceneCollectionProperties",
	 "ISEObsSceneManager::SerializeObsCurrentSceneCollection", ""},
	{"getHostReleaseGroupProperties",
	 "SEHostUpdateManager::SerializeReleaseGroupProperties", ""},
	{"getExternalSceneDataProviders",
	 "ISEExternalSceneDataProviderManager::SerializeProviders", ""},
	{"getStreamingStatus",
	 "SEHostStreamingManager::SerializeStreamingStatus", ""},
	{"getUserInterfaceState",
	 "SEStatePersistence::SerializeUserInterfaceState", ""},
	{"getAuxiliaryMenuItems", "ISEMenuManager::SerializeAuxiliaryMenuItems",
	 ""},
	{"getAllProfiles", "SEProfilesManager::SerializeAllProfiles", ""},
	{"getCurrentProfile", "SEProfilesManager::SerializeCurrentProfile", ""},
	{"getAllAvailableVideoEncoderClasses",
	 "ISEVideoCompositionManager::SerializeAvailableVideoEncoderClasses",
	 ""},
	{"getAllAvailableAudioEncoderClasses",
	 "ISEVideoCompositionManager::SerializeAvailableAudioEncoderClasses",
	 ""},
	{"getAllVideoCompositions",
	 "ISEVideoCompositionManager::SerializeAllCompositions", ""},
	{"getAvailableTransitionClasses",
	 "ISEVideoCompositionManager::SerializeAvailableTransitionClasses", ""},
	{"getAllAudioCompositions",
	 "ISEAudioCompositionManager::SerializeAllCompositions", ""},
	{"getAllSharedVideoCompositions",
	 "ISESharedVideoCompositionManager::SerializeAllSharedVideoCompositions",
	 ""},
};

// --- shape 3: the service's bool IS the answer ------------------------
//
// `result->SetBool(service->Method(args->GetValue(0)));` -- 10 bodies, and
// the rows run them both ways. A body that calls the service and then
// answers `true` regardless looks identical to this one from a live probe,
// and six handlers in this file do exactly that, so the false case is the
// assertion that matters.
//
struct BoolCall {
	const char *method;
	const char *call;
};

const BoolCall boolCalls[] = {
	{"showStatusBarTemporaryMessage",
	 "SEStatusBarMessageManager::DeserializeStatusBarTemporaryMessage"},
	{"setStreamingSettings",
	 "SEOutputSettingsManager::SetStreamingSettings"},
	{"setEncodingSettings", "SEOutputSettingsManager::SetEncodingSettings"},
	{"openPopupWindow", "ISEPopupWindowManager::DeserializePopupWindow"},
	{"closeNonModalDialogsByIds",
	 "SEBrowserDialogManager::DeserializeCloseNonModalDialogsByIds"},
	{"focusNonModalDialogById",
	 "SEBrowserDialogManager::DeserializeFocusNonModalDialogById"},
	{"setStreamingStartUIHandlerProperties",
	 "ISENativeObsUIControlsManager::"
	 "DeserializeStartStreamingUIHandlerProperties"},
	{"setUserInterfaceState",
	 "SEStatePersistence::DeserializeUserInterfaceState"},
	{"setAuxiliaryMenuItems",
	 "ISEMenuManager::DeserializeAuxiliaryMenuItems"},
	{"setCurrentProfile",
	 "SEProfilesManager::DeserializeCurrentProfileById"},
};

// --- shape 4: tell the service, answer true ---------------------------
//
// `service->Method(); result->SetBool(true);` -- 6 bodies, no arguments and
// no return value. The reply carries no information at all, so the call is
// the only thing there is to assert, and three of these six are destructive
// enough that a live probe never runs them.
//
struct VoidCall {
	const char *method;
	const char *call;
};

const VoidCall voidCalls[] = {
	{"deleteAllCookies", "SESessionManager::DeleteCookies"},
	{"requestStreamingStart",
	 "ISENativeObsUIControlsManager::StartStreaming"},
	{"requestStreamingStop", "SEHostStreamingManager::RequestStop"},
	{"adviseStreamingStartUIRequestAccepted",
	 "ISENativeObsUIControlsManager::AdviseRequestStartStreamingAccepted"},
	{"adviseStreamingStartUIRequestRejected",
	 "ISENativeObsUIControlsManager::AdviseRequestStartStreamingRejected"},
};

// --- shape 5: the output family ---------------------------------------
//
// Sixteen bodies over one service, distinguished only by an ObsOutputType
// constant the page never sees: three kinds times five or six operations.
// The constant is the whole content of the handler, so passing the wrong
// one -- enable a recording output from the streaming handler -- would be
// invisible in the reply and is exactly what these rows pin.
//
struct OutputCall {
	const char *method;
	const char *call;
	ObsOutputType type;
	bool takesArgument;
};

const OutputCall outputCalls[] = {
	{"getAllStreamingOutputs", "SEOutputManager::SerializeAllOutputs",
	 StreamingOutput, false},
	{"addStreamingOutput", "SEOutputManager::DeserializeOutput",
	 StreamingOutput, true},
	{"removeStreamingOutputsByIds", "SEOutputManager::RemoveOutputsByIds",
	 StreamingOutput, true},
	{"enableStreamingOutputsByIds", "SEOutputManager::EnableOutputsByIds",
	 StreamingOutput, true},
	{"disableStreamingOutputsByIds", "SEOutputManager::DisableOutputsByIds",
	 StreamingOutput, true},
	{"getAllRecordingOutputs", "SEOutputManager::SerializeAllOutputs",
	 RecordingOutput, false},
	{"addRecordingOutput", "SEOutputManager::DeserializeOutput",
	 RecordingOutput, true},
	{"removeRecordingOutputsByIds", "SEOutputManager::RemoveOutputsByIds",
	 RecordingOutput, true},
	{"enableRecordingOutputsByIds", "SEOutputManager::EnableOutputsByIds",
	 RecordingOutput, true},
	{"disableRecordingOutputsByIds", "SEOutputManager::DisableOutputsByIds",
	 RecordingOutput, true},
	{"getAllReplayBufferOutputs", "SEOutputManager::SerializeAllOutputs",
	 ReplayBufferOutput, false},
	{"addReplayBufferOutput", "SEOutputManager::DeserializeOutput",
	 ReplayBufferOutput, true},
	{"removeReplayBufferOutputsByIds",
	 "SEOutputManager::RemoveOutputsByIds", ReplayBufferOutput, true},
	{"enableReplayBufferOutputsByIds",
	 "SEOutputManager::EnableOutputsByIds", ReplayBufferOutput, true},
	{"disableReplayBufferOutputsByIds",
	 "SEOutputManager::DisableOutputsByIds", ReplayBufferOutput, true},
};

// --- shape 6: the dock family -----------------------------------------
//
// `result->SetBool(widgetManager->Method(...)); if (result->GetBool()) {
// menuManager->Update(); statePersistence->PersistState(); }` -- eight
// bodies that gate the menu rebuild and the layout save on the widget
// manager's answer, which is one of the three different rules this file
// has about when to persist (see test_api_handler_bodies.cpp for the other
// two).
//
// Three rows each: it worked, it did not, and the arguments were wrong.
// The middle one is the interesting one -- a widget change that did not
// take effect must not persist a layout that did not change -- and it is
// not observable from a reply, which is `false` either way.
//
struct DockCall {
	const char *method;
	const char *call;
	const char *args;    // the page's arguments, a JSON array
	const char *passed;  // what the service must receive
	const char *refused; // arguments the body must refuse outright
};

const DockCall dockCalls[] = {
	{"toggleDockingWidgetFloatingById",
	 "ISEBrowserWidgetManager::ToggleWidgetFloatingStateById", "['d1']",
	 "d1", "[7]"},
	{"setDockingWidgetUrlById", "ISEBrowserWidgetManager::SetWidgetUrlById",
	 "['d1','https://a/']", "d1|https://a/", "['d1',7]"},
	{"setDockingWidgetTitleById",
	 "ISEBrowserWidgetManager::SetWidgetTitleById", "['d1','Title']",
	 "d1|Title", "['d1']"},
	{"showDockingWidgetById", "ISEBrowserWidgetManager::ShowDockWidgetById",
	 "['d1']", "d1", "[7]"},
	{"hideDockingWidgetById", "ISEBrowserWidgetManager::HideDockWidgetById",
	 "['d1']", "d1", "[7]"},
	{"groupDockingWidgetsPairByIds",
	 "ISEBrowserWidgetManager::GroupDockingWidgetPairByIds", "['d1','d2']",
	 "d1|d2", "['d1']"},
	//
	// Both insert handlers call the same service method and differ only
	// in the trailing bool, which is the entire difference between
	// "before" and "after" and is invisible in the reply.
	//
	{"insertDockingWidgetBeforeId",
	 "ISEBrowserWidgetManager::InsertDockingWidgetRelativeToId",
	 "['d1','d2']", "d1|d2|true", "['d1']"},
	{"insertDockingWidgetAfterId",
	 "ISEBrowserWidgetManager::InsertDockingWidgetRelativeToId",
	 "['d1','d2']", "d1|d2|false", "['d1']"},
	{"setDockingWidgetDimensionsById",
	 "ISEBrowserWidgetManager::SetWidgetDimensionsById",
	 "['d1',{'width':10,'height':20}]", "d1|10|20", "['d1']"},
	{"setDockingWidgetPositionById",
	 "ISEBrowserWidgetManager::SetWidgetPositionById",
	 "['d1',{'left':10,'top':20}]", "d1|10|20", "['d1']"},
};

// --- the rest, a row at a time ----------------------------------------
//
// Everything whose body does something of its own: flag arithmetic, a
// constant the page cannot see, a second-level composition call, a target
// address stitched into an event name. The repeated `probe` is the
// container target the fixture dispatches as.
//
const Row rows[] = {
	// --- the startup flags ---------------------------------------
	{"getStartupFlags", "[]", "ISEConfig::GetStartupFlags=int:7",
	 "ISEConfig::GetStartupFlags", "7"},
	{"setStartupFlags", "[7]", "", "ISEConfig::SetStartupFlags(7)", "true"},
	//
	// The type check that was missing: CefValueImpl::GetInt() does a bare
	// throw, so this row used to be a process kill rather than a refusal.
	//
	{"setStartupFlags", "['seven']", "", "", "false"},
	{"setStartupFlags", "[]", "", "", "false"},

	// --- the notification bar ------------------------------------
	{"showNotificationBar", "[{'text':'hi'}]", "",
	 "ISEBrowserWidgetManager::DeserializeNotificationBar({'text':'hi'});"
	 "SEStatePersistence::PersistState(true)",
	 "true"},
	{"showNotificationBar", "[]", "", "", "false"},
	{"hideNotificationBar", "[]", "",
	 "ISEBrowserWidgetManager::HideNotificationBar;"
	 "SEStatePersistence::PersistState(true)",
	 "true"},

	// --- the central widget --------------------------------------
	//
	// The drain loop at the top of both bodies is covered next door,
	// where the fake can be made to answer true a fixed number of
	// times. Here: the order of the four calls, and that the optional
	// script is passed as an empty string rather than omitted.
	//
	{"showCentralWidget",
	 "[{'url':'https://a/','executeJavaScriptCodeOnLoad':'go()'}]", "",
	 "ISEBrowserWidgetManager::DestroyCurrentCentralBrowserWidget;"
	 "ISEBrowserWidgetManager::PushCentralBrowserWidget(https://a/|go());"
	 "ISEMenuManager::Update;SEStatePersistence::PersistState(true)",
	 "true"},
	{"showCentralWidget", "[{'url':'https://a/'}]", "",
	 "ISEBrowserWidgetManager::DestroyCurrentCentralBrowserWidget;"
	 "ISEBrowserWidgetManager::PushCentralBrowserWidget(https://a/|);"
	 "ISEMenuManager::Update;SEStatePersistence::PersistState(true)",
	 "true"},
	{"hideCentralWidget", "[]", "",
	 "ISEBrowserWidgetManager::DestroyCurrentCentralBrowserWidget;"
	 "ISEMenuManager::Update;SEStatePersistence::PersistState(true)",
	 "true"},

	// --- onboarding and the sign-in flags ------------------------
	//
	// Destructive on a live profile -- they sign the account out -- so
	// these three rows are the only coverage these bodies will ever
	// have. completeOnBoarding reads the flags TWICE and writes twice,
	// rather than computing both changes from one read: with the fake
	// answering 3 both times, the first write is 2 and the second 3.
	//
	{"beginOnBoarding", "[]", "", "SESessionManager::Reset(true|0)",
	 "true"},
	{"completeOnBoarding", "[]", "ISEConfig::GetStartupFlags=int:3",
	 "ISEBrowserWidgetManager::DestroyCurrentCentralBrowserWidget;"
	 "ISEConfig::GetStartupFlags;ISEConfig::SetStartupFlags(2);"
	 "ISEConfig::GetStartupFlags;ISEConfig::SetStartupFlags(3);"
	 "ISEMenuManager::Update;SEStatePersistence::PersistState(true)",
	 "true"},
	{"adviseSignedIn", "[]", "ISEConfig::GetStartupFlags=int:0",
	 "ISEConfig::GetStartupFlags;ISEConfig::SetStartupFlags(2);"
	 "ISEMenuManager::Update;SEStatePersistence::PersistState(false)",
	 "true"},
	{"adviseSignedOut", "[]", "ISEConfig::GetStartupFlags=int:3",
	 "ISEConfig::GetStartupFlags;ISEConfig::SetStartupFlags(1);"
	 "ISEMenuManager::Update;SEStatePersistence::PersistState(false)",
	 "true"},

	// --- the browser ------------------------------------------------
	{"openDefaultBrowser", "['https://a/']",
	 "SEShellServices::OpenDefaultBrowser=bool:true",
	 "SEShellServices::OpenDefaultBrowser(https://a/)", "true"},
	{"openDefaultBrowser", "['https://a/']", "",
	 "SEShellServices::OpenDefaultBrowser(https://a/)", "false"},
	{"openDefaultBrowser", "[7]", "", "", "false"},

	// --- background workers -----------------------------------------
	//
	// PersistState(false) rather than the default true: adding a worker
	// deliberately does not tell the page its own state changed.
	//
	{"addBackgroundWorker", "[{'url':'https://a/'}]",
	 "ISEWorkerManager::DeserializeOne=str:wk-1",
	 "ISEWorkerManager::DeserializeOne({'url':'https://a/'});"
	 "SEStatePersistence::PersistState(false)",
	 "'wk-1'"},
	{"removeBackgroundWorkersByIds", "[['a','b']]", "",
	 "ISEWorkerManager::Remove(a);ISEWorkerManager::Remove(b);"
	 "SEStatePersistence::PersistState(false)",
	 "true"},

	// --- the bandwidth test -----------------------------------------
	//
	// The only handler that passes the calling container's address to a
	// service, which is how the result finds its way back to the page
	// that asked.
	//
	{"streamingBandwidthTestBegin", "[{'s':1},{'v':2}]",
	 "ISEBandwidthTestManager::BeginBandwidthTest=bool:true",
	 "ISEBandwidthTestManager::BeginBandwidthTest({'s':1}|{'v':2}|probe)",
	 "true"},
	{"streamingBandwidthTestBegin", "[{'s':1}]", "", "", "false"},
	{"streamingBandwidthTestEnd", "[{'o':1}]",
	 "ISEBandwidthTestManager::EndBandwidthTest=dict:{'done':true}",
	 "ISEBandwidthTestManager::EndBandwidthTest({'o':1})", "{'done':true}"},
	//
	// With no arguments the body passes a NULL handle rather than a
	// CefValue holding null, which is a difference the model can tell
	// and a service could crash on.
	//
	{"streamingBandwidthTestEnd", "[]",
	 "ISEBandwidthTestManager::EndBandwidthTest=dict:{'done':true}",
	 "ISEBandwidthTestManager::EndBandwidthTest(<nullptr>)",
	 "{'done':true}"},
	{"streamingBandwidthTestGetStatus", "[]",
	 "ISEBandwidthTestManager::GetBandwidthTestStatus=dict:{'active':false}",
	 "ISEBandwidthTestManager::GetBandwidthTestStatus", "{'active':false}"},

	// --- dialogs ----------------------------------------------------
	{"setNonModalDialogDimensionsById", "['d1',{'width':10}]",
	 "SEBrowserDialogManager::DeserializeNonModalDialogDimensionsById="
	 "bool:true",
	 "SEBrowserDialogManager::DeserializeNonModalDialogDimensionsById"
	 "('d1'|{'width':10})",
	 "true"},
	{"setNonModalDialogDimensionsById", "['d1']", "", "", "false"},

	// --- hotkeys ----------------------------------------------------
	{"setHotkeyBindingTriggers", "[{'t':1}]",
	 "ISEHotkeyManager::DeserializeHotkeyTriggers=bool:true",
	 "ISEHotkeyManager::DeserializeHotkeyTriggers({'t':1})", "true"},
	//
	// Refused with null rather than false, unlike most of the surface:
	// three of the hotkey handlers say so explicitly.
	//
	{"setHotkeyBindingTriggers", "[]", "", "", "null"},
	{"removeHotkeyBindingById", "[5]",
	 "ISEHotkeyManager::RemoveHotkeyBindingById=bool:true",
	 "ISEHotkeyManager::RemoveHotkeyBindingById(5)", "true"},
	{"removeHotkeyBindingById", "['5']", "", "", "null"},

	// --- the message bus --------------------------------------------
	//
	// The happy path has a case of its own below, because the flags are
	// spelled symbolically there rather than as a number -- the number
	// is the thing under test. This row is the refusal.
	//
	{"broadcastMessage", "['not a dictionary']", "", "", "false"},
	{"addBrowserScopedHttpServer", "[{'h':1}]",
	 "ISEMessageBus::DeserializeBrowserHttpServer=json:{'id':'srv-1'}",
	 "ISEMessageBus::DeserializeBrowserHttpServer(probe|{'h':1})",
	 "{'id':'srv-1'}"},
	{"getAllBrowserScopedHttpServers", "[]",
	 "ISEMessageBus::SerializeBrowserHttpServers=json:[]",
	 "ISEMessageBus::SerializeBrowserHttpServers(probe)", "[]"},
	{"removeBrowserScopedHttpServersByIds", "[['srv-1']]",
	 "ISEMessageBus::RemoveBrowserHttpServersByIds=json:{'removed':1}",
	 "ISEMessageBus::RemoveBrowserHttpServersByIds(probe|['srv-1'])",
	 "{'removed':1}"},
	{"sendHttpRequestResponse", "['req-1',{'status':200}]",
	 "ISEMessageBus::DeserializeHttpRequestResponse=json:{'sent':true}",
	 "ISEMessageBus::DeserializeHttpRequestResponse"
	 "('req-1'|{'status':200})",
	 "{'sent':true}"},
	{"sendHttpRequestResponse", "['req-1']", "", "", "false"},

	// --- scene items and scenes, where absent means null ------------
	//
	// Four bodies build a CefValue holding null when the page sends
	// nothing, rather than skipping the call: the service is asked for
	// everything. The `false` is SerializeObsSceneItems' second
	// parameter, which no handler ever passes as true.
	//
	{"getAllCurrentSceneItems", "[{'sceneId':'s1'}]",
	 "ISEObsSceneManager::SerializeObsSceneItems=json:[]",
	 "ISEObsSceneManager::SerializeObsSceneItems({'sceneId':'s1'}|false)",
	 "[]"},
	{"getAllCurrentSceneItems", "[]",
	 "ISEObsSceneManager::SerializeObsSceneItems=json:[]",
	 "ISEObsSceneManager::SerializeObsSceneItems(null|false)", "[]"},
	{"getAllSceneItems", "[{'sceneId':'s1'}]",
	 "ISEObsSceneManager::SerializeObsSceneItems=json:[]",
	 "ISEObsSceneManager::SerializeObsSceneItems({'sceneId':'s1'}|false)",
	 "[]"},
	{"getAllSceneItems", "[]",
	 "ISEObsSceneManager::SerializeObsSceneItems=json:[]",
	 "ISEObsSceneManager::SerializeObsSceneItems(null|false)", "[]"},
	{"getAllScenes", "[{'videoCompositionId':'vc-1'}]",
	 "ISEObsSceneManager::SerializeObsScenes=json:[]",
	 "ISEObsSceneManager::SerializeObsScenes({'videoCompositionId':'vc-1'})",
	 "[]"},
	{"getCurrentScene", "[{'videoCompositionId':'vc-1'}]",
	 "ISEObsSceneManager::SerializeObsCurrentScene=json:{'id':'s1'}",
	 "ISEObsSceneManager::SerializeObsCurrentScene"
	 "({'videoCompositionId':'vc-1'})",
	 "{'id':'s1'}"},
	{"getCurrentScene", "[]",
	 "ISEObsSceneManager::SerializeObsCurrentScene=json:{'id':'s1'}",
	 "ISEObsSceneManager::SerializeObsCurrentScene(null)", "{'id':'s1'}"},

	// --- the update check -------------------------------------------
	//
	// Three booleans, none of them in the reply, and the default for the
	// third is TRUE while the other two default to false. A page that
	// sends nothing gets the last cached answer.
	//
	{"queryHostReleaseGroupUpdateAvailability", "[]", "",
	 "SEHostUpdateManager::RequestCheckForUpdates(false|false|true)",
	 "true"},
	{"queryHostReleaseGroupUpdateAvailability",
	 "[{'allowDowngrade':true,'forceInstall':true,"
	 "'allowUseLastResponse':false}]",
	 "", "SEHostUpdateManager::RequestCheckForUpdates(true|true|false)",
	 "true"},
	//
	// A non-dictionary argument is ignored rather than refused: the
	// check still runs, with the defaults.
	//
	{"queryHostReleaseGroupUpdateAvailability", "['please']", "",
	 "SEHostUpdateManager::RequestCheckForUpdates(false|false|true)",
	 "true"},

	// --- the defer-save transaction ---------------------------------
	{"beginDeferSaveTransaction", "[]",
	 "SEObsApiTransactionManager::Create=str:tx-1",
	 "SEObsApiTransactionManager::Create(60000)", "'tx-1'"},
	{"completeDeferSaveTransaction", "['tx-1']", "",
	 "SEObsApiTransactionManager::Complete(tx-1)", "true"},
	{"completeDeferSaveTransaction", "[7]", "", "", "false"},

	// --- the menu ---------------------------------------------------
	{"setShowBuiltInMenuItems", "[true]", "",
	 "ISEMenuManager::SetShowBuiltInMenuItems(true)", "true"},
	{"setShowBuiltInMenuItems", "[false]", "",
	 "ISEMenuManager::SetShowBuiltInMenuItems(false)", "true"},
	{"setShowBuiltInMenuItems", "['yes']", "", "", "false"},
	{"getShowBuiltInMenuItems", "[]",
	 "ISEMenuManager::GetShowBuiltInMenuItems=bool:true",
	 "ISEMenuManager::GetShowBuiltInMenuItems", "true"},

	// --- the native UI ----------------------------------------------
	//
	// One bool, wrapped in a one-key dictionary the service does not
	// produce -- the body builds it.
	//
	{"getNativeStreamingServiceIntegrationStatus", "[]",
	 "ISENativeObsUIControlsManager::"
	 "GetNativeManageBroadcastButtonVisibility=bool:true",
	 "ISENativeObsUIControlsManager::"
	 "GetNativeManageBroadcastButtonVisibility",
	 "{'manageBroadcastButton':true}"},
	{"getNativeStreamingServiceIntegrationStatus", "[]", "",
	 "ISENativeObsUIControlsManager::"
	 "GetNativeManageBroadcastButtonVisibility",
	 "{'manageBroadcastButton':false}"},

	// --- the second level: a composition, then a call on it ---------
	//
	// These four are the only bodies that call a method on something a
	// service RETURNED, so they are the only coverage ISEVideoComposition
	// has. Note which overload of GetVideoComposition each one takes:
	// takeScreenshot passes the id as a std::string, which records
	// bare, and the transition handlers pass the whole CefValue, which
	// records as JSON.
	//
	{"takeScreenshot", "[{'videoCompositionId':'vc-1'}]",
	 "ISEVideoCompositionManager::GetVideoComposition=comp:",
	 "ISEVideoCompositionManager::GetVideoComposition(vc-1);"
	 "ISEVideoComposition::TakeScreenshot",
	 "true"},
	//
	// No id: the native composition, which FakeApiServices installs
	// because production never returns null for it.
	//
	{"takeScreenshot", "[{}]", "",
	 "ISEVideoCompositionManager::GetNativeVideoComposition;"
	 "ISEVideoComposition::TakeScreenshot",
	 "true"},
	//
	// A composition the manager does not know: no screenshot, and the
	// reply is the dispatcher's false rather than an error.
	//
	{"takeScreenshot", "[{'videoCompositionId':'nope'}]", "",
	 "ISEVideoCompositionManager::GetVideoComposition(nope)", "false"},
	{"takeScreenshot", "['vc-1']", "", "", "false"},
	{"getVideoCompositionTransition", "[{'videoCompositionId':'vc-1'}]",
	 "ISEVideoCompositionManager::GetVideoComposition=comp:;"
	 "ISEVideoComposition::SerializeTransition=json:{'id':'fade'}",
	 "ISEVideoCompositionManager::GetVideoComposition"
	 "({'videoCompositionId':'vc-1'});"
	 "ISEVideoComposition::SerializeTransition",
	 "{'id':'fade'}"},
	{"getVideoCompositionTransition", "[]",
	 "ISEVideoComposition::SerializeTransition=json:{'id':'fade'}",
	 "ISEVideoCompositionManager::GetNativeVideoComposition;"
	 "ISEVideoComposition::SerializeTransition",
	 "{'id':'fade'}"},
	//
	// An unknown composition leaves the reply at false -- and the body
	// does not guard the no-argument path the same way, which is why
	// the row above with no arguments reaches the native composition
	// instead of refusing.
	//
	{"getVideoCompositionTransition", "[{'videoCompositionId':'nope'}]", "",
	 "ISEVideoCompositionManager::GetVideoComposition"
	 "({'videoCompositionId':'nope'})",
	 "false"},
	//
	// setVideoCompositionTransition hands the SAME argument to both
	// calls: the dictionary that names the composition is also the
	// transition description.
	//
	{"setVideoCompositionTransition",
	 "[{'videoCompositionId':'vc-1','id':'cut'}]",
	 "ISEVideoCompositionManager::GetVideoComposition=comp:;"
	 "ISEVideoComposition::DeserializeTransition=json:{'id':'cut'}",
	 "ISEVideoCompositionManager::GetVideoComposition"
	 "({'videoCompositionId':'vc-1','id':'cut'});"
	 "ISEVideoComposition::DeserializeTransition"
	 "({'videoCompositionId':'vc-1','id':'cut'})",
	 "{'id':'cut'}"},
	{"showVideoCompositionTransitionPropertiesDialog",
	 "[{'videoCompositionId':'vc-1'}]",
	 "ISEVideoCompositionManager::GetVideoComposition=comp:;"
	 "ISEVideoComposition::ShowTransitionPropertiesDialog=bool:true",
	 "ISEVideoCompositionManager::GetVideoComposition"
	 "({'videoCompositionId':'vc-1'});"
	 "ISEVideoComposition::ShowTransitionPropertiesDialog",
	 "true"},

	// --- the popup menu ---------------------------------------------
	//
	// Unreachable from a live probe in either direction: QMenu::exec()
	// blocks until a human dismisses the menu, so a CDP run that
	// called this would hang OBS. These two rows are its only coverage.
	//
	{"showPopupMenuAtMousePointerPosition", "[{'items':[]}]",
	 "SEAuxiliaryControlsBuilder::ShowMenuAtMousePointerPosition=bool:true",
	 "SEAuxiliaryControlsBuilder::ShowMenuAtMousePointerPosition"
	 "({'items':[]})",
	 "true"},
	//
	// A menu that was not shown leaves the reply at false -- the body
	// sets true only inside the if, which is a different shape from
	// result->SetBool(service->Method()) and answers the same.
	//
	{"showPopupMenuAtMousePointerPosition", "[{'items':[]}]", "",
	 "SEAuxiliaryControlsBuilder::ShowMenuAtMousePointerPosition"
	 "({'items':[]})",
	 "false"},
};

// --- names covered by a narrative case, or deliberately not called ----
//
// The completeness check below needs to know that these are not oversights.
// Each is either pinned in test_api_handler_bodies.cpp, where it needed more
// than one row to say anything useful, or is one of the three handlers whose
// job is to kill the process.
//
const char *const coveredElsewhere[] = {
	// narrative cases next door
	"addDockingWidget",
	"removeDockingWidgetsByIds",
	"getCurrentContainerProperties",
	"broadcastEvent",
	"addHotkeyBinding",
	"showNonModalDialog",
	"removeVideoCompositionViewOverlay",
	"getVideoCompositionViewOverlayProperties",
	"setVideoCompositionViewOverlayProperties",
	"batchInvokeSeries",

	// these exist to take the process down
	"crashProgram",
	"crashProgramFastFail",
	"deadlockProgram",
};

// --- the expansions ---------------------------------------------------

std::vector<Row> all_rows()
{
	std::vector<Row> out;

	for (const PassThrough &p : passThrough) {
		const std::string answer =
			std::string("{'answered':'") + p.method + "'}";

		out.push_back({p.method, "[{'probe':1}]",
			       std::string(p.call) + "=json:" + answer,
			       std::string(p.call) + "({'probe':1})", answer});
		out.push_back({p.method, "[]", "", "",
			       p.nullWhenRefused ? "null" : "false"});
	}

	for (const Serializer &s : serializers) {
		const std::string answer =
			std::string("{'answered':'") + s.method + "'}";

		out.push_back({s.method, "[]",
			       std::string(s.call) + "=json:" + answer,
			       std::string(s.call) + "(" + s.args + ")",
			       answer});
	}

	for (const BoolCall &b : boolCalls) {
		out.push_back({b.method, "[{'probe':1}]",
			       std::string(b.call) + "=bool:true",
			       std::string(b.call) + "({'probe':1})", "true"});
		out.push_back({b.method, "[{'probe':1}]", "",
			       std::string(b.call) + "({'probe':1})", "false"});
		out.push_back({b.method, "[]", "", "", "false"});
	}

	for (const VoidCall &v : voidCalls)
		out.push_back({v.method, "[]", "", v.call, "true"});

	for (const OutputCall &o : outputCalls) {
		const std::string answer =
			std::string("{'answered':'") + o.method + "'}";
		const std::string type = std::to_string(o.type);

		if (!o.takesArgument) {
			out.push_back({o.method, "[]",
				       std::string(o.call) + "=json:" + answer,
				       std::string(o.call) + "(" + type + ")",
				       answer});
			continue;
		}

		out.push_back(
			{o.method, "[{'probe':1}]",
			 std::string(o.call) + "=json:" + answer,
			 std::string(o.call) + "(" + type + "|{'probe':1})",
			 answer});
		out.push_back({o.method, "[]", "", "", "false"});
	}

	for (const DockCall &d : dockCalls) {
		const std::string call = d.call;

		// It worked: the menu is rebuilt and the layout saved.
		out.push_back({d.method, d.args, call + "=bool:true",
			       call + "(" + d.passed + ");" +
				       "ISEMenuManager::Update;"
				       "SEStatePersistence::PersistState(true)",
			       "true"});

		// It did not: neither happens.
		out.push_back({d.method, d.args, "",
			       call + "(" + d.passed + ")", "false"});

		// The arguments were wrong: the service is not asked at all.
		out.push_back({d.method, d.refused, "", "", "false"});
	}

	for (const Row &r : rows)
		out.push_back(r);

	return out;
}

// --- the cases --------------------------------------------------------

void every_handler_does_what_its_contract_says()
{
	const std::vector<Row> table = all_rows();

	for (const Row &row : table) {
		//
		// Named and flushed before the call, as the sweep does it: a
		// body that takes the process down cannot report which one it
		// was, and ctest hides stdout on a pass.
		//
		std::printf("      contract: %s %s\n", row.method.c_str(),
			    quoted(row.args).c_str());
		std::fflush(stdout);

		run_row(row);
	}

	check(table.size() >= 300,
	      "the contract table must still be expanding to hundreds of "
	      "rows; a much smaller number means a shape table stopped being "
	      "reached");
}

//
// The flags broadcastMessage sends, by value rather than by name. A page
// calls this to reach other SE.Live surfaces in the same OBS, and the
// destination mask decides which ones -- browser sources included here,
// unlike broadcastEvent, which excludes them. Neither is visible in the
// reply, and a live probe can prove only that SOMETHING was delivered.
//
void broadcasting_a_message_names_its_audience_and_its_source()
{
	Fixture f;

	check_eq(f.Call("broadcastMessage", {"{\"m\":1}"}), "true",
		 "broadcastMessage: a dictionary is accepted");

	const se_fakes::Call *call =
		f.journal().First("ISEMessageBus::NotifyAllMessageListeners");

	check(call != nullptr,
	      "broadcastMessage: the bus is notified exactly once");

	if (!call)
		return;

	check_eq(call->args.size(), static_cast<std::size_t>(4),
		 "broadcastMessage: flags, source, address, payload");

	if (call->args.size() != 4)
		return;

	check_eq(call->args[0], std::to_string(ISEMessageBus::DEST_ALL_LOCAL),
		 "broadcastMessage: every local listener, browser sources "
		 "included -- which is where it differs from broadcastEvent");
	check_eq(call->args[1], std::string(ISEMessageBus::SOURCE_WEB),
		 "broadcastMessage: the source is the web side");
	check_eq(call->args[2], "urn:streamelements:internal:probe",
		 "broadcastMessage: the address carries the calling "
		 "container's own target");
	check_eq(normalize(call->args[3]), normalize("{\"m\":1}"),
		 "broadcastMessage: the payload is the page's dictionary, "
		 "unwrapped");
}

//
// Every name the page is offered has to be accounted for. This is the check
// that makes the table a contract rather than a sample: a host API method
// added without a row fails here, naming itself, and the fix is to say what
// it does.
//
void every_registered_method_is_in_the_table()
{
	const std::vector<Row> table = all_rows();

	std::vector<std::string> covered;

	for (const Row &row : table)
		covered.push_back(row.method);

	for (const char *const name : coveredElsewhere)
		covered.push_back(name);

	std::size_t missing = 0;

	for (const std::string &name : registered_names()) {
		if (std::find(covered.begin(), covered.end(), name) !=
		    covered.end())
			continue;

		std::fprintf(stderr,
			     "FAIL: %s is registered but has no contract row "
			     "and is not listed as covered elsewhere\n",
			     name.c_str());
		++missing;
	}

	check_eq(missing, static_cast<std::size_t>(0),
		 "every registered host API method is covered");

	//
	// And the other direction, which is the one that rots: a row for a
	// method that no longer exists would otherwise pass for ever,
	// testing nothing.
	//
	const std::vector<std::string> names = registered_names();

	std::size_t stale = 0;

	for (const std::string &name : covered) {
		if (std::find(names.begin(), names.end(), name) != names.end())
			continue;

		//
		// crashProgramFastFail is behind WIN32 and legitimately
		// absent everywhere else.
		//
		if (name == "crashProgramFastFail")
			continue;

		std::fprintf(stderr,
			     "FAIL: %s has a contract row but is not "
			     "registered\n",
			     name.c_str());
		++stale;
	}

	check_eq(stale, static_cast<std::size_t>(0),
		 "every contract row names a method that exists");
}

const Test cases[] = {
	{"every registered method is in the contract table",
	 every_registered_method_is_in_the_table},
	{"broadcasting a message names its audience and its source",
	 broadcasting_a_message_names_its_audience_and_its_source},
	{"every handler does what its contract says",
	 every_handler_does_what_its_contract_says},
};

} // namespace

namespace se_bodies {

const Test *const contract_tests = cases;
const std::size_t contract_test_count = sizeof(cases) / sizeof(cases[0]);

} // namespace se_bodies
