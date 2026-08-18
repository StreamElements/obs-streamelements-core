#pragma once

#include "cef-headers.hpp"

#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsBrowserWidget.hpp"

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>
#include <QPointer>

#include <string>
#include <vector>

class StreamElementsBrowserWidget;

class StreamElementsMenuManager :
	public QObject
{
	Q_OBJECT;

private:
	enum aux_menu_item_type_t { Command, Separator, Container };

public:
	StreamElementsMenuManager(QMainWindow* parent);
	virtual ~StreamElementsMenuManager();

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
