#pragma once

#include "StreamElementsBrowserMessageHandler.hpp"

#include <mutex>
#include <functional>

class StreamElementsApiMessageHandler
	: public StreamElementsBrowserMessageHandler {
public:
	StreamElementsApiMessageHandler() { RegisterIncomingApiCallHandlers(); }
	virtual ~StreamElementsApiMessageHandler() {}

public:
	virtual bool
	OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
#if CHROME_VERSION_BUILD >= 3770
				 CefRefPtr<CefFrame> frame,
#endif
				 CefProcessId source_process,
				 CefRefPtr<CefProcessMessage> message,
				 const long cefClientId) override;

	void setInitialHiddenState(bool isHidden)
	{
		m_initialHiddenState = isHidden;
	}

protected:
	virtual void RegisterIncomingApiCallHandlers();

	typedef void (*incoming_call_handler_t)(
		StreamElementsApiMessageHandler *,
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefListValue> args, CefRefPtr<CefValue> &result,
		CefRefPtr<CefBrowser> browser, const long cefClientId, std::function<void()> complete_callback);

	void RegisterIncomingApiCallHandler(std::string id,
					    incoming_call_handler_t handler);

	void InvokeApiCallHandlerAsync(
		CefRefPtr<CefProcessMessage> message,
		CefRefPtr<CefBrowser> browser, std::string invokeId,
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
	std::map<std::string, incoming_call_handler_t> m_apiCallHandlers;
	bool m_initialHiddenState = false;

	CefRefPtr<CefDictionaryValue> CreateApiCallHandlersDictionaryInternal();
	CefRefPtr<CefDictionaryValue> CreateApiPropsDictionaryInternal();

	void
	RegisterIncomingApiCallHandlersInternal(CefRefPtr<CefBrowser> browser);
	void RegisterApiPropsInternal(CefRefPtr<CefBrowser> browser);
	void DispatchHostReadyEventInternal(CefRefPtr<CefBrowser> browser);
	void DispatchEventInternal(CefRefPtr<CefBrowser> browser,
				   std::string event,
				   std::string eventArgsJson);

public:
	class InvokeHandler;

public:
	IMPLEMENT_REFCOUNTING(StreamElementsApiMessageHandler);
};

class StreamElementsApiMessageHandler::InvokeHandler
	: public StreamElementsApiMessageHandler {
public:
	InvokeHandler()
	{
		RegisterIncomingApiCallHandlers();
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
						InvokeHandler>();
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
