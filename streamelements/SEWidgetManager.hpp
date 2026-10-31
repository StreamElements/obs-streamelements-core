#pragma once

#include "SEApiServices.hpp"

#include <QWidget>
#include <QPointer>
#include <QDockWidget>
#include <QMainWindow>
#include <stack>
#include <map>
#include <mutex>

#include "../cef-headers.hpp"

#include "SEObsAppMonitor.hpp"
#include "SEUtils.hpp"

class SEStatePersistence;

class SEWidgetManager :
	public SEObsAppMonitor
{
public:
	class DockWidgetInfo
	{
	public:
		std::string m_id;
		std::string m_title;
		bool m_visible = true;
		std::string m_dockingArea;

	public:
		DockWidgetInfo()
		{ }

		DockWidgetInfo(const DockWidgetInfo& other)
		{
			m_id = other.m_id;
			m_title = other.m_title;
			m_visible = other.m_visible;
			m_dockingArea = other.m_dockingArea;

			m_widget = other.m_widget;
		}

	private:
		QWidget* m_widget = nullptr;

		friend class SEWidgetManager;

	public:
		QWidget* GetWidget() { return m_widget; }
	};

public:
	SEWidgetManager(QMainWindow *parent,
			std::shared_ptr<SEApiServices> apiServices);

protected:
	//
	// Handed to every browser widget and API handler created by a widget
	// manager.
	//
	// Weak: this manager's destructor removes and deletes the docks, each
	// dock destroys its browser widget, and every widget's API handler
	// holds a strong reference to the bundle. A strong reference here is
	// therefore a ring -- and the one that stalled shutdown, because the
	// loop runs through a destructor rather than through refcounts, so
	// nothing unwinds it.
	//
	std::weak_ptr<SEApiServices> m_apiServices;

	//
	// Locks it. Every caller is about to build a widget or an API
	// handler, and a dead bundle means not building one.
	//
	std::shared_ptr<SEApiServices> ApiServices() const
	{
		return m_apiServices.lock();
	}

	//
	// Read by the deferred tasks that save window state.
	//
	// Weak, and set after construction rather than injected, for the
	// same destruction-order reason that makes SEStatePersistence borrow
	// this manager: statePersistence is destroyed first, so a strong
	// handle here would invert that and leave its four borrowed pointers
	// dangling. Set-after-construction is also the only option -- the
	// persistence object is built from this manager.
	//
	std::weak_ptr<SEStatePersistence> m_statePersistence;

public:
	void SetStatePersistence(
		std::shared_ptr<SEStatePersistence> statePersistence)
	{
		m_statePersistence = statePersistence;
	}

public:
	~SEWidgetManager();

	/* central widget */

	void PushCentralWidget(QWidget* widget);
	bool DestroyCurrentCentralWidget();
	bool HasCentralWidget();

	/* dockable widgets */

	bool AddDockWidget(
		const char* const id,
		const char* const title,
		QWidget* const widget,
		const Qt::DockWidgetArea area,
		const Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
		const QDockWidget::DockWidgetFeatures features =
			QDockWidget::DockWidgetClosable |
			QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable);

	virtual bool ShowWidgetById(const char *const id);
	virtual bool HideWidgetById(const char *const id);

	virtual bool ToggleWidgetFloatingStateById(const char *const id);

	virtual bool SetWidgetDimensionsById(const char* const id, const int width, const int height);
	virtual bool SetWidgetPositionById(const char* const id, const int left, const int top);
	virtual bool SetWidgetTitleById(const char *const id,
					const char *const title);

	virtual bool RemoveDockWidget(const char* const id);

	void GetDockWidgetIdentifiers(std::vector<std::string>& result);

	QDockWidget* GetDockWidget(const char* const id);

	//
	// Whether there is one, without handing out the widget. The host API
	// asks only this -- addDockingWidget took the QDockWidget* purely as
	// a success check and never used it -- and asking it this way keeps
	// Qt out of the one signature a handler body reaches.
	//
	bool HasDockWidget(const char *const id);

	DockWidgetInfo* GetDockWidgetInfo(const char* const id);

	virtual void SerializeDockingWidgets(CefRefPtr<CefValue>& output) = 0;
	virtual void DeserializeDockingWidgets(CefRefPtr<CefValue>& input) = 0;

	void SerializeDockingWidgets(std::string& output);
	void DeserializeDockingWidgets(std::string& input);

protected:
	QMainWindow* mainWindow() { return m_parent; }

private:
	QPointer<QMainWindow> m_parent;
	QPointer<QWidget> m_nativeCentralWidget = nullptr;
	//QWidget* m_currentCentralWidget = nullptr;

	// QPointer, not a raw pointer: the docks are children of the OBS main
	// window (addDockWidget), so Qt owns them and destroys them with that
	// window. Nothing tells this map when that happens, and on the OBSInit
	// re-entrancy path it happens before ~SEWidgetManager runs.
	// Raw pointers went stale and were deleted a second time (CORE-786).
	std::map<std::string, QPointer<QDockWidget>> m_dockWidgets;
	std::map<std::string, Qt::DockWidgetArea> m_dockWidgetAreas;

	std::map<std::string, QSize> m_dockWidgetSavedMinSize;

protected:
	std::recursive_mutex m_mutex;

protected:
	void SaveDockWidgetsGeometry();
	void RestoreDockWidgetsGeometry();


protected:
	virtual void OnObsExit() override;
};
