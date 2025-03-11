#pragma once

#include "cef-headers.hpp"

#include <mutex>
#include <shared_mutex>
#include <functional>
#include <memory>

class StreamElementsBrowserWidget;

class StreamElementsApiMessageHandlerRuntimeStatus {
public:
	bool m_running = true;

	StreamElementsApiMessageHandlerRuntimeStatus() : m_running(true) {

	}
};

class StreamElementsApiMessageHandler : public std::enable_shared_from_this <
					StreamElementsApiMessageHandler> {
public:

private:
	std::shared_ptr<StreamElementsApiMessageHandlerRuntimeStatus>
		m_runtimeStatus = std::make_shared<
			StreamElementsApiMessageHandlerRuntimeStatus>();

protected:
	std::string m_containerType;
	StreamElementsBrowserWidget *m_browserWidget = nullptr;

public:
	StreamElementsApiMessageHandler(std::string containerType)
		: m_containerType(containerType)
	{
		// RegisterIncomingApiCallHandlers();
	}

	virtual ~StreamElementsApiMessageHandler() {
		m_runtimeStatus->m_running = false;
	}

	void Shutdown() { m_runtimeStatus->m_running = false; }

	//
	// Called by StreamElementsBrowserWidget ctor/dtor to set/unset m_browserWidget
	//
	// This is necessary for implementing API calls which impact the browser widget itself.
	//
	void SetBrowserWidget(StreamElementsBrowserWidget* browserWidget)
	{
		m_browserWidget = browserWidget;
	}

	StreamElementsBrowserWidget* GetBrowserWidget()
	{
		return m_browserWidget;
	}

public:
	virtual bool
	OnProcessMessageReceived(std::string source,
				 CefRefPtr<CefProcessMessage> message,
				 const long cefClientId);

	void setInitialHiddenState(bool isHidden)
	{
		m_initialHiddenState = isHidden;
	}

public:
	virtual std::shared_ptr<StreamElementsApiMessageHandler> Clone() {
		return shared_from_this();
	}

protected:
	virtual void RegisterIncomingApiCallHandlers();

	typedef void (*incoming_call_handler_t)(
		std::shared_ptr<StreamElementsApiMessageHandler>,
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,
		std::string target, const long cefClientId, std::function<void()> complete_callback);

	void RegisterIncomingApiCallHandler(std::string id,
					    incoming_call_handler_t handler);

	void InvokeApiCallHandlerAsync(
		CefRefPtr<CefProcessMessage> message,
		std::string target, std::string invokeId,
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

	void
	RegisterIncomingApiCallHandlersInternal(std::string target);
	void RegisterApiPropsInternal(std::string target);
	void DispatchHostReadyEventInternal(std::string target);
	void DispatchEventInternal(std::string target,
				   std::string event,
				   std::string eventArgsJson);

public:
	class InvokeHandler;
};

class StreamElementsApiMessageHandler::InvokeHandler
	: public StreamElementsApiMessageHandler {
public:
	InvokeHandler(std::string containerType)
		: StreamElementsApiMessageHandler(containerType)
	{
		// RegisterIncomingApiCallHandlers();
	}

public:
	~InvokeHandler() {}

	static std::shared_ptr<StreamElementsApiMessageHandler::InvokeHandler> GetInstance()
	{
		static std::mutex mutex;

		if (!s_singleton) {
			std::lock_guard<std::mutex> guard(mutex);

			if (!s_singleton) {
				s_singleton = std::make_shared<
					StreamElementsApiMessageHandler::
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
	static std::shared_ptr<StreamElementsApiMessageHandler::InvokeHandler> s_singleton;
};
