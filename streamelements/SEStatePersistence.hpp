#pragma once

#include "cef-headers.hpp"

#include <memory>

class QMainWindow;
class SEBrowserWidgetManager;
class SEWorkerManager;
class SEHotkeyManager;
class SENativeObsUIControlsManager;
class SEHostEventDispatcher;
class SEConfig;

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
	//
	// The managers are borrowed, not owned. The owner is
	// SEGlobalStateManager, which keeps them alive for longer than this
	// object: SEApiServices destroys its fields in reverse declaration
	// order, and statePersistence is declared after all four, so it is
	// destroyed first. Raw pointers say that, and they keep this out of
	// the reference graph entirely -- a manager whose destructor deletes
	// widgets cannot be kept alive by the thing that serializes it.
	//
	SEStatePersistence(
		QMainWindow *mainWindow, SEBrowserWidgetManager *widgetManager,
		SEWorkerManager *workerManager, SEHotkeyManager *hotkeyManager,
		SENativeObsUIControlsManager *nativeObsUIControlsManager,
		std::shared_ptr<SEHostEventDispatcher> hostEventDispatcher,
		std::shared_ptr<SEConfig> config);
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

	SEBrowserWidgetManager *m_widgetManager = nullptr;
	SEWorkerManager *m_workerManager = nullptr;
	SEHotkeyManager *m_hotkeyManager = nullptr;
	SENativeObsUIControlsManager *m_nativeObsUIControlsManager = nullptr;

	std::shared_ptr<SEHostEventDispatcher> m_hostEventDispatcher;

	//
	// The whole of the persisted UI state is one base64 blob in the
	// plug-in's ini, so this is both the reader and the writer.
	//
	std::shared_ptr<SEConfig> m_config;

	bool m_enabled = false;
};
