#pragma once

#include "cef-headers.hpp"

#include "StreamElementsApiMessageHandler.hpp"
#include "StreamElementsBrowserWidget.hpp"

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>

#include <string>
#include <vector>

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

	void SetFocusedBrowserWidget(StreamElementsBrowserWidget *widget);

protected:
	QMainWindow* mainWindow() { return m_mainWindow; }

	void SaveConfig();
	void LoadConfig();

private:
	void UpdateInternal();
	void UpdateOBSEditMenuInternal();

	void HandleFocusedBrowserWidgetDOMNodeEditableChanged(bool isEditable);
	void HandleClipboardDataChanged();

	void HandleCefCopy();
	void HandleCefCut();
	void HandleCefPaste();
	void HandleCefSelectAll();

	void AddOBSEditMenuActions();
	void RemoveOBSEditMenuActions();

private:
	QMainWindow* m_mainWindow;
	QMenu *m_menu;

	//
	// OBS-native Edit menu.
	//
	QMenu *m_editMenu;

	//
	// Our actions to be added under OBS-native Edit
	// menu when appropriate (see m_focusedBrowserWidget
	// below for further details).
	//
	std::vector<QAction *> m_cefOBSEditMenuActions;
	QAction *m_cefOBSEditMenuActionCopy = nullptr;
	QAction *m_cefOBSEditMenuActionCut = nullptr;
	QAction *m_cefOBSEditMenuActionPaste = nullptr;
	QAction *m_cefOBSEditMenuActionSelectAll = nullptr;

	//
	// OBS-native Edit->Copy menu item.
	//
	QAction *m_nativeOBSEditMenuCopySourceAction = nullptr;

	//
	// Indicates which browser widget is currently in focus.
	//
	// When a browser widget is in focus and it's internally
	// focused DOM node is an editable element, we'll show
	// Cut/Copy/Paste/Select All items under the OBS native
	// "Edit" menu. We'll also hide & disable the OBS-native
	// "Copy" action.
	//
	StreamElementsBrowserWidget *m_focusedBrowserWidget = nullptr;

	CefRefPtr<CefValue> m_auxMenuItems = CefValue::Create();
	bool m_showBuiltInMenuItems = true;
};
