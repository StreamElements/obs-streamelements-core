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
	QMenu *m_menu;

	CefRefPtr<CefValue> m_auxMenuItems = CefValue::Create();
	bool m_showBuiltInMenuItems = true;
};
