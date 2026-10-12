#pragma once

#include "cef-headers.hpp"

#include <mutex>
#include <shared_mutex>
#include <functional>
#include <memory>

#include "SEWebsocketApiServer.hpp"

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

public:
	SEApiMessageHandler(std::string containerType)
		: m_containerType(containerType)		  
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
	virtual void RegisterIncomingApiCallHandlers();

	typedef void (*incoming_call_handler_t)(
		std::shared_ptr<SEApiMessageHandler>,
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
	InvokeHandler(std::string containerType)
		: SEApiMessageHandler(containerType)
	{
		// We must init this here since Invoke handler does not get "context initialized" event from
		// the browser script which does not exist here :)
		RegisterIncomingApiCallHandlers();
	}

public:
	~InvokeHandler() {}

	static std::shared_ptr<SEApiMessageHandler::InvokeHandler> GetInstance()
	{
		static std::mutex mutex;

		if (!s_singleton) {
			std::lock_guard<std::mutex> guard(mutex);

			if (!s_singleton) {
				s_singleton = std::make_shared<
					SEApiMessageHandler::
						InvokeHandler>("globalInvokeHandler");
			}
		}

		return s_singleton;
	}

public:
	bool
	InvokeApiCallAsync(std::string invoke, CefRefPtr<CefListValue> args,
			   std::function<void(CefRefPtr<CefValue>)> callback);

private:
	static std::shared_ptr<SEApiMessageHandler::InvokeHandler> s_singleton;
};
