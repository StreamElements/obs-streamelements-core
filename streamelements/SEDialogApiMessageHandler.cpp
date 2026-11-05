#include "SEDialogApiMessageHandler.hpp"

#include <mutex>

//
// The API call lock for the three dialog handlers, and it must not be the one
// in SEApiMessageHandler.cpp.
//
// showModalDialog's body holds that one for as long as the modal dialog is
// up -- it calls SEBrowserDialogManager::DeserializeModalDialog, which calls
// exec() synchronously -- and these three are the only way that dialog ever
// closes. One domain and the exit path blocks on the lock its own opener
// holds, forever; the mutex being recursive does not help, because the two
// run on different threads.
//
// check_the_api_call_lock_stays_per_translation_unit is what stops the two
// definitions being tidied into one. See the comment on API_HANDLER_BEGIN.
//
static std::recursive_mutex s_sync_api_call_mutex;

//
// Three names, one behaviour, and that was true before this file existed:
// the page's three ways of ending a dialog grew separately rather than
// differing. EndDialogFromApiCall is the single body, and the one place
// their behaviour is asserted -- tests/test_dialog_api_call.cpp.
//
void SEDialogApiMessageHandler::RegisterIncomingApiCallHandlers()
{
	SEApiMessageHandler::RegisterIncomingApiCallHandlers();

	API_HANDLER_BEGIN("endModalDialog");
	{
		std::shared_ptr<SEDialogApiMessageHandler> handler =
			std::static_pointer_cast<SEDialogApiMessageHandler>(
				self);

		EndDialogFromApiCall(handler->DialogHost(), args, result,
				     [handler]() { handler->Shutdown(); });
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("endNonModalDialog");
	{
		std::shared_ptr<SEDialogApiMessageHandler> handler =
			std::static_pointer_cast<SEDialogApiMessageHandler>(
				self);

		EndDialogFromApiCall(handler->DialogHost(), args, result,
				     [handler]() { handler->Shutdown(); });
	}
	API_HANDLER_END();

	API_HANDLER_BEGIN("endDialog");
	{
		std::shared_ptr<SEDialogApiMessageHandler> handler =
			std::static_pointer_cast<SEDialogApiMessageHandler>(
				self);

		EndDialogFromApiCall(handler->DialogHost(), args, result,
				     [handler]() { handler->Shutdown(); });
	}
	API_HANDLER_END();
}
