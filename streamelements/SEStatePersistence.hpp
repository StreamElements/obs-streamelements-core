#pragma once

#include "cef-headers.hpp"

#include <memory>

class QMainWindow;
class SEBrowserWidgetManager;
class SEWorkerManager;
class SEHotkeyManager;
class SENativeOBSControlsManager;

//
// Collects what the user arranged -- docks, the notification bar, background
// workers, hotkey bindings, the main window's geometry -- writes it to the
// config, and puts it back on the next launch.
//
// It was four methods on SEGlobalStateManager, which is why anything that
// wanted to save state had to reach for the singleton to do it. The managers
// it reads are injected, so it can be driven with any set of them.
//
class SEStatePersistence {
public:
	SEStatePersistence(QMainWindow *mainWindow,
			   std::shared_ptr<SEBrowserWidgetManager> widgetManager,
			   std::shared_ptr<SEWorkerManager> workerManager,
			   std::shared_ptr<SEHotkeyManager> hotkeyManager,
			   std::shared_ptr<SENativeOBSControlsManager>
				   nativeObsControlsManager);
	~SEStatePersistence();

	//
	// Off until the previous session's state has been restored, and off
	// again from the moment shutdown begins: a save in either window
	// writes half-built or half-destroyed state over the real thing.
	//
	void SetEnabled(bool enabled) { m_enabled = enabled; }

	void PersistState(bool sendEventToGuest = true);
	void RestoreState();

	void SerializeUserInterfaceState(CefRefPtr<CefValue> &output);
	bool DeserializeUserInterfaceState(CefRefPtr<CefValue> input);

private:
	QMainWindow *m_mainWindow = nullptr;

	std::shared_ptr<SEBrowserWidgetManager> m_widgetManager;
	std::shared_ptr<SEWorkerManager> m_workerManager;
	std::shared_ptr<SEHotkeyManager> m_hotkeyManager;
	std::shared_ptr<SENativeOBSControlsManager> m_nativeObsControlsManager;

	bool m_enabled = false;
};
