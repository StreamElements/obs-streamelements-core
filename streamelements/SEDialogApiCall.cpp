#include "SEDialogApiCall.hpp"

void EndDialogFromApiCall(ISEDialogHost *dialog, CefRefPtr<CefListValue> args,
			  CefRefPtr<CefValue> &result,
			  std::function<void()> stopHandler)
{
	result->SetBool(false);

	if (!args || !args->GetSize())
		return;

	if (!dialog)
		return;

	CefRefPtr<CefValue> arg = args->GetValue(0);

	//
	// Before anything else, so a message that is already queued for this
	// handler does not run against a dialog on its way out.
	//
	if (stopHandler)
		stopHandler();

	std::string json = "null";

	if (arg && arg->GetType() != VTYPE_NULL) {
		CefString written = CefWriteJSON(arg, JSON_WRITER_DEFAULT);

		//
		// The size check was in all three copies and is kept, but it
		// cannot fire: the local CefValue model serializes through
		// nlohmann::json::dump(), which produces "null" for anything
		// it cannot represent, so the result is never shorter than
		// four characters. Kept because it costs nothing and states
		// the intent -- an argument that serialized to nothing would
		// leave an empty string where the caller expects JSON --
		// and pinned by an assertion in test_dialog_api_call.cpp, so
		// that if the model ever can return nothing, the guard stops
		// being dead and somebody is told.
		//
		if (written.size())
			json = written.ToString();
	}

	dialog->SetResultJson(json);

	dialog->Accept();
}
