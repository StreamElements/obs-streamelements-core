#include "SEHostEventUtils.hpp"

#include "SEStringUtils.hpp"

//
// After the SEUtils headers on purpose: SEGlobalStateManager.hpp drags
// windows.h in, and ahead of winsock2.h that redefines every socket struct.
//
#include "../SEGlobalStateManager.hpp"
#include "../SEWebsocketApiServer.hpp"

#include <util/threading.h>

#include <QApplication>
#include <QTimer>

#include <regex>

void AdviseHostUserInterfaceStateChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {
		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			os_atomic_set_bool(&isBusy, false);

			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostUserInterfaceStateChanged",
					      "null");
		});
	}
}
void AdviseHostHotkeyBindingsChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {

		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostHotkeyBindingsChanged",
					      "null");

			os_atomic_set_bool(&isBusy, false);
		});
	}
}
void DispatchClientMessage(std::string target, CefRefPtr<CefProcessMessage> msg)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer =
		SEGlobalStateManager::GetInstance()->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetClientMessage("system", target, msg);
}
void DispatchJSEventGlobal(std::string event, std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer =
		SEGlobalStateManager::GetInstance()->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", event, eventArgsJson);
}
void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer =
		SEGlobalStateManager::GetInstance()->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetJSEvent("system", target, event,
					 eventArgsJson);
}
