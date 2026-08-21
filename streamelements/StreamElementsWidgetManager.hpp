#pragma once

#include <QWidget>
#include <QPointer>
#include <QDockWidget>
#include <QMainWindow>
#include <stack>
#include <map>
#include <mutex>

#include "../cef-headers.hpp"

#include "StreamElementsObsAppMonitor.hpp"
#include "StreamElementsUtils.hpp"

class StreamElementsWidgetManager :
	public StreamElementsObsAppMonitor
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

		friend class StreamElementsWidgetManager;

	public:
		QWidget* GetWidget() { return m_widget; }
	};

public:
	StreamElementsWidgetManager(QMainWindow* parent);
	~StreamElementsWidgetManager();

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
	// re-entrancy path it happens before ~StreamElementsWidgetManager runs.
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
