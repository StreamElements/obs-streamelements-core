#pragma once

#include "cef-headers.hpp"

#include <mutex>
#include <shared_mutex>
#include <functional>
#include <memory>

#include "SEWebsocketApiServer.hpp"
#include "SEApiServices.hpp"

class SEBrowserWidget;

class SEApiMessageHandlerRuntimeStatus {
public:
	bool m_running = true;

	SEApiMessageHandlerRuntimeStatus() : m_running(true) {

	}
};

class SEApiMessageHandler : public std::enable_shared_from_this <
					SEApiMessageHandler> {
private:
	std::shared_ptr<SEApiMessageHandlerRuntimeStatus>
		m_runtimeStatus = std::make_shared<
			SEApiMessageHandlerRuntimeStatus>();

protected:
	std::string m_containerType;
	SEBrowserWidget *m_browserWidget = nullptr;

	//
	// The services this handler's methods are allowed to use. Injected, so
	// a handler can be built against anything -- including a bundle with
	// only the two services a test cares about.
	//
	// Weak, because a handler is owned by a browser widget, which is
	// owned by a dock or a dialog that one of the bundle's own managers
	// destroys. Handlers do not read it: the dispatcher locks it once per
	// call and passes the result in, so the four sites left here are the
	// ones that are not dispatches.
	//
	std::weak_ptr<SEApiServices> m_services;

public:
	SEApiMessageHandler(std::string containerType,
			    std::shared_ptr<SEApiServices> services)
		: m_containerType(containerType), m_services(services)
	{
		// RegisterIncomingApiCallHandlers();
	}

	virtual ~SEApiMessageHandler() {
		Shutdown();
	}

	void Shutdown()
	{
		m_runtimeStatus->m_running = false;
	}

	//
	// Called by SEBrowserWidget ctor/dtor to set/unset m_browserWidget
	//
	// This is necessary for implementing API calls which impact the browser widget itself.
	//
	void SetBrowserWidget(SEBrowserWidget* browserWidget)
	{
		m_browserWidget = browserWidget;
	}

	SEBrowserWidget* GetBrowserWidget()
	{
		return m_browserWidget;
	}

public:
	virtual bool OnProcessMessageReceived(
		std::shared_ptr<SEWebsocketApiServer::ClientInfo>
			source,
		CefRefPtr<CefProcessMessage> message, const long cefClientId);

	void setInitialHiddenState(bool isHidden)
	{
		m_initialHiddenState = isHidden;
	}

public:
	virtual std::shared_ptr<SEApiMessageHandler> Clone() {
		return shared_from_this();
	}

protected:
	//
	// Whether a dispatch may run at all: obs is up, the services bundle
	// is still there, and the root has finished wiring it together. It
	// was a file-static asking SEGlobalStateManager whether the plug-in
	// was available, which is the same question put to global state
	// (CORE-757).
	//
	bool IsPluginInitialized();

	virtual void RegisterIncomingApiCallHandlers();

	//
	// apiServices is locked by InvokeApiCallHandlerAsync before the
	// handler is looked up, so a method body can use it without checking
	// -- a call that arrives with no bundle is answered with null and
	// never reaches a handler. It is also what makes a handler callable
	// from a test with a hand-built bundle and no singleton anywhere.
	//
	typedef void (*incoming_call_handler_t)(
		std::shared_ptr<SEApiMessageHandler>,
		std::shared_ptr<SEApiServices> apiServices,
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,
		std::shared_ptr<SEWebsocketApiServer::ClientInfo> target, const long cefClientId, std::function<void()> complete_callback);

	void RegisterIncomingApiCallHandler(std::string id,
					    incoming_call_handler_t handler);

	void InvokeApiCallHandlerAsync(
		CefRefPtr<CefProcessMessage> message,
		std::shared_ptr<SEWebsocketApiServer::ClientInfo>
			target,
		std::string invokeId,
		CefRefPtr<CefListValue> invokeArgs,
		std::function<void(CefRefPtr<CefValue>)> result_callback,
		const long cefClientId,
		const bool enable_logging = false);

#if ENABLE_CREATE_BROWSER_API
public:
	CefRefPtr<CefDictionaryValue> CreateBrowserArgsDictionary();

private:
	CefRefPtr<CefDictionaryValue> CreateApiSpecDictionaryInternal();
#endif

private:
	std::shared_mutex m_processMessageReceivedMutex;

	std::map<std::string, incoming_call_handler_t> m_apiCallHandlers;
	bool m_initialHiddenState = false;

	CefRefPtr<CefDictionaryValue> CreateApiCallHandlersDictionaryInternal();
	CefRefPtr<CefDictionaryValue> CreateApiPropsDictionaryInternal();

	void RegisterIncomingApiCallHandlersInternal(
		std::shared_ptr<SEWebsocketApiServer::ClientInfo>
			target);
	void RegisterApiPropsInternal(
		std::shared_ptr<SEWebsocketApiServer::ClientInfo>
			target);
	void DispatchHostReadyEventInternal(
		std::shared_ptr<SEWebsocketApiServer::ClientInfo>
			target);
	void DispatchEventInternal(
		std::shared_ptr<SEWebsocketApiServer::ClientInfo>
			target,
				   std::string event,
				   std::string eventArgsJson);

public:
	class InvokeHandler;
};

