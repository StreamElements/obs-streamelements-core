#include "SEApiMessageHandler.hpp"

// __fastfail, for the crashProgramFastFail test hook below. Windows only: this
// file is in the unconditional source list, and neither the intrinsic nor the
// header exists on macOS.
#ifdef WIN32
#include <intrin.h>

#include <condition_variable>
#include <mutex>
#endif

#include "cef-headers.hpp"

#include "SEConfig.hpp"
//
// The services the handler bodies reach through apiServices->, one include
// each. They used to arrive transitively through SEGlobalStateManager.hpp:
// that header's own symbol was never referenced here, but it was pulling in
// roughly four hundred of this translation unit's four hundred and fifty
// local headers, and every service header among them. Naming them is what
// lets the unused include go, and it is also the list part 2 has to replace
// with interfaces -- so it should be visible rather than implied.
//
// SEConfig.hpp and SEMessageBus.hpp are the two already included above.
//
#include "SEAudioCompositionManager.hpp"
#include "SEAuxiliaryControlsBuilder.hpp"
#include "SEBackupManager.hpp"
#include "ISEBandwidthTestManager.hpp"
#include "SEBrowserDialogManager.hpp"
#include "SEBrowserWidgetManager.hpp"
#include "ISEExternalSceneDataProviderManager.hpp"
#include "ISEHotkeyManager.hpp"
#include "SEHttpClient.hpp"
#include "SEMenuManager.hpp"
#include "SENativeObsUIControlsManager.hpp"
#include "SEObsApiTransactionManager.hpp"
#include "SEObsSceneManager.hpp"
#include "SEOutputManager.hpp"
#include "SEOutputSettingsManager.hpp"
#include "ISEPopupWindowManager.hpp"
#include "SEProfilesManager.hpp"
#include "SESessionManager.hpp"
#include "SEHostInfoProvider.hpp"
#include "SEHostStreamingManager.hpp"
#include "SEHostUpdateManager.hpp"
#include "SEShellServices.hpp"
#include "SESharedVideoCompositionManager.hpp"
#include "SEStatePersistence.hpp"
#include "SEStatusBarMessageManager.hpp"
#include "SEVideoCompositionManager.hpp"
#include "ISEWorkerManager.hpp"
#ifdef SE_ENABLE_WYVRN
#include "SERazerWyvrnManager.hpp"
#endif
//
// The domains these bodies use, named rather than taken through the
// SEUtils.hpp umbrella -- four symbols out of twenty-one domains, which is
// what the umbrella was costing.
//
// What is left in this file's include closure is the 27 service headers
// below, which is part 2 of the plan: the bodies dereference concrete
// service types, and a complete type is what drags in obs.h and Qt.
//
#include "SEUtils/SELogUtils.hpp"
#include "SEUtils/SEEnvConfigUtils.hpp"
#include "SEUtils/SEObsSourceCatalogUtils.hpp"
#include "SEMessageBus.hpp"
#include "SEApiClientInfo.hpp"

//
// The API call lock for this translation unit. API_HANDLER_BEGIN takes it, and
// the handler header explains at length why it is per-TU rather than shared --
// in short, showModalDialog holds this one while a nested exec() runs, and the
// handlers that close that dialog therefore have to lock something else.
//
static std::recursive_mutex s_sync_api_call_mutex;

#ifdef SE_ENABLE_WYVRN
//
// The WYVRN manager, or null. Absent both before Initialize() has created it
// and after Shutdown() has released it -- and Shutdown() is exactly when a page
// is most likely to still be calling.
//
static std::shared_ptr<SERazerWyvrnManager>
GetRazerWyvrnManager(std::shared_ptr<SEApiServices> apiServices)
{
	return apiServices ? apiServices->razerWyvrnManager : nullptr;
}

//
// The `razerWyvrn` object. One serializer shared with the
// hostRazerWyvrnStatusChanged event, so a page cannot be told two different
// things about the same state.
//
static CefRefPtr<CefValue>
SerializeRazerWyvrnStatus(std::shared_ptr<SEApiServices> apiServices)
{
	auto manager = GetRazerWyvrnManager(apiServices);

	if (manager.get())
		return manager->SerializeStatus();

	// No manager at all: report the same shape rather than null, so a
	// caller never has to branch on its absence.
	CefRefPtr<CefValue> result = CefValue::Create();
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	d->SetBool("available", false);
	d->SetBool("initialized", false);
	d->SetString("status", "notCompiledIn");
	d->SetInt("eventCount", 0);

	result->SetDictionary(d);

	return result;
}
#endif

void SEApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	//
	// Registration is unconditional. The dispatcher bails per call when
	// the bundle cannot be locked, so there is no reason to answer the
	// same question again here -- and this way the shape of window.host
	// does not depend on construction timing: all of the methods are
	// always there, and one called after the bundle is gone resolves
	// with null.
	//
	// The trade: a page can no longer tell "not supported by this
	// plugin" from "plugin is shutting down" by probing for a missing
	// method.
	//

	RegisterIncomingApiCallHandler(
		"batchInvokeSeries",
		[](std::shared_ptr<SEApiMessageHandlerBase> self,
		   std::shared_ptr<SEApiServices> apiServices,
		   CefRefPtr<CefProcessMessage> message,
		   CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,
		   std::shared_ptr<SEApiClientInfo> target,
		   const long cefClientId,
		   std::function<void()> complete_callback) {
			result->SetNull();

			if (!self->IsRunning()) {
				complete_callback();
				return;
			}

			if (args->GetSize() < 1) {
				complete_callback();
				return;
			}

			if (args->GetType(0) != VTYPE_LIST) {
				complete_callback();
				return;
			}

			struct local_context {
				CefRefPtr<CefValue> queueIndex =
					CefValue::Create();
				CefRefPtr<CefListValue> queue =
					CefListValue::Create();
				CefRefPtr<CefListValue> results =
					CefListValue::Create();
				std::function<void()> process;
				std::function<void()> done;

				std::shared_ptr<SEApiMessageHandlerBase> self;
				CefRefPtr<CefProcessMessage> message;
				CefRefPtr<CefListValue> args;
				CefRefPtr<CefValue> result;
				std::shared_ptr<SEApiClientInfo> target;
				long cefClientId;
				std::function<void()> complete_callback;

				//
				// The autosave deferral this batch opens is
				// closed by done(), which captures [context]
				// only -- so the service travels here. The
				// manager rather than the whole bundle: a
				// heap-allocated context has no business in
				// the container's refcount.
				//
				std::shared_ptr<SEObsApiTransactionManager>
					transactions;
			};

			local_context *context = new local_context();

			context->self = self;
			context->message = message;
			context->args = args;
			context->result = result;
			context->target = target;
			context->cefClientId = cefClientId;
			context->complete_callback = complete_callback;
			context->transactions =
				apiServices->obsApiTransactionManager;

			context->queueIndex->SetInt(0);
			context->queue = args->GetList(0);
			context->done = [context]() {
				context->transactions->EndDeferSave();

				context->result->SetList(context->results);
				context->complete_callback();
				delete context;
			};

			context->process = [context]() {
				size_t index = context->queueIndex->GetInt();

				if (index >= context->queue->GetSize()) {
					// End of queue
					context->done();
					return;
				}

				if (context->queue->GetType(index) !=
				    VTYPE_DICTIONARY) {
					// Invalid queue element type
					context->done();
					return;
				}

				CefRefPtr<CefDictionaryValue> d =
					context->queue->GetDictionary(index);

				if (!d->HasKey("invoke") ||
				    !d->HasKey("invokeArgs") ||
				    d->GetType("invoke") != VTYPE_STRING ||
				    d->GetType("invokeArgs") != VTYPE_LIST) {
					// Invalid queue element structure
					context->done();
					return;
				}

				std::string invokeId =
					d->GetString("invoke").ToString();

				CefRefPtr<CefListValue> invokeArgs =
					d->GetList("invokeArgs");

				context->self->InvokeApiCallHandlerAsync(
					context->message,
					context->target,
					invokeId,
					invokeArgs,
					[=](CefRefPtr<CefValue> callResult) {
						context->results->SetValue(
							context->results
								->GetSize(),
							callResult);

						context->queueIndex->SetInt(
							context->queueIndex
								->GetInt() +
							1);

						context->process();
					},
					context->cefClientId,
					IsTraceLogLevel() /* enable_logging */);
			};

			apiServices->obsApiTransactionManager->BeginDeferSave();

			context->process();
		});

	API_HANDLER_BEGIN("getStartupFlags");
	{
		result->SetInt(
			apiServices->config->GetStartupFlags());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStartupFlags");
	{
		if (args->GetSize()) {
			apiServices->config->SetStartupFlags(
				args->GetValue(0)->GetInt());

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("deleteAllCookies");
	{
		apiServices->sessionManager->DeleteCookies();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openDefaultBrowser");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				apiServices->shellServices->OpenDefaultBrowser(
					args->GetString(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showNotificationBar");
	{
		if (args->GetSize()) {
			CefRefPtr<CefValue> barInfo = args->GetValue(0);

			apiServices->widgetManager
				->DeserializeNotificationBar(barInfo);

			apiServices->statePersistence->PersistState();

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideNotificationBar");
	{
		apiServices->widgetManager->HideNotificationBar();

		apiServices->statePersistence->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showCentralWidget");
	{
		if (args->GetSize()) {
			CefRefPtr<CefDictionaryValue> rootDictionary =
				args->GetValue(0)->GetDictionary();

			if (rootDictionary.get() &&
			    rootDictionary->HasKey("url")) {
				// Remove all central widgets
				while (apiServices->widgetManager
					       ->DestroyCurrentCentralBrowserWidget()) {
				}

				std::string executeJavaScriptCodeOnLoad;

				if (rootDictionary->HasKey(
					    "executeJavaScriptCodeOnLoad")) {
					executeJavaScriptCodeOnLoad =
						rootDictionary
							->GetString(
								"executeJavaScriptCodeOnLoad")
							.ToString();
				}

				apiServices->widgetManager
					->PushCentralBrowserWidget(
						rootDictionary->GetString("url")
							.ToString()
							.c_str(),
						executeJavaScriptCodeOnLoad
							.c_str());

				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideCentralWidget");
	{
		while (apiServices->widgetManager
			       ->DestroyCurrentCentralBrowserWidget()) {
		}

		apiServices->menuManager->Update();
		apiServices->statePersistence->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addDockingWidget");
	{
		if (args->GetSize()) {
			CefRefPtr<CefValue> widgetInfo = args->GetValue(0);

			std::string id =
				apiServices->widgetManager
					->AddDockBrowserWidget(widgetInfo);

			if (apiServices->widgetManager->HasDockWidget(
				    id.c_str())) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();

				result->SetString(id);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeDockingWidgetsByIds");
	{
		if (args->GetSize()) {
			CefRefPtr<CefListValue> list = args->GetList(0);

			if (list.get()) {
				for (size_t i = 0; i < list->GetSize(); ++i) {
					CefString id = list->GetString(i);

					apiServices->widgetManager
						->RemoveDockWidget(
							id.ToString().c_str());
				}

				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllDockingWidgets");
	{
		apiServices->widgetManager->SerializeDockingWidgets(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("toggleDockingWidgetFloatingById");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(apiServices->widgetManager
						->ToggleWidgetFloatingStateById(
							args->GetString(0)
								.ToString()
								.c_str()));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetDimensionsById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_DICTIONARY) {
			int width = -1;
			int height = -1;

			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(1);

			if (d->HasKey("width") &&
			    d->GetType("width") == VTYPE_INT) {
				width = d->GetInt("width");
			}

			if (d->HasKey("height") &&
			    d->GetType("height") == VTYPE_INT) {
				height = d->GetInt("height");
			}

			result->SetBool(apiServices->widgetManager
						->SetWidgetDimensionsById(
							args->GetString(0)
								.ToString()
								.c_str(),
							width, height));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetPositionById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_DICTIONARY) {
			int left = -1;
			int top = -1;

			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(1);

			if (d->HasKey("left") &&
			    d->GetType("left") == VTYPE_INT) {
				left = d->GetInt("left");
			}

			if (d->HasKey("top") &&
			    d->GetType("top") == VTYPE_INT) {
				top = d->GetInt("top");
			}

			result->SetBool(apiServices->widgetManager
						->SetWidgetPositionById(
							args->GetString(0)
								.ToString()
								.c_str(),
							left, top));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetUrlById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_STRING) {
			result->SetBool(
				apiServices->widgetManager->SetWidgetUrlById(
					args->GetString(0).ToString().c_str(),
					args->GetString(1).ToString().c_str()));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setDockingWidgetTitleById");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_STRING) {
			result->SetBool(
				apiServices->widgetManager->SetWidgetTitleById(
					args->GetString(0).ToString().c_str(),
					args->GetString(1).ToString().c_str()));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showDockingWidgetById");
	{
		if (args->GetSize() >= 1 && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				apiServices->widgetManager
					->ShowDockWidgetById(args->GetString(0)
								     .ToString()
								     .c_str()));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideDockingWidgetById");
	{
		if (args->GetSize() >= 1 && args->GetType(0) == VTYPE_STRING) {
			result->SetBool(
				apiServices->widgetManager
					->HideDockWidgetById(args->GetString(0)
								     .ToString()
								     .c_str()));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("groupDockingWidgetsPairByIds");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING && args->GetType(1) == VTYPE_STRING) {
			result->SetBool(apiServices->widgetManager
						->GroupDockingWidgetPairByIds(
							args->GetString(0)
								.ToString()
								.c_str(),
							args->GetString(1)
								.ToString()
								.c_str()));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("insertDockingWidgetBeforeId");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_STRING) {
			result->SetBool(
				apiServices->widgetManager
					->InsertDockingWidgetRelativeToId(
						args->GetString(0)
							.ToString()
							.c_str(),
						args->GetString(1)
							.ToString()
							.c_str(),
						true));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("insertDockingWidgetAfterId");
	{
		if (args->GetSize() >= 2 && args->GetType(0) == VTYPE_STRING &&
		    args->GetType(1) == VTYPE_STRING) {
			result->SetBool(
				apiServices->widgetManager
					->InsertDockingWidgetRelativeToId(
						args->GetString(0)
							.ToString()
							.c_str(),
						args->GetString(1)
							.ToString()
							.c_str(),
						false));

			if (result->GetBool()) {
				apiServices->menuManager->Update();
				apiServices->statePersistence
					->PersistState();
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("beginOnBoarding");
	{
		apiServices->sessionManager->Reset();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("completeOnBoarding");
	{
		while (apiServices->widgetManager
			       ->DestroyCurrentCentralBrowserWidget()) {
		}

		apiServices->config->SetStartupFlags(
			apiServices->config->GetStartupFlags() &
			~SEConfig::STARTUP_FLAGS_ONBOARDING_MODE);

		//
		// Once on-boarding is complete, we assume our state is signed-in.
		//
		// This is not enough. There are edge cases where the user is signed-in
		// but not yet has completed on-boarding.
		//
		// For those cases, we provide the adviseSignedIn() API call.
		//
		apiServices->config->SetStartupFlags(
			apiServices->config->GetStartupFlags() |
			SEConfig::STARTUP_FLAGS_SIGNED_IN);

		apiServices->menuManager->Update();
		apiServices->statePersistence->PersistState();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showStatusBarTemporaryMessage");
	{
		if (args->GetSize()) {
			result->SetBool(
				apiServices->statusBarMessageManager
					->DeserializeStatusBarTemporaryMessage(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestBegin");
	{
		if (args->GetSize() >= 2) {
			result->SetBool(
				apiServices->bandwidthTestManager
					->BeginBandwidthTest(args->GetValue(0),
							     args->GetValue(1),
							     target->m_target));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestEnd");
	{
		CefRefPtr<CefValue> options;

		if (args->GetSize()) {
			options = args->GetValue(0);
		}
		result->SetDictionary(apiServices->bandwidthTestManager
					      ->EndBandwidthTest(options));
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("streamingBandwidthTestGetStatus");
	{
		result->SetDictionary(apiServices->bandwidthTestManager
					      ->GetBandwidthTestStatus());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getEncoderProperties");
	{
		if (args->GetSize() > 0) {
			apiServices->outputSettingsManager
				->GetEncoderProperties(args->GetValue(0),
						       result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableEncoders");
	{
		apiServices->outputSettingsManager->GetAvailableEncoders(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableVideoEncoders");
	{
		apiServices->outputSettingsManager->GetAvailableVideoEncoders(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableAudioEncoders");
	{
		apiServices->outputSettingsManager->GetAvailableAudioEncoders(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStreamingSettings");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->outputSettingsManager
						->SetStreamingSettings(
							args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getEncodingSettings");
	{
		apiServices->outputSettingsManager->GetEncodingSettings(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setEncodingSettings");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->outputSettingsManager
						->SetEncodingSettings(
							args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentContainerProperties");
	{
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		std::string dockingArea = "none";

		SEBrowserWidgetManager::DockBrowserWidgetInfo *info =
			apiServices->widgetManager
				->GetDockBrowserWidgetInfo(
					target->m_target.c_str());

		if (info) {
			dockingArea = info->m_dockingArea;

			d->SetString("url", info->m_url);

			delete info;
		}

		d->SetString("id", target->m_target);
		d->SetString("dockingArea", dockingArea);
		d->SetString("theme", GetCurrentThemeName());
		d->SetString("type", self->ContainerType());

		result->SetDictionary(d);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openPopupWindow");
	{
		if (args->GetSize()) {
			result->SetBool(
				apiServices->popupWindowManager
					->DeserializePopupWindow(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addBackgroundWorker");
	{
		if (args->GetSize()) {
			result->SetString(
				apiServices->workerManager->DeserializeOne(
					args->GetValue(0)));

			apiServices->statePersistence->PersistState(false);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllBackgroundWorkers");
	{
		apiServices->workerManager->Serialize(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeBackgroundWorkersByIds");
	{
		if (args->GetSize()) {
			CefRefPtr<CefListValue> list = args->GetList(0);

			if (list.get()) {
				for (size_t i = 0; i < list->GetSize(); ++i) {
					CefString id = list->GetString(i);

					apiServices->workerManager->Remove(
						id);
				}

				apiServices->statePersistence->PersistState(
					false);

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showModalDialog");
	{
		if (args->GetSize()) {
			apiServices->browserDialogManager
				->DeserializeModalDialog(args->GetValue(0),
							 result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showNonModalDialog");
	{
		if (args->GetSize()) {
			auto promise_ptr =
				apiServices->browserDialogManager
					->DeserializeNonModalDialog(
						args->GetValue(0));

			std::thread([complete_callback, promise_ptr, result]() -> void {
				auto future = promise_ptr->get_future();

				future.wait();

				if (!future.valid()) {
					result->SetNull();
				} else {
					result->SetValue(future.get());
				}

				complete_callback();
			}).detach();
		}
	}
	API_HANDLER_END_ASYNC();

	API_HANDLER_BEGIN("getAllNonModalDialogs");
	{
		apiServices->browserDialogManager
			->SerializeAllNonModalDialogs(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("closeNonModalDialogsByIds");
	{
		if (args->GetSize() >= 1) {
			result->SetBool(
				apiServices->browserDialogManager
					->DeserializeCloseNonModalDialogsByIds(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("focusNonModalDialogById");
	{
		if (args->GetSize() >= 1) {
			result->SetBool(
				apiServices->browserDialogManager
					->DeserializeFocusNonModalDialogById(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setNonModalDialogDimensionsById");
	{
		if (args->GetSize() >= 2) {
			result->SetBool(
				apiServices->browserDialogManager
					->DeserializeNonModalDialogDimensionsById(
						args->GetValue(0),
						args->GetValue(1)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSystemCPUUsageTimes");
	{
		apiServices->hostInfoProvider->SerializeSystemCPUUsageTimes(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSystemMemoryUsage");
	{
		apiServices->hostInfoProvider->SerializeSystemMemoryUsage(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSystemHardwareProperties");
	{
		apiServices->hostInfoProvider->SerializeSystemHardwareProperties(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableFilterSourceTypes");
	{
		SerializeAvailableFilterSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableAudioFilterSourceTypes");
	{
		SerializeAvailableAudioFilterSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableVideoFilterSourceTypes");
	{
		SerializeAvailableVideoFilterSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getInputSourceProperties");
	{
		if (args->GetSize() > 0) {
			SerializeObsSourceProperties(args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getFilterSourceProperties");
	{
		if (args->GetSize() > 0) {
			SerializeObsSourceProperties(args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableInputSourceTypes");
	{
		SerializeAvailableInputSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableVideoInputSourceTypes");
	{
		SerializeAvailableVideoInputSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableAudioInputSourceTypes");
	{
		SerializeAvailableAudioInputSourceTypes(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllExistingVideoInputSources");
	{
		SerializeExistingVideoInputSources(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllExistingAudioInputSources");
	{
		SerializeExistingAudioInputSources(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllExistingInputSources");
	{
		SerializeExistingInputSources(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllHotkeyBindings");
	{
		apiServices->hotkeyManager->SerializeHotkeyBindings(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllManagedHotkeyBindings");
	{
		apiServices->hotkeyManager->SerializeHotkeyBindings(result,
									 true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addHotkeyBinding");
	{
		if (args->GetSize()) {
			const int64_t id =
				apiServices->hotkeyManager
					->DeserializeSingleHotkeyBinding(
						args->GetValue(0));

			if (id >= 0) {
				result->SetInt((int)id);
			} else {
				result->SetNull();
			}
		} else
			result->SetNull();
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setHotkeyBindingTriggers");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->hotkeyManager
						->DeserializeHotkeyTriggers(
							args->GetValue(0)));
		} else {
			result->SetNull();
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeHotkeyBindingById");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_INT) {
			result->SetBool(apiServices->hotkeyManager
						->RemoveHotkeyBindingById(
							args->GetInt(0)));
		} else {
			result->SetNull();
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getHostProperties");
	{
		apiServices->hostInfoProvider->SerializeHostProperties(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseSignedIn");
	{
		auto config = apiServices->config;

		config->SetStartupFlags(
			config->GetStartupFlags() |
			SEConfig::STARTUP_FLAGS_SIGNED_IN);

		apiServices->menuManager->Update();
		apiServices->statePersistence->PersistState(false);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseSignedOut");
	{
		auto config = apiServices->config;

		config->SetStartupFlags(
			config->GetStartupFlags() &
			~SEConfig::STARTUP_FLAGS_SIGNED_IN);

		apiServices->menuManager->Update();
		apiServices->statePersistence->PersistState(false);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("broadcastMessage");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_DICTIONARY) {
			CefRefPtr<CefValue> message = args->GetValue(0);

			apiServices->messageBus
				->NotifyAllMessageListeners(
					SEMessageBus::DEST_ALL_LOCAL,
					SEMessageBus::SOURCE_WEB,
					"urn:streamelements:internal:" + target->m_target,
					message);

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("broadcastEvent");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> d =
				args->GetDictionary(0);

			if (d->HasKey("name") &&
			    d->GetType("name") == VTYPE_STRING) {
				std::string sourceUrl =
					"urn:streamelements:internal:" + target->m_target;

				apiServices->messageBus
					->NotifyAllLocalEventListeners(
						SEMessageBus::
								DEST_ALL_LOCAL &
							~SEMessageBus::
								DEST_BROWSER_SOURCE,
						SEMessageBus::
							SOURCE_WEB,
						sourceUrl, "hostEventReceived",
						args->GetValue(0));

				apiServices->messageBus
					->NotifyAllExternalEventListeners(
						SEMessageBus::
							DEST_ALL_EXTERNAL,
						SEMessageBus::
							SOURCE_WEB,
						sourceUrl, d->GetString("name"),
						d->GetValue("data"));

				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemBrowserSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsBrowserSource(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemBrowserSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsBrowserSource(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemGameCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsGameCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemGameCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsGameCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemVideoCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsVideoCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemVideoCaptureSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsVideoCaptureSource(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemObsNativeSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsNativeSource(args->GetValue(0),
							     result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemObsNativeSource");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsNativeSource(args->GetValue(0),
							     result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addCurrentSceneItemGroup");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsSceneItemGroup(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneItemGroup");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsSceneItemGroup(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllCurrentSceneItems");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SerializeObsSceneItems(args->GetValue(0),
							 result, false);
		} else {
			CefRefPtr<CefValue> nullArg = CefValue::Create();

			apiServices->obsSceneManager
				->SerializeObsSceneItems(nullArg, result,
							 false);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllSceneItems");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SerializeObsSceneItems(args->GetValue(0),
							 result, false);
		} else {
			CefRefPtr<CefValue> nullArg = CefValue::Create();

			apiServices->obsSceneManager
				->SerializeObsSceneItems(nullArg, result,
							 false);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeCurrentSceneItemsByIds");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->RemoveObsSceneItemsByIds(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeSceneItemsByIds");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->RemoveObsSceneItemsByIds(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("invokeCurrentSceneItemDefaultActionById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->InvokeCurrentSceneItemDefaultActionById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("invokeCurrentSceneItemDefaultContextMenuById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->InvokeCurrentSceneItemDefaultContextMenuById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneItemsAuxiliaryActions");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeSceneItemsAuxiliaryActions(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentSceneItemsAuxiliaryActions");
	{
		apiServices->obsSceneManager
			->SerializeSceneItemsAuxiliaryActions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setScenesAuxiliaryActions");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeScenesAuxiliaryActions(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getScenesAuxiliaryActions");
	{
		apiServices->obsSceneManager
			->SerializeScenesAuxiliaryActions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneItemPropertiesById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SetObsSceneItemPropertiesById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setSceneItemPropertiesById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SetObsSceneItemPropertiesById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentSceneItemPropertiesById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->GetObsSceneItemPropertiesById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSceneItemPropertiesById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->GetObsSceneItemPropertiesById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("ungroupCurrentSceneItemGroupById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->UngroupObsSceneItemsByGroupId(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("ungroupSceneItemGroupById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->UngroupObsSceneItemsByGroupId(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableInputSourceClasses");
	{
		apiServices->obsSceneManager->SerializeInputSourceClasses(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSourceClassProperties");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SerializeSourceClassProperties(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getHostReleaseGroupProperties");
	{
		apiServices->hostUpdateManager->SerializeReleaseGroupProperties(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setHostReleaseGroupProperties");
	{
		if (args->GetSize()) {
			apiServices->hostUpdateManager
				->DeserializeReleaseGroupProperties(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("queryHostReleaseGroupUpdateAvailability");
	{
		bool allowDowngrade = false;
		bool forceInstall = false;
		bool allowUseLastResponse = true;

		if (args->GetSize() > 0) {
			if (args->GetValue(0)->GetType() == VTYPE_DICTIONARY) {
				CefRefPtr<CefDictionaryValue> d =
					args->GetDictionary(0);

				if (d->HasKey("allowDowngrade") &&
				    d->GetType("allowDowngrade") ==
					    VTYPE_BOOL) {
					allowDowngrade =
						d->GetBool("allowDowngrade");
				}

				if (d->HasKey("forceInstall") &&
				    d->GetType("forceInstall") ==
					    VTYPE_BOOL) {
					forceInstall =
						d->GetBool("forceInstall");
				}

				if (d->HasKey("allowUseLastResponse") &&
				    d->GetType("allowUseLastResponse") == VTYPE_BOOL) {
					allowUseLastResponse =
						d->GetBool("allowUseLastResponse");
				}
			}
		}

		apiServices->hostUpdateManager->RequestCheckForUpdates(
			allowDowngrade, forceInstall, allowUseLastResponse);

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getExternalSceneDataProviders");
	{
		apiServices->externalSceneDataProviderManager
			->SerializeProviders(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getExternalSceneDataSceneCollections");
	{
		if (args->GetSize()) {
			apiServices->externalSceneDataProviderManager
				->SerializeProviderSceneCollections(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getExternalSceneDataSceneCollectionContent");
	{
		if (args->GetSize()) {
			apiServices->externalSceneDataProviderManager
				->SerializeProviderSceneColletion(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("httpRequestText");
	{
		if (args->GetSize()) {
			apiServices->httpClient->DeserializeHttpRequestText(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getStreamingStatus");
	{
		apiServices->hostStreamingManager->SerializeStreamingStatus(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("requestStreamingStart");
	{
		apiServices->nativeObsUIControlsManager->StartStreaming();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("requestStreamingStop");
	{
		apiServices->hostStreamingManager->RequestStop();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setStreamingStartUIHandlerProperties");
	{
		if (args->GetSize()) {
			result->SetBool(
				apiServices->nativeObsUIControlsManager
					->DeserializeStartStreamingUIHandlerProperties(
						args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseStreamingStartUIRequestAccepted");
	{
		apiServices->nativeObsUIControlsManager
			->AdviseRequestStartStreamingAccepted();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("adviseStreamingStartUIRequestRejected");
	{
		apiServices->nativeObsUIControlsManager
			->AdviseRequestStartStreamingRejected();

		result->SetBool(true);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getUserInterfaceState");
	{
		apiServices->statePersistence->SerializeUserInterfaceState(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setUserInterfaceState");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->statePersistence
						->DeserializeUserInterfaceState(
							args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllScenes");
	{
		CefRefPtr<CefValue> input = CefValue::Create();

		if (args->GetSize())
			input = args->GetValue(0);
		else
			input->SetNull();

		apiServices->obsSceneManager->SerializeObsScenes(input,
								      result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentScene");
	{
		CefRefPtr<CefValue> input = CefValue::Create();

		if (args->GetSize())
			input = args->GetValue(0);
		else
			input->SetNull();

		apiServices->obsSceneManager->SerializeObsCurrentScene(
			input, result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addScene");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager->DeserializeObsScene(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SetCurrentObsSceneById(args->GetValue(0),
							 result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeScenesByIds");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager->RemoveObsScenesByIds(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setScenePropertiesById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SetObsScenePropertiesById(args->GetValue(0),
							    result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setAuxiliaryMenuItems");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->menuManager
						->DeserializeAuxiliaryMenuItems(
							args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAuxiliaryMenuItems");
	{
		apiServices->menuManager->SerializeAuxiliaryMenuItems(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllProfiles");
	{
		apiServices->profilesManager->SerializeAllProfiles(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentProfile");
	{
		apiServices->profilesManager->SerializeCurrentProfile(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentProfile");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->profilesManager
						->DeserializeCurrentProfileById(
							args->GetValue(0)));
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllSceneCollections");
	{
		apiServices->obsSceneManager->SerializeObsSceneCollections(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getCurrentSceneCollectionProperties");
	{
		apiServices->obsSceneManager
			->SerializeObsCurrentSceneCollection(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSceneCollection");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsSceneCollection(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setCurrentSceneCollectionById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeObsCurrentSceneCollectionById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("queryUserEnvironmentBackupReferencedFiles");
	{
		if (args->GetSize()) {
			apiServices->backupManager
				->QueryLocalBackupPackageReferencedFiles(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("createUserEnvironmentBackupPackage");
	{
		if (args->GetSize()) {
			apiServices->backupManager
				->CreateLocalBackupPackage(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("queryUserEnvironmentBackupPackageContent");
	{
		if (args->GetSize()) {
			apiServices->backupManager
				->QueryBackupPackageContent(args->GetValue(0),
							    result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("restoreUserEnvironmentBackupPackageContent");
	{
		if (args->GetSize()) {
			apiServices->backupManager
				->RestoreBackupPackageContent(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("beginDeferSaveTransaction");
	{
		result->SetString(
			apiServices->obsApiTransactionManager->Create());
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("completeDeferSaveTransaction");
	{
		if (args->GetSize() && args->GetType(0) == VTYPE_STRING) {
			apiServices->obsApiTransactionManager->Complete(
				args->GetString(0));

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addBrowserScopedHttpServer");
	{
		if (args->GetSize()) {
			apiServices->messageBus
				->DeserializeBrowserHttpServer(
					target->m_target, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllBrowserScopedHttpServers");
	{
		apiServices->messageBus->SerializeBrowserHttpServers(
			target->m_target, result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeBrowserScopedHttpServersByIds");
	{
		if (args->GetSize()) {
			apiServices->messageBus
				->RemoveBrowserHttpServersByIds(
					target->m_target, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("sendHttpRequestResponse");
	{
		if (args->GetSize() >= 2) {
			apiServices->messageBus
				->DeserializeHttpRequestResponse(
					args->GetValue(0), args->GetValue(1),
					result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setShowBuiltInMenuItems");
	{
		if (args->GetSize() >= 1 && args->GetType(0) == VTYPE_BOOL) {
			apiServices->menuManager->SetShowBuiltInMenuItems(
				args->GetBool(0));

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getShowBuiltInMenuItems");
	{
		result->SetBool(apiServices->menuManager
					->GetShowBuiltInMenuItems());
	}
	API_HANDLER_END();

	#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	API_HANDLER_BEGIN("showOutputPreviewTitleBar");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->nativeObsUIControlsManager
						->DeserializePreviewTitleBar(
							args->GetValue(0)));

			apiServices->statePersistence->PersistState();
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideOutputPreviewTitleBar");
	{
		apiServices->nativeObsUIControlsManager->HidePreviewTitleBar();

		result->SetBool(true);

		apiServices->statePersistence->PersistState();
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showOutputPreviewFrame");
	{
		if (args->GetSize()) {
			result->SetBool(apiServices->nativeObsUIControlsManager
						->DeserializePreviewFrame(
							args->GetValue(0)));

			apiServices->statePersistence->PersistState();
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("hideOutputPreviewFrame");
	{
		apiServices->nativeObsUIControlsManager->HidePreviewFrame();

		result->SetBool(true);

		apiServices->statePersistence->PersistState();
	}
	API_HANDLER_END();
	#endif

	API_HANDLER_BEGIN("getNativeStreamingServiceIntegrationStatus");
	{
		bool manageBroadcastButtonVisible =
			apiServices->nativeObsUIControlsManager
				->GetNativeManageBroadcastButtonVisibility();

		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();
		d->SetBool("manageBroadcastButton",
			   manageBroadcastButtonVisible);

		result->SetDictionary(d);

	}

	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllStreamingOutputs");
	{
		apiServices->outputManager->SerializeAllOutputs(
			StreamingOutput, result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addStreamingOutput");
	{
		if (args->GetSize()) {
			apiServices->outputManager->DeserializeOutput(
				StreamingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeStreamingOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->RemoveOutputsByIds(
				StreamingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("enableStreamingOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->EnableOutputsByIds(
				StreamingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("disableStreamingOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->DisableOutputsByIds(
				StreamingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllRecordingOutputs");
	{
		apiServices->outputManager->SerializeAllOutputs(
			RecordingOutput, result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addRecordingOutput");
	{
		if (args->GetSize()) {
			apiServices->outputManager->DeserializeOutput(
				RecordingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeRecordingOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->RemoveOutputsByIds(
				RecordingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("enableRecordingOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->EnableOutputsByIds(
				RecordingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("disableRecordingOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->DisableOutputsByIds(
				RecordingOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("triggerRecordingOutputSplitById");
	{
		if (args->GetSize()) {
			apiServices->outputManager
				->TriggerSplitRecordingOutputById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllReplayBufferOutputs");
	{
		apiServices->outputManager->SerializeAllOutputs(
			ReplayBufferOutput, result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addReplayBufferOutput");
	{
		if (args->GetSize()) {
			apiServices->outputManager->DeserializeOutput(
				ReplayBufferOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeReplayBufferOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->RemoveOutputsByIds(
				ReplayBufferOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("enableReplayBufferOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->EnableOutputsByIds(
				ReplayBufferOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("disableReplayBufferOutputsByIds");
	{
		if (args->GetSize()) {
			apiServices->outputManager->DisableOutputsByIds(
				ReplayBufferOutput, args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("triggerReplayBufferOutputSaveById");
	{
		if (args->GetSize()) {
			apiServices->outputManager
				->TriggerSaveReplayBufferById(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("readScopedStorageJsonItem");
	{
		if (args->GetSize()) {
			apiServices->config->ReadScopedJsonFile(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("writeScopedStorageJsonItem");
	{
		if (args->GetSize()) {
			apiServices->config->WriteScopedJsonFile(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeScopedStorageJsonItem");
	{
		if (args->GetSize()) {
			apiServices->config->RemoveScopedJsonFile(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllScopedStorageJsonItems");
	{
		if (args->GetSize()) {
			apiServices->config->ReadScopedJsonFilesList(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllAvailableVideoEncoderClasses");
	{
		apiServices->videoCompositionManager
			->SerializeAvailableVideoEncoderClasses(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllAvailableAudioEncoderClasses");
	{
		apiServices->videoCompositionManager
			->SerializeAvailableAudioEncoderClasses(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableAudioEncoderClassProperties");
	{
		apiServices->videoCompositionManager
			->SerializeAvailableAudioEncoderClassProperties(
				args->GetValue(0), result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableVideoEncoderClassProperties");
	{
		apiServices->videoCompositionManager
			->SerializeAvailableVideoEncoderClassProperties(
				args->GetValue(0), result);
	}
	API_HANDLER_END();


	API_HANDLER_BEGIN("getAllVideoCompositions");
	{
		apiServices->videoCompositionManager
			->SerializeAllCompositions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllAudioCompositions");
	{
		apiServices->audioCompositionManager
			->SerializeAllCompositions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeVideoCompositionsByIds");
	{
		if (args->GetSize()) {
			apiServices->videoCompositionManager
				->RemoveCompositionsByIds(args->GetValue(0),
							  result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeAudioCompositionsByIds");
	{
		if (args->GetSize()) {
			apiServices->audioCompositionManager
				->RemoveCompositionsByIds(args->GetValue(0),
							  result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addVideoComposition");
	{
		if (args->GetSize()) {
			apiServices->videoCompositionManager
				->DeserializeComposition(args->GetValue(0),
							 result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addAudioComposition");
	{
		if (args->GetSize()) {
			apiServices->audioCompositionManager
				->DeserializeComposition(args->GetValue(0),
							 result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setVideoCompositionProperties");
	{
		if (args->GetSize()) {
			apiServices->videoCompositionManager
				->DeserializeExistingCompositionProperties(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setAudioCompositionProperties");
	{
		if (args->GetSize()) {
			apiServices->audioCompositionManager
				->DeserializeExistingCompositionProperties(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeVideoCompositionViewOverlay");
	{
		auto browserWidget = self->GetBrowserWidget();

		if (browserWidget) {
			browserWidget->RemoveVideoCompositionView();

			result->SetBool(true);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getVideoCompositionViewOverlayProperties");
	{
		result->SetNull();

		auto browserWidget = self->GetBrowserWidget();

		if (browserWidget) {
			browserWidget->SerializeVideoCompositionView(result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setVideoCompositionViewOverlayProperties");
	{
		result->SetNull();

		if (args->GetSize()) {
			auto browserWidget = self->GetBrowserWidget();

			if (browserWidget) {
				browserWidget->DeserializeVideoCompositionView(
					args->GetValue(0),
					result);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("addSharedVideoComposition");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->sharedVideoCompositionManager
				->DeserializeSharedVideoComposition(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setSharedVideoCompositionProperties");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->sharedVideoCompositionManager
				->DeserializeSharedVideoCompositionProperties(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("removeSharedVideoCompositionsByIds");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->sharedVideoCompositionManager
				->RemoveSharedVideoCompositionsByIds(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllSharedVideoCompositions");
	{
		result->SetNull();

		apiServices->sharedVideoCompositionManager
			->SerializeAllSharedVideoCompositions(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("connectVideoCompositionToSharedVideoComposition");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->sharedVideoCompositionManager
				->ConnectVideoCompositionToSharedVideoComposition(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("disconnectVideoCompositionsFromSharedVideoCompositionsByIds");
	{
		result->SetNull();

		if (args->GetSize()) {
			apiServices->sharedVideoCompositionManager
				->DisconnectVideoCompositionsFromSharedVideoCompositionsByIds(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openSceneItemPropertiesDialogById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->OpenSceneItemPropertiesById(args->GetValue(0),
							      result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openSceneItemFiltersDialogById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->OpenSceneItemFiltersById(args->GetValue(0),
							   result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openSceneItemInteractionDialogById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->OpenSceneItemInteractionById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openSceneItemTransformEditorDialogById");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->OpenSceneItemTransformEditorById(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSceneItemRotationInViewport");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SerializeSceneItemViewportRotation(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setSceneItemRotationInViewport");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeSceneItemViewportRotation(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getSceneItemBoundingBoxInViewport");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->SerializeSceneItemViewportBoundingRectangle(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setSceneItemPositionInViewport");
	{
		if (args->GetSize()) {
			apiServices->obsSceneManager
				->DeserializeSceneItemViewportPosition(
					args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("takeScreenshot");
	{
		if (args->GetSize()) {
			auto input = args->GetValue(0);
			if (input->GetType() == VTYPE_DICTIONARY) {
				auto d = input->GetDictionary();

				auto videoCompositionManager =
					apiServices
						->videoCompositionManager;

				std::shared_ptr<
					SEVideoCompositionBase>
					videoComposition = nullptr;

				if (d->HasKey("videoCompositionId") &&
				    d->GetType("videoCompositionId") ==
					    VTYPE_STRING) {
					videoComposition =
						videoCompositionManager->GetVideoCompositionById(
							d->GetString(
								"videoCompositionId"));
				} else {
					videoComposition =
						videoCompositionManager
							->GetObsNativeVideoComposition();
				}

				if (videoComposition.get()) {
					videoComposition->TakeScreenshot();

					result->SetBool(true);
				}
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAvailableTransitionClasses");
	{
		apiServices->videoCompositionManager
			->SerializeAvailableTransitionClasses(result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getVideoCompositionTransition");
	{
		if (args->GetSize()) {
			auto videoComposition =
				apiServices->videoCompositionManager
					->GetVideoCompositionById(
						args->GetValue(0));

			if (videoComposition.get())
				videoComposition->SerializeTransition(result);
		} else {
			apiServices->videoCompositionManager
				->GetObsNativeVideoComposition()
				->SerializeTransition(result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("setVideoCompositionTransition");
	{
		if (args->GetSize()) {
			auto videoComposition =
				apiServices->videoCompositionManager
					->GetVideoCompositionById(
						args->GetValue(0));

			if (videoComposition.get())
				videoComposition->DeserializeTransition(args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showVideoCompositionTransitionPropertiesDialog");
	{
		if (args->GetSize()) {
			auto videoComposition =
				apiServices->videoCompositionManager
					->GetVideoCompositionById(
						args->GetValue(0));

			if (videoComposition.get()) {
				result->SetBool(
					videoComposition
						->ShowTransitionPropertiesDialog());
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("showPopupMenuAtMousePointerPosition");
	{
		if (args->GetSize()) {
			//
			// result is left alone when the menu does not
			// deserialize, exactly as before: a page gets false
			// on the renderer path and null on the direct one.
			//
			if (apiServices->auxiliaryControlsBuilder
				    ->ShowMenuAtMousePointerPosition(
					    args->GetValue(0))) {
				result->SetBool(true);
			}
		}
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("getAllLoadedHostModules");
	{
		apiServices->hostInfoProvider->SerializeLoadedHostModules(
			result);
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("openFileLocationInHostFileManager");
	{
		if (args->GetSize() > 0) {
			apiServices->shellServices->RevealFileInGraphicalShell(
				args->GetValue(0), result);
		}
	}
	API_HANDLER_END();

	//
	// API 6.8, registered only where SE_ENABLE_WYVRN is defined -- Windows
	// builds, since the option is forced off on every other platform.
	//
	// getHostCapabilities is grouped with the Razer WYVRN calls on purpose:
	// the `razerWyvrn` member is the only thing it has to report, so
	// registering it alone would publish a new API version whose sole
	// content is an object saying the feature is absent. The whole 6.8
	// surface is on or off together, and Version.hpp derives
	// HOST_API_VERSION_MINOR from the same switch.
	//
#ifdef SE_ENABLE_WYVRN
	//
	// Host capabilities. Documented since API 1.8 and never implemented until
	// now -- so nothing can depend on the old shape, and the documented
	// `sceneCollections` member is kept purely for fidelity with the
	// specification.
	//
	// This is deliberately the ONLY place WYVRN availability is reported. A
	// separate status call would be a second source of truth that could
	// disagree with this one.
	//
	API_HANDLER_BEGIN("getHostCapabilities");
	{
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		d->SetString("sceneCollections", "available");

		d->SetValue("razerWyvrn", SerializeRazerWyvrnStatus(apiServices));

		result->SetDictionary(d);
	}
	API_HANDLER_END();

	//
	// Every event declared by every WYVRN configuration installed on this
	// machine, each carrying its Chroma and haptic components and
	// session-signed URLs for their assets -- so a caller can see what firing
	// an event would actually do, and preview it, without a second call.
	//
	// Optional filter argument: { group, idPrefix }. There are ~4,000 events
	// on a machine with Synapse installed, so callers are expected to use it.
	//
	// An unavailable subsystem yields an empty array, never an error.
	//
	API_HANDLER_BEGIN("getAllRazerWyvrnEvents");
	{
		std::string groupFilter;
		std::string idPrefix;

		// Defaults to true: one call that fully answers "what would
		// this do" is the point of the call. Pass false to get ids
		// only -- unfiltered, that is the difference between 7.7 MB in
		// ~2.1 s and 0.28 MB in ~65 ms, and the whole request runs
		// inside the API lock.
		bool components = true;

		if (args->GetSize() > 0 &&
		    args->GetValue(0)->GetType() == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> d =
				args->GetValue(0)->GetDictionary();

			if (d->HasKey("group") &&
			    d->GetType("group") == VTYPE_STRING)
				groupFilter = d->GetString("group").ToString();

			if (d->HasKey("idPrefix") &&
			    d->GetType("idPrefix") == VTYPE_STRING)
				idPrefix = d->GetString("idPrefix").ToString();

			if (d->HasKey("components") &&
			    d->GetType("components") == VTYPE_BOOL)
				components = d->GetBool("components");
		}

		auto manager = GetRazerWyvrnManager(apiServices);

		if (manager.get()) {
			result = manager->SerializeEvents(groupFilter, idPrefix,
							  components);
		} else {
			result->SetList(CefListValue::Create());
		}
	}
	API_HANDLER_END();

	//
	// Fire an event.
	//
	// Accepts a RazerWyvrnEventInfo object rather than a bare string, so an
	// item from getAllRazerWyvrnEvents can be handed straight back; only
	// `id` and `fallback` are read. A bare string is accepted too.
	//
	// Stopping playback -- the SDK's own convention for an empty name -- is
	// spelled any of `null`, no argument at all, or an object whose `id` is
	// empty. All three mean the same thing, because a caller clearing an
	// event should not have to remember which shape we wanted.
	//
	// `fallback` names another event to try when this one is not declared by
	// any configuration on this machine, and nests to arbitrary depth:
	//
	//     { id: "Headshot",
	//       fallback: { id: "Hit", fallback: { id: "Generic_Impact" } } }
	//
	// The chain is resolved against the scan, not against the SDK: the SDK
	// accepts an event belonging to another application and reports success,
	// so asking it "did that work?" would always answer yes and the fallback
	// would never fire.
	//
	// The scan decides the *spelling*, not whether the call is allowed. When
	// nothing on this machine declares any name in the chain, the caller's
	// own spelling of the first one is sent anyway. The scan is a cache of
	// what was on disk when it ran, so it is not authoritative about what the
	// SDK will accept -- a configuration installed since then is invisible to
	// us and would otherwise be unreachable forever. Refusing on our own
	// stale evidence is worse than letting the SDK decide.
	//
	// Nothing here blocks. API_HANDLER_BEGIN holds a process-wide recursive
	// mutex that serialises every API call, and SetEventName only parks the
	// name for the SDK thread.
	//
	API_HANDLER_BEGIN("setRazerWyvrnEvent");
	{
		auto manager = GetRazerWyvrnManager(apiServices);

		// Bounded so a pathological structure cannot spin here while the
		// global API mutex is held.
		const int kMaxFallbackDepth = 16;

		std::string resolved;
		std::string firstId;
		std::string tried;
		bool stopRequested = false;
		bool sawCandidate = false;

		CefRefPtr<CefValue> arg =
			args->GetSize() > 0 ? args->GetValue(0) : nullptr;

		if (arg.get() && arg->GetType() == VTYPE_STRING) {
			const std::string id = arg->GetString().ToString();

			sawCandidate = !id.empty();
			stopRequested = id.empty();
			tried = id;
			firstId = id;

			if (manager.get() && !id.empty())
				resolved = manager->ResolveEventId(id);
		} else if (arg.get() && arg->GetType() == VTYPE_DICTIONARY) {
			CefRefPtr<CefDictionaryValue> d = arg->GetDictionary();

			for (int depth = 0;
			     d.get() && depth < kMaxFallbackDepth; ++depth) {
				std::string id;

				if (d->HasKey("id") &&
				    d->GetType("id") == VTYPE_STRING)
					id = d->GetString("id").ToString();

				if (id.empty() && !d->HasKey("fallback")) {
					// An explicit empty id, with nothing to
					// fall back to, is a stop.
					stopRequested = true;
					break;
				}

				if (!id.empty()) {
					sawCandidate = true;

					if (firstId.empty())
						firstId = id;

					tried += tried.empty() ? id
							       : (" -> " + id);

					if (manager.get()) {
						resolved =
							manager->ResolveEventId(
								id);

						if (!resolved.empty())
							break;
					}
				}

				if (d->HasKey("fallback") &&
				    d->GetType("fallback") == VTYPE_DICTIONARY)
					d = d->GetDictionary("fallback");
				else
					d = nullptr;
			}
		} else {
			// null, or no argument at all.
			stopRequested = true;
		}

		if (!manager.get()) {
			result->SetBool(false);
		} else if (stopRequested || !sawCandidate) {
			result->SetBool(manager->SetEventName(std::string()));
		} else if (!resolved.empty()) {
			result->SetBool(manager->SetEventName(resolved));
		} else {
			// Nothing scanned declares any of these names, so there
			// is no canonical spelling to correct to -- send the
			// caller's own, and let the SDK be the judge. Logged
			// because a name that renders nothing is worth seeing
			// when someone asks why an event did not fire.
			SELog(SE_LOG_INFO,
			      "obs-streamelements-core: WYVRN: no scanned configuration declares any of '%s'; sending '%s' as given",
			      tried.c_str(), firstId.c_str());

			result->SetBool(manager->SetEventName(firstId));
		}
	}
	API_HANDLER_END();
#endif

	API_HANDLER_BEGIN("crashProgram");
	{
		// Crash
		*(static_cast<volatile int*>(nullptr)) = 12345; // exception
		// *((int *)nullptr) = 12345; // exception

		(void)result;
	}
	API_HANDLER_END();

#ifdef WIN32
	// Windows only, and not for want of a macOS equivalent: the door this
	// exists to test is a WER runtime exception module, which is a Windows
	// mechanism. There is nothing on macOS for the call to prove.
	API_HANDLER_BEGIN("crashProgramFastFail");
	{
		// The counterpart to crashProgram above, and the only way to
		// exercise the fast-fail door (CORE-864).
		//
		// __fastfail raises STATUS_STACK_BUFFER_OVERRUN, which bypasses
		// SEH entirely: our top-level filter never runs, the SIGABRT
		// door never runs, and OBS's own handler never runs. The one
		// thing that can capture it is the WER runtime exception module,
		// out of process, after we are already dead -- so without a
		// trigger there is no way to confirm that path works until a
		// real user hits it.
		//
		// It matters that the call sits HERE, in the plug-in's own
		// module. The gate that module applies asks whether the crashed
		// thread's stack passes through our code, so a trigger placed
		// anywhere else would be declined by design -- and would prove
		// the opposite of what it set out to.
		//
		// Nothing runs after this line: not the UNUSED_PARAMETER, not
		// the handler's epilogue. The process is gone.
		__fastfail(FAST_FAIL_FATAL_APP_EXIT);
	}
	API_HANDLER_END();
#endif

	API_HANDLER_BEGIN("deadlockProgram");
	{
		//
		// Hangs the calling thread on purpose, to exercise the
		// deadlock detector. It waited five times on a libobs
		// auto-reset event that nothing ever signals; a condition
		// variable nothing notifies is the same hang with no libobs
		// and no platform code, and the predicate makes it immune to
		// a spurious wakeup -- wait() re-checks and goes back to
		// sleep, where os_event_wait() would have returned and fallen
		// through to the next of the five.
		//
		// The five calls were belt and braces for exactly that. One
		// is enough when the wait cannot be woken.
		//
		std::mutex deadlockMutex;
		std::condition_variable never;

		std::unique_lock<std::mutex> deadlockLock(deadlockMutex);

		never.wait(deadlockLock, []() { return false; });

		(void)result;
	}
	API_HANDLER_END();
}

bool SEApiMessageHandler::InvokeHandler::InvokeApiCallAsync(
	std::string invoke, CefRefPtr<CefListValue> args,
	std::function<void(CefRefPtr<CefValue>)> callback)
{
	if (!m_apiCallHandlers.count(invoke))
		return false;

	if (IsTraceLogLevel()) {
		SELog(SE_LOG_INFO,
		      "obs-streamelements-core: SEApiMessageHandler::InvokeHandler::InvokeApiCallAsync: '%s', [%d]",
		      invoke.c_str(), int(args->GetSize()));
	}

	incoming_call_handler_t handler = m_apiCallHandlers[invoke];

	struct local_context {
		CefRefPtr<CefValue> result;
		std::function<void(CefRefPtr<CefValue>)> callback;
	};

	local_context *context = new local_context();

	context->result = CefValue::Create();
	context->result->SetBool(false);
	context->callback = callback;

	static auto nullTarget = std::make_shared<SEApiClientInfo>("", "");

	auto apiServices = m_services.lock();

	if (!apiServices) {
		SELog(SE_LOG_WARNING,
		      "obs-streamelements-core: SEApiMessageHandler::InvokeHandler::InvokeApiCallAsync: not invoking '%s': the services bundle is gone",
		      invoke.c_str());

		delete context;

		return false;
	}

	handler(this->Clone(), apiServices, nullptr, args, context->result,
		nullTarget, -1, [context]() {
			context->callback(context->result);

			delete context;
		});

	return true;
}
