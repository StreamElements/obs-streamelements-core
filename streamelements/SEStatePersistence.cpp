#include "SEStatePersistence.hpp"

#include "SEUtils.hpp"
#include "SEConfig.hpp"
#include "base64/base64.hpp"
#include "SEBrowserWidgetManager.hpp"
#include "SEWorkerManager.hpp"
#include "SEHotkeyManager.hpp"
#include "SENativeOBSControlsManager.hpp"

#include <QMainWindow>
#include <QApplication>
#include <QScreen>

SEStatePersistence::SEStatePersistence(
	QMainWindow *mainWindow, SEBrowserWidgetManager *widgetManager,
	SEWorkerManager *workerManager, SEHotkeyManager *hotkeyManager,
	SENativeOBSControlsManager *nativeObsControlsManager)
	: m_mainWindow(mainWindow),
	  m_widgetManager(widgetManager),
	  m_workerManager(workerManager),
	  m_hotkeyManager(hotkeyManager),
	  m_nativeObsControlsManager(nativeObsControlsManager)
{
}

SEStatePersistence::~SEStatePersistence() {}

void SEStatePersistence::PersistState(bool sendEventToGuest)
{
	PREVENT_RECURSIVE_REENTRY();

	if (!m_enabled) {
		return;
	}

	CefRefPtr<CefValue> root = CefValue::Create();
	CefRefPtr<CefDictionaryValue> rootDictionary =
		CefDictionaryValue::Create();

	CefRefPtr<CefValue> dockingWidgetsState = CefValue::Create();
	CefRefPtr<CefValue> notificationBarState = CefValue::Create();
	CefRefPtr<CefValue> workersState = CefValue::Create();
	CefRefPtr<CefValue> hotkeysState = CefValue::Create();
	CefRefPtr<CefValue> userInterfaceState = CefValue::Create();

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	CefRefPtr<CefValue> outputPreviewTitleBarState = CefValue::Create();
	CefRefPtr<CefValue> outputPreviewFrameState = CefValue::Create();
#endif

	if (m_widgetManager) {
		m_widgetManager->SerializeDockingWidgets(dockingWidgetsState);
		m_widgetManager->SerializeNotificationBar(notificationBarState);
	}

	if (m_workerManager) {
		m_workerManager->Serialize(workersState);
	}

	if (m_hotkeyManager) {
		m_hotkeyManager->SerializeHotkeyBindings(hotkeysState, true);
	}
	SerializeUserInterfaceState(userInterfaceState);

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	m_nativeObsControlsManager->SerializePreviewTitleBar(
		outputPreviewTitleBarState);
	m_nativeObsControlsManager->SerializePreviewFrame(
		outputPreviewFrameState);
#endif

	rootDictionary->SetValue("dockingBrowserWidgets", dockingWidgetsState);
	rootDictionary->SetValue("notificationBar", notificationBarState);
	rootDictionary->SetValue("workers", workersState);
	rootDictionary->SetValue("hotkeyBindings", hotkeysState);
	rootDictionary->SetValue("userInterfaceState", userInterfaceState);

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	rootDictionary->SetValue("outputPreviewTitleBarState",
				 outputPreviewTitleBarState);
	rootDictionary->SetValue("outputPreviewFrameState",
				 outputPreviewFrameState);
#endif

	root->SetDictionary(rootDictionary);

	CefString json = CefWriteJSON(root, JSON_WRITER_DEFAULT);

	std::string base64EncodedJSON = base64_encode(json.ToString());

	SEConfig::GetInstance()->SetStartupState(base64EncodedJSON);

	if (sendEventToGuest) {
		AdviseHostUserInterfaceStateChanged();
	}
}

