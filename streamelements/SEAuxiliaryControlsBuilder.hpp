#pragma once

//
// Menus, actions, icons and control widgets built from a CefValue
// description, and the host API call an action invokes.
//
// These were free functions in SEUtils that fetched what they needed from
// SEGlobalStateManager: the docks submenu wants the widget, menu and
// analytics managers, and an invoked action wants the entire host API
// surface. The second of those is why this is one service holding the
// services bundle weakly rather than a handful of injected managers -- the
// same exception SEApiMessageHandler and SESessionManager already make, for
// the same reason, and the reason the InvokeHandler below could never be
// handed anything and became a singleton instead.
//
// Everything a page can describe here ends up on a Qt object that outlives
// the call that built it -- a QAction on a menu the menu bar owns, a push
// button on a toolbar -- so every handler lambda holds this service through
// weak_from_this() and locks it when it fires.
//

#include "cef-headers.hpp"

#include "SEApiMessageHandler.hpp"
#include "SEApiServices.hpp"

#include <functional>
#include <memory>
#include <mutex>

class QMenu;
class QWidget;
class QPixmap;

class SEAuxiliaryControlsBuilder
	: public std::enable_shared_from_this<SEAuxiliaryControlsBuilder> {
public:
	SEAuxiliaryControlsBuilder(std::weak_ptr<SEApiServices> apiServices);
	~SEAuxiliaryControlsBuilder();

public:
	//
	// Invokes the host API method the description names, or runs one of
	// the two ":default*" callbacks the caller supplies.
	//
	bool
	DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				   std::function<void()> defaultAction,
				   std::function<void()> defaultContextMenu);

	//
	// One checkable entry per dock browser widget, toggling it. The
	// ":dockingWidgets" items source in DeserializeMenu below.
	//
	bool DeserializeDocksMenu(QMenu &menu);

	bool DeserializeMenu(
		CefRefPtr<CefValue> input, QMenu &menu,
		std::function<void()> defaultAction = []() {},
		std::function<void()> defaultContextMenu = []() {});

	//
	// Builds the menu and runs it modally at the mouse pointer. The
	// handler body used to construct a stack QMenu and call
	// exec(QCursor::pos()) itself, which is the only reason the host API
	// surface named a Qt type at all.
	//
	bool ShowMenuAtMousePointerPosition(CefRefPtr<CefValue> input);

	QWidget *DeserializeAuxiliaryControlWidget(
		CefRefPtr<CefValue> input,
		std::function<void()> defaultAction = []() {},
		std::function<void()> defaultContextMenu = []() {});

	//
	// Needs nothing from this service, and is here because it is the
	// fourth user of the three remote-icon Qt subclasses in the .cpp --
	// keeping it with them keeps them file-local.
	//
	QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
					     QPixmap *defaultPixmap = nullptr);

private:
	//
	// Weak, and locked per call: this service is in the bundle, so
	// holding it strongly would be a two-node ring.
	//
	std::weak_ptr<SEApiServices> m_services;

	//
	// The handler auxiliary actions are dispatched through, built on
	// first use -- the timing it has always had. It used to be a
	// file-static shared_ptr that was never released, which is why the
	// bundle's refcount could not reach zero even with nothing else
	// alive; now it dies with this service.
	//
	std::shared_ptr<SEApiMessageHandler::InvokeHandler> m_invokeHandler;
	std::mutex m_invokeHandlerMutex;

	std::shared_ptr<SEApiMessageHandler::InvokeHandler>
	EnsureInvokeHandler();
};
