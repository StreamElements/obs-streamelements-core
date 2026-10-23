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
	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (d.get() && d->HasKey("text")) {
		int timeoutMilliseconds = 0;
		std::string text = d->GetString("text");

		if (d->HasKey("timeoutMilliseconds")) {
			timeoutMilliseconds = d->GetInt("timeoutMilliseconds");
		}

		m_mainWindow->statusBar()->showMessage(text.c_str(),
						       timeoutMilliseconds);

		return true;
	}

	return false;
}
