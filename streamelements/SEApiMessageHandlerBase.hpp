#pragma once

//
// The API handler infrastructure: the registration map, the dispatcher, the
// per-call locking of the services bundle, the API-context stack the crash
// handler reads, and the renderer handshake that builds window.host.
//
// It knows nothing about any particular host API method.
// RegisterIncomingApiCallHandlers() is pure virtual, so this class cannot be
// instantiated and every registration lives in a subclass:
// SEApiMessageHandler has the 208 host API methods, SEDialogApiMessageHandler
// adds the three that close a dialog, and a test can subclass this with three
// fake registrations and exercise the dispatcher without libobs or Qt.
//
// That last one is the reason for the split. The 208 bodies dereference 27
// concrete services, so their translation unit pulls in obs.h and Qt and
// cannot be compiled in tests/ until part 2 replaces those types with
// interfaces. This half dereferences almost nothing, and its include closure
// is already clean.
//

#include "cef-headers.hpp"

#include <mutex>
#include <shared_mutex>
#include <functional>
#include <memory>

#include "SEApiMessageSink.hpp"
#include "SEApiServices.hpp"
#include "ISEVideoCompositionViewHost.hpp"

class SEApiMessageHandlerRuntimeStatus {
public:
	bool m_running = true;

	SEApiMessageHandlerRuntimeStatus() : m_running(true) {}
};

class SEApiMessageHandlerBase
	: public std::enable_shared_from_this<SEApiMessageHandlerBase> {
private:
	std::shared_ptr<SEApiMessageHandlerRuntimeStatus> m_runtimeStatus =
		std::make_shared<SEApiMessageHandlerRuntimeStatus>();

protected:
	std::string m_containerType;
	//
	// Held as the contract rather than as SEBrowserWidget *, so that
	// this header needs no Qt and the three handler bodies that use
	// it need no complete widget type. The upcast happens in
	// SetBrowserWidget's two callers, both inside SEBrowserWidget
	// itself.
	//
	ISEVideoCompositionViewHost *m_browserWidget = nullptr;

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
	SEApiMessageHandlerBase(std::string containerType,
				std::shared_ptr<SEApiServices> services)
		: m_containerType(containerType), m_services(services)
	{
		// RegisterIncomingApiCallHandlers();
	}

	virtual ~SEApiMessageHandlerBase() { Shutdown(); }

	void Shutdown() { m_runtimeStatus->m_running = false; }

	//
	// Called by SEBrowserWidget ctor/dtor to set/unset m_browserWidget
	//
	// This is necessary for implementing API calls which impact the browser widget itself.
	//
	void SetBrowserWidget(ISEVideoCompositionViewHost *browserWidget)
	{
		m_browserWidget = browserWidget;
	}

	ISEVideoCompositionViewHost *GetBrowserWidget()
	{
		return m_browserWidget;
	}

	//
	// The handler-facing surface. A registered method body is a
	// capture-less lambda, which is a free function with a
	// SEApiMessageHandlerBase pointer -- so anything it reads on the
	// handler has to be public, protected is not enough. These three are
	// the whole of what the 208 bodies need.
	//
	bool IsRunning() const { return m_runtimeStatus->m_running; }

	const std::string &ContainerType() const { return m_containerType; }

public:
	virtual bool
	OnProcessMessageReceived(std::shared_ptr<SEApiClientInfo> source,
				 CefRefPtr<CefProcessMessage> message,
				 const long cefClientId);

	void setInitialHiddenState(bool isHidden)
	{
		m_initialHiddenState = isHidden;
	}

public:
	virtual std::shared_ptr<SEApiMessageHandlerBase> Clone()
	{
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

	//
	// Pure, so the infrastructure cannot be instantiated on its
	// own and every subclass states what it registers.
	//
	virtual void RegisterIncomingApiCallHandlers() = 0;

	//
	// apiServices is locked by InvokeApiCallHandlerAsync before the
	// handler is looked up, so a method body can use it without checking
	// -- a call that arrives with no bundle is answered with null and
	// never reaches a handler. It is also what makes a handler callable
	// from a test with a hand-built bundle and no singleton anywhere.
	//
	typedef void (*incoming_call_handler_t)(
		std::shared_ptr<SEApiMessageHandlerBase>,
		std::shared_ptr<SEApiServices> apiServices,
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,
		std::shared_ptr<SEApiClientInfo> target, const long cefClientId,
		std::function<void()> complete_callback);

	void RegisterIncomingApiCallHandler(std::string id,
					    incoming_call_handler_t handler);

public:
	void InvokeApiCallHandlerAsync(
		CefRefPtr<CefProcessMessage> message,
		std::shared_ptr<SEApiClientInfo> target, std::string invokeId,
		CefRefPtr<CefListValue> invokeArgs,
		std::function<void(CefRefPtr<CefValue>)> result_callback,
		const long cefClientId, const bool enable_logging = false);

#if ENABLE_CREATE_BROWSER_API
public:
	CefRefPtr<CefDictionaryValue> CreateBrowserArgsDictionary();

private:
	CefRefPtr<CefDictionaryValue> CreateApiSpecDictionaryInternal();
#endif

private:
	std::shared_mutex m_processMessageReceivedMutex;

protected:
	//
	// Protected rather than private: InvokeHandler looks a method up in
	// here to invoke it directly, without a renderer round trip.
	//
	std::map<std::string, incoming_call_handler_t> m_apiCallHandlers;

private:
	bool m_initialHiddenState = false;

	CefRefPtr<CefDictionaryValue> CreateApiCallHandlersDictionaryInternal();
	CefRefPtr<CefDictionaryValue> CreateApiPropsDictionaryInternal();

	void RegisterIncomingApiCallHandlersInternal(
		std::shared_ptr<SEApiClientInfo> target);
	void RegisterApiPropsInternal(std::shared_ptr<SEApiClientInfo> target);
	void
	DispatchHostReadyEventInternal(std::shared_ptr<SEApiClientInfo> target);
	void DispatchEventInternal(std::shared_ptr<SEApiClientInfo> target,
				   std::string event,
				   std::string eventArgsJson);
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
		[](std::shared_ptr<SEApiMessageHandlerBase> self,                  \
		   std::shared_ptr<SEApiServices> apiServices,                 \
		   CefRefPtr<CefProcessMessage> message,                       \
		   CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,  \
		   std::shared_ptr<SEApiClientInfo> target,   \
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
