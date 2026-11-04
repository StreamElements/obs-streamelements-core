#pragma once

#include "ISEStatusBarMessageManager.hpp"

#include "cef-headers.hpp"

class QMainWindow;

//
// Temporary messages in OBS's own status bar.
//
// One method and no state. It was a method on SEMain, and the only thing it
// needed from there was the main window pointer -- which is the whole reason a
// status bar message had to go through the singleton.
//
class SEStatusBarMessageManager : public ISEStatusBarMessageManager {
public:
	SEStatusBarMessageManager(QMainWindow *mainWindow);
	~SEStatusBarMessageManager();

	bool DeserializeStatusBarTemporaryMessage(
		CefRefPtr<CefValue> input) override;

private:
	QMainWindow *m_mainWindow = nullptr;
};