class SEApiMessageHandler::InvokeHandler
	: public SEApiMessageHandler {
public:
	InvokeHandler(std::string containerType,
		      std::shared_ptr<SEApiServices> services)
		: SEApiMessageHandler(containerType, services)
	{
		// We must init this here since Invoke handler does not get "context initialized" event from
		// the browser script which does not exist here :)
		RegisterIncomingApiCallHandlers();
	}

public:
	~InvokeHandler() {}

	//
	// Owned by SEAuxiliaryControlsBuilder, which is the only thing that
	// invokes an auxiliary action. It was a singleton for as long as its
	// caller was a free function with nothing to hand it: a file-static
	// shared_ptr, never released, which kept the services bundle's
	// refcount off zero even with no widget alive.
	//
	bool
	InvokeApiCallAsync(std::string invoke, CefRefPtr<CefListValue> args,
			   std::function<void(CefRefPtr<CefValue>)> callback);
};

//
// The API handler macros: one definition, expanded in every translation unit
// that registers host API methods.
//
// API_HANDLER_BEGIN opens a registration and names the variables a body may
// use -- self, apiServices, message, args, result, target, cefClientId and
// complete_callback -- then takes the API call lock. API_HANDLER_END closes
// the body and calls complete_callback for you; API_HANDLER_END_ASYNC closes
// it without calling anything, for a body that hands the work off and must
// call complete_callback itself on every path, error paths included.
//
// *** The lock is deliberately per-translation-unit, and must stay that way. ***
//
// Each expanding TU defines its own `static std::recursive_mutex
// s_sync_api_call_mutex`, and this macro picks up whichever one is in scope.
// That looks like an accident and is not: the handlers that end a dialog have
// to be in a different lock domain from the handler that opened it.
//
// showModalDialog's body calls SEBrowserDialogManager::DeserializeModalDialog,
// which calls dialog->exec() synchronously, so it holds the lock of the TU it
// was compiled in for the entire lifetime of the modal dialog. The only way
// that dialog ever closes is its own endModalDialog / endDialog handler. Put
// both in one domain and the exit path blocks on the lock its own opener is
// holding, forever -- the mutex being recursive does not help, because the
// two run on different threads.
//
// So a TU that expands these macros must define s_sync_api_call_mutex itself.
// Forgetting to is a compile error, which is the intended outcome;
// check_the_api_call_lock_stays_per_translation_unit is what stops the two
// definitions being "simplified" into one.
//
#define API_HANDLER_BEGIN(name)                                               \
	RegisterIncomingApiCallHandler(                                        \
		name,                                                          \
		[](std::shared_ptr<SEApiMessageHandler> self,                  \
		   std::shared_ptr<SEApiServices> apiServices,                 \
		   CefRefPtr<CefProcessMessage> message,                       \
		   CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,  \
		   std::shared_ptr<SEWebsocketApiServer::ClientInfo> target,   \
		   const long cefClientId,                                     \
		   std::function<void()> complete_callback) {                  \
			(void)self;                                            \
			(void)apiServices;                                     \
			(void)message;                                         \
			(void)args;                                            \
			(void)result;                                          \
			(void)target;                                          \
			(void)cefClientId;                                     \
			(void)complete_callback;                               \
			std::lock_guard<std::recursive_mutex> _api_sync_guard( \
				s_sync_api_call_mutex);

#define API_HANDLER_END()    \
	complete_callback(); \
	});

#define API_HANDLER_END_ASYNC() \
	});
