#include "SEHostEventDispatcher.hpp"

#include "SEWebsocketApiServer.hpp"

#include <util/threading.h>

#include <QApplication>
#include <QTimer>

#include <chrono>

SEHostEventDispatcher::SEHostEventDispatcher(
	std::shared_ptr<SEWebsocketApiServer> apiServer)
	: m_apiServer(apiServer)
{
}

SEHostEventDispatcher::~SEHostEventDispatcher() {}

void SEHostEventDispatcher::DispatchJSEventGlobal(std::string event,
						  std::string eventArgsJson)
{
	if (!m_apiServer)
		return;

	m_apiServer->DispatchJSEvent("system", event, eventArgsJson);
}

void SEHostEventDispatcher::DispatchJSEventContainer(
	std::string target, std::string event, std::string eventArgsJson)
{
	if (!m_apiServer)
		return;

	m_apiServer->DispatchTargetJSEvent("system", target, event,
					   eventArgsJson);
}

void SEHostEventDispatcher::DispatchClientMessage(
	std::string target, CefRefPtr<CefProcessMessage> msg)
{
	if (!m_apiServer)
		return;

	m_apiServer->DispatchTargetClientMessage("system", target, msg);
}

void SEHostEventDispatcher::AdviseHostUserInterfaceStateChanged()
{
	//
	// Function-local, so the debounce is shared by every dispatcher --
	// which is what it was as a file static, and what it should be: the
	// thing being debounced is the page's view of one user interface.
	//
	static bool isBusy = false;

	if (os_atomic_set_bool(&isBusy, true))
		return;

	std::weak_ptr<SEHostEventDispatcher> self = weak_from_this();

	QTimer::singleShot(std::chrono::milliseconds(250), qApp, [self]() {
		os_atomic_set_bool(&isBusy, false);

		auto dispatcher = self.lock();

		if (!dispatcher)
			return;

		// Advise guest code of user interface state changes
		dispatcher->DispatchJSEventGlobal(
			"hostUserInterfaceStateChanged", "null");
	});
}

void SEHostEventDispatcher::AdviseHostHotkeyBindingsChanged()
{
	static bool isBusy = false;

	if (os_atomic_set_bool(&isBusy, true))
		return;

	std::weak_ptr<SEHostEventDispatcher> self = weak_from_this();

	QTimer::singleShot(std::chrono::milliseconds(250), qApp, [self]() {
		auto dispatcher = self.lock();

		if (dispatcher) {
			// Advise guest code of hotkey binding changes
			dispatcher->DispatchJSEventGlobal(
				"hostHotkeyBindingsChanged", "null");
		}

		os_atomic_set_bool(&isBusy, false);
	});
}
