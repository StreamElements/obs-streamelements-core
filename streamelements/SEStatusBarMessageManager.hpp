#pragma once

#include "cef-headers.hpp"

class QMainWindow;

//
// Temporary messages in OBS's own status bar.
//
// One method and no state. It was a method on SEGlobalStateManager, and the
// only thing it needed from there was the main window pointer -- which is
// the whole reason a status bar message had to go through the singleton.
//
class SEStatusBarMessageManager {
public:
	SEStatusBarMessageManager(QMainWindow *mainWindow);
	~SEStatusBarMessageManager();

	bool DeserializeStatusBarTemporaryMessage(CefRefPtr<CefValue> input);

private:
	QMainWindow *m_mainWindow = nullptr;
};