void SEStatePersistence::RestoreState()
{
	std::string base64EncodedJSON =
		SEConfig::GetInstance()->GetStartupState();

	if (!base64EncodedJSON.size()) {
		return;
	}

	if (IsTraceLogLevel()) {
		blog(LOG_INFO,
		     "obs-streamelements-core: state: restoring state from base64: %s",
		     base64EncodedJSON.c_str());
	}

	CefString json = base64_decode(base64EncodedJSON);

	if (!json.size()) {
		return;
	}

	if (IsTraceLogLevel()) {
		blog(LOG_INFO,
		     "obs-streamelements-core: state: restoring state from json: %s",
		     json.ToString().c_str());
	}

	CefRefPtr<CefValue> root =
		CefParseJSON(json, JSON_PARSER_ALLOW_TRAILING_COMMAS);

	// CefParseJSON returns null for anything it cannot parse, and CefRefPtr
	// is std::shared_ptr here, so calling through it is a hard dereference of
	// null. The check below used to be the only one, and it tests the wrong
	// value one line too late.
	//
	// The state we are parsing is persisted, so this is not a one-off crash:
	// every subsequent launch reads the same bad blob back and dies in the
	// same place, before anything else runs (CORE-1601). Returning here
	// leaves the user with default state, which is recoverable; crashing
	// leaves them with an application that cannot start.
	if (!root.get()) {
		blog(LOG_WARNING,
		     "obs-streamelements-core: state: discarding unparsable startup state");

		return;
	}

	CefRefPtr<CefDictionaryValue> rootDictionary = root->GetDictionary();

	if (!rootDictionary.get()) {
		return;
	}

	auto dockingWidgetsState =
		rootDictionary->GetValue("dockingBrowserWidgets");
	auto notificationBarState = rootDictionary->GetValue("notificationBar");
	auto workersState = rootDictionary->GetValue("workers");
	auto hotkeysState = rootDictionary->GetValue("hotkeyBindings");
	auto userInterfaceState =
		rootDictionary->GetValue("userInterfaceState");

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	auto outputPreviewTitleBarState =
		rootDictionary->GetValue("outputPreviewTitleBarState");
	auto outputPreviewFrameState =
		rootDictionary->GetValue("outputPreviewFrameState");
#endif

	if (workersState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring workers: %s",
			     CefWriteJSON(workersState, JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		m_workerManager->Deserialize(workersState);
	}

	if (dockingWidgetsState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring docking widgets: %s",
			     CefWriteJSON(dockingWidgetsState,
					  JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		m_widgetManager->DeserializeDockingWidgets(dockingWidgetsState);
	}

	if (notificationBarState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring notification bar: %s",
			     CefWriteJSON(notificationBarState,
					  JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		m_widgetManager->DeserializeNotificationBar(
			notificationBarState);
	}

	if (hotkeysState.get()) {
		if (IsTraceLogLevel()) {
			blog(LOG_INFO,
			     "obs-streamelements-core: state: restoring hotkey bindings: %s",
			     CefWriteJSON(hotkeysState, JSON_WRITER_DEFAULT)
				     .ToString()
				     .c_str());
		}

		m_hotkeyManager->DeserializeHotkeyBindings(hotkeysState);
	}

	if (userInterfaceState.get()) {
		DeserializeUserInterfaceState(userInterfaceState);
	}

#if SE_ENABLE_CENTRAL_WIDGET_DECORATIONS
	if (outputPreviewTitleBarState.get()) {
		m_nativeObsControlsManager->DeserializePreviewTitleBar(
			outputPreviewTitleBarState);
	}

	if (outputPreviewFrameState.get()) {
		m_nativeObsControlsManager->DeserializePreviewFrame(
			outputPreviewFrameState);
	}
#endif
}

void SEStatePersistence::SerializeUserInterfaceState(CefRefPtr<CefValue> &output)
{
	CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

	QByteArray geometry = m_mainWindow->saveGeometry();
	QByteArray windowState = m_mainWindow->saveState();

	d->SetString("geometry",
		     base64_encode(geometry.begin(), geometry.size()));
	d->SetString("windowState",
		     base64_encode(windowState.begin(), windowState.size()));

	output->SetDictionary(d);
}

bool SEStatePersistence::DeserializeUserInterfaceState(CefRefPtr<CefValue> input)
{
	if (input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	bool result = false;

	if (d->HasKey("geometry") && d->GetType("geometry") == VTYPE_STRING) {
		auto geometry = d->GetValue("geometry");

		if (geometry.get() && geometry->GetType() == VTYPE_STRING) {
			if (IsTraceLogLevel()) {
				blog(LOG_INFO,
				     "obs-streamelements-core: state: restoring geometry: %s",
				     geometry->GetString().ToString().c_str());
			}

			if (m_mainWindow->restoreGeometry(
				    QByteArray::fromStdString(base64_decode(
					    geometry->GetString().ToString())))) {
				// https://bugreports.qt.io/browse/QTBUG-46620
				if (m_mainWindow->isMaximized()) {
					m_mainWindow->setGeometry(
#if QT_VERSION_MAJOR >= 6
						QApplication::primaryScreen()
#else
						QApplication::desktop()
#endif
							->availableGeometry(
#if QT_VERSION_MAJOR < 6
								m_mainWindow
#endif
								));
				}

				result = true;
			} else {
				blog(LOG_ERROR,
				     "obs-streamelements-core: state: failed restoring geometry: %s",
				     geometry->GetString().ToString().c_str());
			}
		}
	}

	if (d->HasKey("windowState") &&
	    d->GetType("windowState") == VTYPE_STRING) {
		auto windowState = d->GetValue("windowState");

		if (windowState.get() &&
		    windowState->GetType() == VTYPE_STRING) {
			if (IsTraceLogLevel()) {
				blog(LOG_INFO,
				     "obs-streamelements-core: state: restoring windowState: %s",
				     windowState->GetString()
					     .ToString()
					     .c_str());
			}

			if (m_mainWindow->restoreState(QByteArray::fromStdString(
				    base64_decode(windowState->GetString()
							  .ToString())))) {
				result = true;
			} else {
				blog(LOG_ERROR,
				     "obs-streamelements-core: state: failed restoring windowState: %s",
				     windowState->GetString()
					     .ToString()
					     .c_str());
			}
		}
	}

	auto screenGeometry = m_mainWindow->screen()->availableSize();

	if (m_mainWindow->frameGeometry().height() > screenGeometry.height()) {
		auto minHeight = m_mainWindow->minimumHeight();

		// https://doc.qt.io/qt-6/application-windows.html#window-geometry
		auto titleBarHeight = m_mainWindow->frameGeometry().height() -
				      m_mainWindow->height();

		m_mainWindow->setFixedHeight(screenGeometry.height() -
					     titleBarHeight);
		m_mainWindow->move(m_mainWindow->x(), 0);

		QtPostTask([minHeight, this]() {
			m_mainWindow->setMinimumHeight(minHeight);

			m_mainWindow->setMaximumHeight(
				16777215); // default according to Qt docs
		});
	}

	return result;
}
