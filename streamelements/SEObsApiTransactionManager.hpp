#pragma once

//
// Long-running host API calls that must not be interrupted: the caller opens
// a transaction, OBS stops autosaving its scene collection for the duration,
// and a "please wait" dialog goes up over the main window. A timeout closes
// an abandoned one so neither state can get stuck on.
//
// This was a file-static std::map plus a recursive_mutex inside
// SEUtils/SELifecycleUtils.cpp, reaching SEPleaseWaitWindow::GetInstance()
// for the dialog -- two singletons' worth of global state behind two free
// functions, whose only callers are two API handlers that already hold the
// services bundle (CORE-757).
//

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

class SEPleaseWaitWindow;

class SEObsApiTransactionManager
	: public std::enable_shared_from_this<SEObsApiTransactionManager> {
public:
	SEObsApiTransactionManager(
		std::shared_ptr<SEPleaseWaitWindow> pleaseWaitWindow);
	~SEObsApiTransactionManager();

public:
	//
	// Opens a transaction and returns its id. The first open shows the
	// dialog and begins the autosave deferral; both are undone when the
	// last one closes.
	//
	std::string Create(int timeoutMilliseconds = 60000);

	// Closes one by id. Unknown ids are ignored, which is what makes the
	// timeout and an explicit completion safe to race.
	void Complete(std::string id);

	//
	// The autosave deferral on its own, with no dialog and no timeout.
	//
	// batchInvokeSeries wraps a list of API calls in one deferral so that
	// OBS does not write its scene collection between them. It is not a
	// long operation anybody should be asked to wait for, which is why it
	// called obs_frontend_defer_save_begin/end straight from the handler
	// rather than opening a transaction -- so the one class that owns the
	// deferral did not own that use of it. libobs counts the deferrals,
	// so this nests with Create()/Complete() safely.
	//
	void BeginDeferSave();
	void EndDeferSave();

private:
	class Handle;

	//
	// Guarded, and recursive because Complete() is what the timeout
	// callback calls, and that can arrive while Create() still holds the
	// lock.
	//
	std::recursive_mutex m_mutex;
	std::map<std::string, Handle *> m_transactions;

	std::shared_ptr<SEPleaseWaitWindow> m_pleaseWaitWindow;
};
