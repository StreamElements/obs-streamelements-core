#pragma once

#include "cef-headers.hpp"

#include <future>
#include <memory>

//
// What the host API needs from the browser dialog manager: the six
// calls behind the modal and non-modal dialog methods.
//
// DeserializeNonModalDialog's return type is the load-bearing part of
// this contract. Every promise it hands back is either stored against a
// live dialog or already satisfied -- a description it refuses resolves
// with null rather than being dropped -- which is what lets
// showNonModalDialog park a detached thread on it without leaking one.
// A change that made the refusal path return early instead would break
// exactly that, so the property is stated here as well as at the
// definition.
//
// The destructor that satisfies the promises of dialogs still open at
// teardown stays off this contract: only the root's destruction order
// reaches it.
//
class ISEBrowserDialogManager {
public:
	virtual ~ISEBrowserDialogManager() {}

	//
	// Closes the listed non-modal dialogs. True if any closed.
	//
	virtual bool
	DeserializeCloseNonModalDialogsByIds(CefRefPtr<CefValue> input) = 0;

	//
	// Raises and focuses one non-modal dialog, named by id.
	//
	virtual bool
	DeserializeFocusNonModalDialogById(CefRefPtr<CefValue> input) = 0;

	//
	// Opens a modal dialog and blocks until it closes, writing its result
	// through output. It holds the calling translation unit's API lock for
	// the dialog's whole lifetime, which is why the dialog's own end*
	// handlers are compiled into a different one.
	//
	virtual bool DeserializeModalDialog(CefRefPtr<CefValue> input,
					    CefRefPtr<CefValue> &output) = 0;

	//
	// Opens a non-modal dialog and answers a promise for its result,
	// satisfied when the dialog closes. Never null, and never
	// unsatisfiable: see above.
	//
	virtual std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
	DeserializeNonModalDialog(CefRefPtr<CefValue> input) = 0;

	//
	// Resizes one non-modal dialog, named by id.
	//
	virtual bool DeserializeNonModalDialogDimensionsById(
		CefRefPtr<CefValue> dialogId,
		CefRefPtr<CefValue> dimensions) = 0;

	//
	// Every non-modal dialog currently open, keyed by id.
	//
	virtual void
	SerializeAllNonModalDialogs(CefRefPtr<CefValue> &output) = 0;
};
