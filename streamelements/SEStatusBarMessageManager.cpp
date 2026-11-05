#include "SEStatusBarMessageManager.hpp"

#include <QMainWindow>
#include <QStatusBar>

#include <string>

SEStatusBarMessageManager::SEStatusBarMessageManager(QMainWindow *mainWindow)
	: m_mainWindow(mainWindow)
{
}

SEStatusBarMessageManager::~SEStatusBarMessageManager() {}

bool SEStatusBarMessageManager::DeserializeStatusBarTemporaryMessage(
	CefRefPtr<CefValue> input)
{
	//
	// `input` is whatever a page passed to
	// window.host.showStatusBarTemporaryMessage(), and every typed getter
	// on the CefValue model is `if (m_type != VTYPE_X) throw;` -- a bare
	// throw outside a catch, which is std::terminate rather than an
	// exception anything can catch. So a read of the wrong type here
	// takes the whole OBS process down.
	//
	// HasKey() is not a type check. CefDictionaryValueImpl::GetString(k)
	// is `m_map[k]->GetString()`, so a member read lands on the same
	// getter: the guard has to be HasKey(k) && GetType(k) == VTYPE_X.
	//
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("text") &&
	    d->GetType("text") == VTYPE_STRING) {
		int timeoutMilliseconds = 0;
		std::string text = d->GetString("text");

		if (d->HasKey("timeoutMilliseconds") &&
		    d->GetType("timeoutMilliseconds") == VTYPE_INT) {
			timeoutMilliseconds = d->GetInt("timeoutMilliseconds");
		}

		m_mainWindow->statusBar()->showMessage(text.c_str(),
						       timeoutMilliseconds);

		return true;
	}

	return false;
}
