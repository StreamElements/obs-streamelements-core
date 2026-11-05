#pragma once

//
// What endModalDialog, endNonModalDialog and endDialog do, and the dialog
// they do it to.
//
// The three host API methods a page inside an SE.Live dialog uses to close
// itself and hand a value back were three copies of one body, registered in
// SEBrowserDialog.cpp -- a QDialog translation unit that neither ratchet
// reads. The copies had already drifted: endDialog sets the result before
// shutting the handler down and the other two after, which is immaterial
// because the two statements are independent, but it is the kind of drift
// that stops being immaterial without anyone deciding so.
//
// One body now, here, behind an interface. It names no Qt type, so it can be
// compiled and asserted against a recording dialog -- which is the only
// coverage these three have ever had, and they are the only way a modal
// SE.Live dialog ever closes.
//

#include "cef-headers.hpp"

#include <functional>
#include <string>

//
// A dialog, as its own page's API methods see it: somewhere to put the
// result, and a way to say it is done.
//
// SEBrowserDialog implements it. Accept() is asynchronous there -- a queued
// invocation of QDialog::accept(), because the call arrives on a thread that
// is not Qt's and the dialog must not be torn down underneath the handler
// that is still running.
//
class ISEDialogHost {
public:
	virtual ~ISEDialogHost() {}

	//
	// JSON, or the literal "null". Read back by whoever opened the
	// dialog, after it closes.
	//
	virtual void SetResultJson(std::string json) = 0;

	virtual void Accept() = 0;
};

//
// Called by all three registrations. Returns with result left as the caller
// set it when there is nothing to do, which is what the three did
// individually:
//
//   - result is false on every path, success included. A page cannot tell
//     "closed" from "ignored" by the return value, and could not before
//     either; recorded rather than changed, because changing it changes what
//     every existing dialog page sees.
//
//   - no arguments at all means the call is ignored: no result, no shutdown
//     and no close. The dialog stays open.
//
// stopHandler is the handler's own Shutdown(), passed in rather than called
// here: it stops queued messages being processed, and the only reason it is
// a parameter is that this function deliberately knows nothing about
// handlers.
//
void EndDialogFromApiCall(ISEDialogHost *dialog, CefRefPtr<CefListValue> args,
			  CefRefPtr<CefValue> &result,
			  std::function<void()> stopHandler);
