#pragma once

#include "cef-headers.hpp"

#include <QPointer>

#include <memory>
#include <vector>

class QMainWindow;
class SEBrowserWidget;
struct SEApiServices;

//
// Pop-up browser windows opened by the host API: a top-level QMainWindow
// with a SEBrowserWidget as its central widget.
//
// It was one method on SEGlobalStateManager, and the windows it opened were
// not tracked by anything, so nothing ever closed one. Each carried a live
// CEF browser into Qt's teardown -- see the comment in
// ~SEBrowserWidgetManager for why that is the dangerous order --
// which is what the destructor here fixes.
//
class SEPopupWindowManager {
public:
	//
	// Takes the bundle, keeps it weakly. Everything it creates -- the API
	// message handler and the browser widget -- holds a strong reference
	// to the bundle, and the destructor below destroys them, so a strong
	// reference here would be a ring the refcount can never unwind:
	// bundle -> this -> (destructor) -> widget -> bundle.
	//
	SEPopupWindowManager(std::shared_ptr<SEApiServices> apiServices);
	~SEPopupWindowManager();

	bool DeserializePopupWindow(CefRefPtr<CefValue> input);

private:
	std::weak_ptr<SEApiServices> m_apiServices;

	//
	// QPointer, because a window the user closed with its own title bar
	// button is only hidden, but one destroyed any other way has to stop
	// being visited here.
	//
	struct PopupWindow {
		QPointer<QMainWindow> window;
		QPointer<SEBrowserWidget> browser;
	};

	std::vector<PopupWindow> m_windows;
};
