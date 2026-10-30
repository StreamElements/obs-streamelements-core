#pragma once

#include "SEWidgetManager.hpp"
#include "SEBrowserWidget.hpp"
#include "ISEBrowserWidgetManager.hpp"

#include <map>
#include <string>

#include <QPointer>

class SEBrowserWidgetManager : protected SEWidgetManager,
			       public ISEBrowserWidgetManager {
public:
	class DockBrowserWidgetInfo :
		public DockWidgetInfo
	{
	public:
		DockBrowserWidgetInfo()
		{
		}

		DockBrowserWidgetInfo(const DockWidgetInfo& other): DockWidgetInfo(other)
		{
		}

		DockBrowserWidgetInfo(const DockBrowserWidgetInfo& other) : DockWidgetInfo(other)
		{
			m_url = other.m_url;
			m_executeJavaScriptOnLoad = other.m_executeJavaScriptOnLoad;
			m_reloadPolicy = other.m_reloadPolicy;
		}

	public:
		std::string m_url;
		std::string m_executeJavaScriptOnLoad;
		std::string m_reloadPolicy = "reload";
	};

public:
	SEBrowserWidgetManager(QMainWindow *parent,
			       std::shared_ptr<SEApiServices> apiServices);
	virtual ~SEBrowserWidgetManager();

	//
	// The inheritance here is protected, so the base class's public
	// members are not public through this one. SEGlobalStateManager hands
	// the persistence object over after construction -- it is built from
	// this manager, so it cannot be a constructor argument -- and this is
	// what lets it.
	//
	using SEWidgetManager::SetStatePersistence;

	/********************/
	/* Central widget   */
	/********************/

	void PushCentralBrowserWidget(
		const char *const url,
		const char *const executeJavaScriptCodeOnLoad) override;

	bool DestroyCurrentCentralBrowserWidget() override;

	bool HasCentralBrowserWidget() override { return HasCentralWidget(); }

	/********************/
	/* Dockable widgets */
	/********************/

	std::string AddDockBrowserWidget(CefRefPtr<CefValue> input,
					 std::string requestId = "") override;

	bool AddDockBrowserWidget(
		const char* const id,
		const char* const title,
		const char* const url,
		const char* const executeJavaScriptCodeOnLoad,
		const char* const reloadPolicy,
		const Qt::DockWidgetArea area,
		const Qt::DockWidgetAreas allowedAreas = Qt::AllDockWidgetAreas,
		const QDockWidget::DockWidgetFeatures features =
			QDockWidget::DockWidgetClosable |
			QDockWidget::DockWidgetMovable |
			QDockWidget::DockWidgetFloatable);

	virtual bool ToggleWidgetFloatingStateById(const char* const id) override;

	virtual bool SetWidgetDimensionsById(const char* const id, const int width, const int height) override;
	virtual bool SetWidgetPositionById(const char* const id, const int left, const int top) override;
	virtual bool SetWidgetUrlById(const char *const id,
				      const char *const url) override;

	virtual bool SetWidgetTitleById(const char* const id,
		const char* const title) override
	{
		return SEWidgetManager::SetWidgetTitleById(id, title);
	}

	virtual bool ShowDockWidgetById(const char *const id) override
	{
		return SEWidgetManager::ShowWidgetById(id);
	}

	virtual bool HideDockWidgetById(const char *const id) override
	{
		return SEWidgetManager::HideWidgetById(id);
	}

	virtual bool RemoveDockWidget(const char* const id) override;

	void RemoveAllDockWidgets() override;

	void GetDockBrowserWidgetIdentifiers(
		std::vector<std::string> &result) override;

	DockBrowserWidgetInfo* GetDockBrowserWidgetInfo(const char* const id);

	//
	// ISEBrowserWidgetManager's view of the getter above: a value, so
	// that a contract can name it -- DockBrowserWidgetInfo is nested
	// in this class and a nested class cannot be forward-declared --
	// and so that a caller cannot forget to free it. The pointer form
	// stays for this manager's own use.
	//
	bool GetDockBrowserWidgetInfo(const char *const id,
				      SEDockBrowserWidgetInfo &output) override;

	QDockWidget *GetDockWidget(const char *const id) override
	{
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		return SEWidgetManager::GetDockWidget(id);
	}

	bool HasDockWidget(const char *const id) override
	{
		std::lock_guard<std::recursive_mutex> guard(m_mutex);

		return SEWidgetManager::HasDockWidget(id);
	}

	virtual void SerializeDockingWidgets(CefRefPtr<CefValue>& output) override;
	virtual void DeserializeDockingWidgets(CefRefPtr<CefValue>& input) override;

	void SerializeDockingWidgets(std::string& output) { SEWidgetManager::SerializeDockingWidgets(output); }
	void DeserializeDockingWidgets(std::string& input) { SEWidgetManager::DeserializeDockingWidgets(input); }

	bool GroupDockingWidgetPairByIds(const char *firstId,
					 const char *secondId) override;

	bool InsertDockingWidgetRelativeToId(const char *firstId,
					     const char *secondId,
					     const bool isBefore) override;

	/********************/
	/* Notification bar */
	/********************/

	void ShowNotificationBar(
		const char* const url,
		const int height,
		const char* const executeJavaScriptCodeOnLoad);

	void HideNotificationBar() override;

	bool HasNotificationBar() override;

	virtual void SerializeNotificationBar(CefRefPtr<CefValue>& output);
	virtual void
	DeserializeNotificationBar(CefRefPtr<CefValue> &input) override;

	void SerializeNotificationBar(std::string& output);
	void DeserializeNotificationBar(std::string& input);

	/********************/
	/* Critical section */
	/********************/

	void EnterCriticalSection() override { m_mutex.lock(); }

	void LeaveCriticalSection() override { m_mutex.unlock(); }

private:
	// QPointer for the same reason as m_dockWidgets (CORE-786): each browser
	// widget is a child of its dock, so it dies with the dock, and this map
	// is not notified.
	std::map<std::string, QPointer<SEBrowserWidget>>
		m_browserWidgets;

	QPointer<QToolBar> m_notificationBarToolBar = nullptr;

	QPointer<SEBrowserWidget> m_notificationBarBrowserWidget = nullptr;
};
