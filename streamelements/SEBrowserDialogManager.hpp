#pragma once

#include "cef-headers.hpp"

#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class QMainWindow;
class SEBrowserDialog;
struct SEApiServices;

//
// The browser dialogs the host API opens: one modal, which blocks on its own
// exec(), and any number of non-modal ones, which are registered by id until
// they close.
//
// This was a map, a mutex and eight methods on SEGlobalStateManager. Moving
// it into a class that owns the registry is what lets the dialogs be closed
// by their owner: until now a non-modal dialog destroyed without emitting
// finished() parked the API handler's detached thread on future.wait()
// forever, because the promise existed only inside the finished handler's
// capture. The registry holds it now, so the destructor can fulfil it.
//
class SEBrowserDialogManager {
public:
	//
	// The bundle is kept weakly. Each dialog holds a strong reference to
	// it (and so does the SEDialogApiMessageHandler it creates), and this
	// destructor destroys the dialogs, so a strong reference here would
	// be a ring: bundle -> this -> (destructor) -> dialog -> bundle.
	//
	SEBrowserDialogManager(QMainWindow *mainWindow,
			       std::shared_ptr<SEApiServices> apiServices);
	~SEBrowserDialogManager();

	bool DeserializeModalDialog(CefRefPtr<CefValue> input,
				    CefRefPtr<CefValue> &output);

	std::shared_ptr<std::promise<CefRefPtr<CefValue>>>
	DeserializeNonModalDialog(CefRefPtr<CefValue> input);

	bool HasNonModalDialog(const char *id);
	std::string GetNonModalDialogUrl(const char *id);
	void SerializeAllNonModalDialogs(CefRefPtr<CefValue> &output);
	bool DeserializeCloseNonModalDialogsByIds(CefRefPtr<CefValue> input);
	bool DeserializeFocusNonModalDialogById(CefRefPtr<CefValue> input);
	bool DeserializeNonModalDialogDimensionsById(
		CefRefPtr<CefValue> dialogId, CefRefPtr<CefValue> dimensions);

private:
	QMainWindow *m_mainWindow = nullptr;
	std::weak_ptr<SEApiServices> m_apiServices;

	//
	// The promise travels with the dialog. It used to live only in the
	// finished handler's capture, which is why nothing but that handler
	// could ever satisfy it.
	//
	struct NonModalDialog {
		SEBrowserDialog *dialog = nullptr;
		std::shared_ptr<std::promise<CefRefPtr<CefValue>>> promise;
	};

	std::recursive_mutex m_mutex;
	std::map<std::string, NonModalDialog> m_nonModalDialogs;

	//
	// Disarms the queued finished handlers. They are captured weakly, so
	// one that runs after this object is gone sees an expired pointer;
	// one that runs during the destructor -- reject() turns Qt's event
	// loop, so that happens -- sees false.
	//
	std::shared_ptr<bool> m_alive = std::make_shared<bool>(true);
};
