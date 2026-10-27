#pragma once

#include "cef-headers.hpp"

#include "SEApiMessageHandler.hpp"
#include "SEBrowserWidget.hpp"

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QPointer>

#include <memory>
#include <string>
#include <vector>

class SEBrowserWidget;
class SESessionManager;
class SEPerformanceHistoryTracker;
class SEAnalyticsEventsManager;
class SEAuxiliaryControlsBuilder;
class SEConfig;
class SEHostUpdateManager;

class SEMenuManager :
	public QObject
{
	Q_OBJECT;

private:
	enum aux_menu_item_type_t { Command, Separator, Container };

public:
	//
	// The session manager is held strongly, and that is not a ring: it
	// holds the services bundle weakly, and nothing it owns owns this.
	// It is injected because two menu items are the non-API way into a
	// session reset, and reaching the singleton for them was the last
	// reason this class needed it.
	//
	SEMenuManager(QMainWindow *parent,
		      std::shared_ptr<SESessionManager> sessionManager,
		      std::shared_ptr<SEPerformanceHistoryTracker>
			      performanceHistoryTracker,
		      std::shared_ptr<SEAnalyticsEventsManager>
			      analyticsEventsManager,
		      std::shared_ptr<SEAuxiliaryControlsBuilder>
			      auxiliaryControlsBuilder,
		      std::shared_ptr<SEConfig> config,
		      std::shared_ptr<SEHostUpdateManager> hostUpdateManager);
	virtual ~SEMenuManager();

public:
	void Update();

	bool DeserializeAuxiliaryMenuItems(CefRefPtr<CefValue> input);
	void SerializeAuxiliaryMenuItems(CefRefPtr<CefValue>& output);

	void Reset();

	void SetShowBuiltInMenuItems(bool show);
	bool GetShowBuiltInMenuItems();

protected:
	QMainWindow* mainWindow() { return m_mainWindow; }

	void SaveConfig();
	void LoadConfig();

private:
	void UpdateInternal();

private:
	QMainWindow* m_mainWindow;
	std::shared_ptr<SESessionManager> m_sessionManager;

	//
	// For the Report Issue dialog, which this menu is the only trigger
	// for. It was a method on SEGlobalStateManager, which is why opening
	// it meant reaching for the singleton.
	//
	std::shared_ptr<SEPerformanceHistoryTracker>
		m_performanceHistoryTracker;
	std::shared_ptr<SEAnalyticsEventsManager> m_analyticsEventsManager;

	//
	// Builds this menu's aux items and its docks submenu from the
	// description a page sends. Was reached through the singleton from
	// free functions in SEUtils.
	//
	std::shared_ptr<SEAuxiliaryControlsBuilder> m_auxiliaryControls;

	//
	// The aux menu items and the built-in-items flag are read from and
	// written to the plug-in's ini, and the onboarding flag decides
	// whether the menu is built at all.
	//
	std::shared_ptr<SEConfig> m_config;
	std::shared_ptr<SEHostUpdateManager> m_hostUpdateManager;

private:
	void ReportIssue();

	//
	// QPointer, not QMenu*, because we do not own this.
	//
	// The menu is created here but handed to the main window's menu bar via
	// addMenu(), which reparents it -- so Qt destroys it when the main
	// window goes down, on its own schedule rather than ours. A raw pointer
	// is left dangling for the whole of shutdown, and UpdateInternal's
	// `if (!m_menu) return;` guard sails straight past a stale one into
	// m_menu->clear() on freed memory.
	//
	// Observed: a crash in UpdateInternal at the moment OBS restarted to
	// apply an update. QPointer is zeroed by Qt when the object dies, which
	// is what makes that guard mean what it says.
	//
	QPointer<QMenu> m_menu;

	CefRefPtr<CefValue> m_auxMenuItems = CefValue::Create();
	bool m_showBuiltInMenuItems = true;
};
