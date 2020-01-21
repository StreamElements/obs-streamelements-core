#pragma once

#include "cef-headers.hpp"

#include "StreamElementsApiMessageHandler.hpp"

#include <QMainWindow>
#include <QMenuBar>
#include <QMenu>

#include <string>
#include <vector>

class StreamElementsMenuManager
{
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

protected:
	QMainWindow* mainWindow() { return m_mainWindow; }

	void SaveConfig();
	void LoadConfig();

private:
	QMainWindow* m_mainWindow;
	QMenu* m_menu;
	CefRefPtr<CefValue> m_auxMenuItems = CefValue::Create();
};
