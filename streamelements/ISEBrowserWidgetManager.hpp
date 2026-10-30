#pragma once

#include "cef-headers.hpp"
#include "SEDockBrowserWidgetInfo.hpp"

#include <string>
#include <vector>

//
// The last of part 2's interfaces, and the one that takes
// SEApiMessageHandler.cpp's include closure to 0 obs and 0 Qt:
// SEBrowserWidgetManager.hpp was its last dirty direct include, and that
// header's own closure is the identical set of 7 obs and 22 Qt headers.
//
// 25 methods, the union over the handler bodies, SESessionManager and
// SEAuxiliaryControlsBuilder. Four of them needed a decision rather than a
// transcription:
//
//  - GetDockWidget returns a QDockWidget *. A forward declaration is
//    enough for a pointer, so this header names the type without
//    including any Qt, and the closure stays clean. No handler body calls
//    it -- SEAuxiliaryControlsBuilder's docks menu is the only caller, and
//    it needs the live widget for windowTitle() and setVisible().
//
//  - GetDockBrowserWidgetInfo could not be transcribed at all: its return
//    type is nested in the concrete manager. See SEDockBrowserWidgetInfo,
//    which is why that file exists.
//
//  - EnterCriticalSection and LeaveCriticalSection are a mutex on an
//    API-facing contract, which is worth being uncomfortable about. They
//    are here as-is, by decision, because the alternative is a behaviour
//    change: the docks menu holds this lock across an enumerate-then-read
//    sequence, and replacing it with methods that lock internally narrows
//    the window from the whole menu build to one call each. That is
//    probably an improvement and it is not a type change, so it does not
//    belong in this commit.
//
//  - AddDockBrowserWidget and DeserializeNotificationBar are overloaded,
//    and only the CefValue overload of each is on the container's surface.
//    The other AddDockBrowserWidget takes Qt::DockWidgetArea,
//    Qt::DockWidgetAreas and QDockWidget::DockWidgetFeatures with Qt
//    default arguments; it is internal, and measuring the union by method
//    name made it look like a handler caller.
//
// What stays on the implementation, because only the root calls it: the
// constructor's QMainWindow, SetStatePersistence, ShowNotificationBar
// (reached only from DeserializeNotificationBar's own body), and the
// Serialize/Deserialize pairs the state persistence drives.
//
class QDockWidget;

class ISEBrowserWidgetManager {
public:
	virtual ~ISEBrowserWidgetManager() {}

	/********************/
	/* Central widget   */
	/********************/

public:
	virtual void PushCentralBrowserWidget(
		const char *const url,
		const char *const executeJavaScriptCodeOnLoad) = 0;

	virtual bool DestroyCurrentCentralBrowserWidget() = 0;

	virtual bool HasCentralBrowserWidget() = 0;

	/********************/
	/* Dockable widgets */
	/********************/

public:
	virtual std::string
	AddDockBrowserWidget(CefRefPtr<CefValue> input,
			     std::string requestId = "") = 0;

	virtual bool ToggleWidgetFloatingStateById(const char *const id) = 0;

	virtual bool SetWidgetDimensionsById(const char *const id,
					     const int width,
					     const int height) = 0;
	virtual bool SetWidgetPositionById(const char *const id, const int left,
					   const int top) = 0;
	virtual bool SetWidgetUrlById(const char *const id,
				      const char *const url) = 0;
	virtual bool SetWidgetTitleById(const char *const id,
					const char *const title) = 0;

	virtual bool ShowDockWidgetById(const char *const id) = 0;
	virtual bool HideDockWidgetById(const char *const id) = 0;

	virtual bool RemoveDockWidget(const char *const id) = 0;
	virtual void RemoveAllDockWidgets() = 0;

	virtual void
	GetDockBrowserWidgetIdentifiers(std::vector<std::string> &result) = 0;

	//
	// False when there is no dock with that id, in which case output is
	// untouched. Replaces the owned-raw-pointer getter of the same name,
	// which is still there for the manager's own use.
	//
	virtual bool
	GetDockBrowserWidgetInfo(const char *const id,
				 SEDockBrowserWidgetInfo &output) = 0;

	//
	// The live widget, for the one consumer that needs Qt from it. Null
	// when there is no such dock.
	//
	virtual QDockWidget *GetDockWidget(const char *const id) = 0;

	virtual bool HasDockWidget(const char *const id) = 0;

	virtual void SerializeDockingWidgets(CefRefPtr<CefValue> &output) = 0;

	virtual bool GroupDockingWidgetPairByIds(const char *firstId,
						 const char *secondId) = 0;

	virtual bool InsertDockingWidgetRelativeToId(const char *firstId,
						     const char *secondId,
						     const bool isBefore) = 0;

	/********************/
	/* Notification bar */
	/********************/

public:
	virtual void DeserializeNotificationBar(CefRefPtr<CefValue> &input) = 0;

	virtual void HideNotificationBar() = 0;

	virtual bool HasNotificationBar() = 0;

	/********************/
	/* Critical section */
	/********************/

public:
	virtual void EnterCriticalSection() = 0;
	virtual void LeaveCriticalSection() = 0;
};
