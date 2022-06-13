#include "StreamElementsBrowserSourceApiMessageHandler.hpp"
#include "StreamElementsMessageBus.hpp"

#include <mutex>

static std::recursive_mutex s_sync_api_call_mutex;

#define API_HANDLER_BEGIN(name)                          \
	RegisterIncomingApiCallHandler(name, []( \
		StreamElementsApiMessageHandler*, \
		CefRefPtr<CefProcessMessage> message, \
		CefRefPtr<CefListValue> args, \
		CefRefPtr<CefValue>& result, \
		std::string target, \
		const long cefClientId, \
		std::function<void()> complete_callback) \
		{ \
			(void)message; \
			(void)args; \
			(void)result; \
			(void)target; \
			(void)cefClientId; \
			(void)complete_callback; \
			std::lock_guard<std::recursive_mutex> _api_sync_guard(s_sync_api_call_mutex);
#define API_HANDLER_END()    \
	complete_callback(); \
	});

StreamElementsBrowserSourceApiMessageHandler::StreamElementsBrowserSourceApiMessageHandler()
{

}

StreamElementsBrowserSourceApiMessageHandler::~StreamElementsBrowserSourceApiMessageHandler()
{

}

void StreamElementsBrowserSourceApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	API_HANDLER_BEGIN("broadcastMessage")
		if (args->GetSize()) {
			StreamElementsMessageBus::GetInstance()->NotifyAllMessageListeners(
				StreamElementsMessageBus::DEST_ALL_LOCAL,
				StreamElementsMessageBus::SOURCE_WEB,
				"urn:streamelements:internal:" + target,
				args->GetValue(0));

			result->SetBool(true);
		}
	API_HANDLER_END()
}
