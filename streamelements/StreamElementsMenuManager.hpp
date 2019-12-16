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

	class aux_menu_item_t
	{
	public:
		enum aux_menu_item_type_t type = Command;
		std::string title;
		std::string apiMethod;
		CefRefPtr<CefListValue> apiArgs;
		std::vector<aux_menu_item_t> items;

		aux_menu_item_t() {}
		~aux_menu_item_t() {}

		aux_menu_item_t(const aux_menu_item_t& o) {
			type = o.type;
			title = o.title;
			apiMethod = o.apiMethod;
			if (o.apiArgs) {
				apiArgs = o.apiArgs->Copy();
			}
			if (o.items.size()) {
				items.resize(o.items.size());

				std::copy(o.items.begin(), o.items.end(),
					  items.begin());
			}
		}
	};

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
	void AddAuxiliaryMenuItems(QMenu *menu,
				   std::vector<aux_menu_item_t> &auxMenuItems,
				   bool requestSeparator);

	bool DeserializeAuxiliaryMenuItemsInternal(
		CefRefPtr<CefValue> input,
		std::vector<aux_menu_item_t> &result);

	CefRefPtr<CefListValue> SerializeAuxiliaryMenuItemsInternal(
		std::vector<aux_menu_item_t> &items);

private:
	QMainWindow* m_mainWindow;
	QMenu* m_menu;
	std::vector<aux_menu_item_t> m_auxMenuItems;
	StreamElementsApiMessageHandler::InvokeHandler *m_handler;
};
