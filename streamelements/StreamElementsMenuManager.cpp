#include "StreamElementsMenuManager.hpp"
#include "StreamElementsGlobalStateManager.hpp"
#include "StreamElementsConfig.hpp"

#include <callback/signal.h>

#include "../cef-headers.hpp"

#include <QObjectList>
#include <QDesktopServices>
#include <QUrl>

#include <algorithm>

StreamElementsMenuManager::StreamElementsMenuManager(QMainWindow *parent)
	: m_mainWindow(parent)
{
	m_handler = new StreamElementsApiMessageHandler::InvokeHandler();

	m_menu = new QMenu("St&reamElements");

	mainWindow()->menuBar()->addMenu(m_menu);

	LoadConfig();
}

StreamElementsMenuManager::~StreamElementsMenuManager()
{
	m_menu->menuAction()->setVisible(false);
	m_menu = nullptr;

	delete m_handler;
	m_handler = nullptr;
}

void StreamElementsMenuManager::AddAuxiliaryMenuItems(
	QMenu *menu, std::vector<aux_menu_item_t> &auxMenuItems,
	bool requestSeparator)
{
	if (!auxMenuItems.size())
		return;

	if (requestSeparator)
		menu->addSeparator();

	for (aux_menu_item_t &item : auxMenuItems) {
		switch (item.type) {
		case Separator:
			menu->addSeparator();
			break;

		case Command: {
			QAction *auxAction = new QAction(item.title.c_str());

			menu->addAction(auxAction);

			auxAction->connect(
				auxAction, &QAction::triggered, [item, this]() {
					// Handle menu action
					m_handler->InvokeApiCallAsync(
						item.apiMethod, item.apiArgs,
						[](CefRefPtr<CefValue>) {});
				});
		} break;

		case Container:
			AddAuxiliaryMenuItems(menu->addMenu(item.title.c_str()),
					      item.items, false);
			break;

		default:
			// Invalid menu item type
			break;
		}
	}
}

void StreamElementsMenuManager::Update()
{
	SYNC_ACCESS();

	if (!m_menu)
		return;

	m_menu->clear();

	auto addURL = [this](QString title, QString url) {
		QAction *menu_action = new QAction(title);
		m_menu->addAction(menu_action);

		menu_action->connect(
			menu_action, &QAction::triggered, [this, url] {
				QUrl navigate_url =
					QUrl(url, QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});
	};

	QAction *onboarding_action = new QAction(
		obs_module_text("StreamElements.Action.ForceOnboarding"));
	m_menu->addAction(onboarding_action);
	onboarding_action->connect(onboarding_action, &QAction::triggered, [this] {
		QtPostTask(
			[](void *) -> void {
				StreamElementsGlobalStateManager::GetInstance()
					->Reset(false,
						StreamElementsGlobalStateManager::
							OnBoarding);
			},
			nullptr);
	});

	addURL(obs_module_text("StreamElements.Action.Overlays"),
	       obs_module_text("StreamElements.Action.Overlays.URL"));
	addURL(obs_module_text("StreamElements.Action.GroundControl"),
	       obs_module_text("StreamElements.Action.GroundControl.URL"));
	m_menu->addSeparator();
	{
		StreamElementsGlobalStateManager::GetInstance()
			->GetWidgetManager()
			->EnterCriticalSection();

		std::vector<std::string> widgetIds;
		StreamElementsGlobalStateManager::GetInstance()
			->GetWidgetManager()
			->GetDockBrowserWidgetIdentifiers(widgetIds);

		std::vector<StreamElementsBrowserWidgetManager::
				    DockBrowserWidgetInfo *>
			widgets;
		for (auto id : widgetIds) {
			auto info =
				StreamElementsGlobalStateManager::GetInstance()
					->GetWidgetManager()
					->GetDockBrowserWidgetInfo(id.c_str());

			if (info) {
				widgets.push_back(info);
			}
		}

		std::sort(widgets.begin(), widgets.end(),
			  [](StreamElementsBrowserWidgetManager::
				     DockBrowserWidgetInfo *a,
			     StreamElementsBrowserWidgetManager::
				     DockBrowserWidgetInfo *b) {
				  return a->m_title < b->m_title;
			  });

		StreamElementsGlobalStateManager::GetInstance()
			->GetWidgetManager()
			->LeaveCriticalSection();

		for (auto widget : widgets) {
			// widget->m_visible
			QAction *widget_action =
				new QAction(QString(widget->m_title.c_str()));
			m_menu->addAction(widget_action);

			std::string id = widget->m_id;
			bool isVisible = widget->m_visible;

			widget_action->setCheckable(true);
			widget_action->setChecked(isVisible);

			QObject::connect(widget_action, &QAction::triggered, [this, id, isVisible, widget_action] {
				QDockWidget *dock =
					StreamElementsGlobalStateManager::
						GetInstance()
							->GetWidgetManager()
							->GetDockWidget(
								id.c_str());

				if (dock) {
					if (isVisible) {
						// Hide
						StreamElementsGlobalStateManager::GetInstance()
							->GetAnalyticsEventsManager()
							->trackDockWidgetEvent(
								dock, "Hide",
								json11::Json::object{
									{"actionSource",
									 "Menu"}});
					} else {
						// Show
						StreamElementsGlobalStateManager::GetInstance()
							->GetAnalyticsEventsManager()
							->trackDockWidgetEvent(
								dock, "Show",
								json11::Json::object{
									{"actionSource",
									 "Menu"}});
					}

					dock->setVisible(!isVisible);

					Update();
				}
			});
		}

		for (auto widget : widgets) {
			delete widget;
		}
	}
	m_menu->addSeparator();

	QAction *import_action =
		new QAction(obs_module_text("StreamElements.Action.Import"));
	m_menu->addAction(import_action);
	import_action->connect(import_action, &QAction::triggered, [this] {
		QtPostTask(
			[](void *) -> void {
				StreamElementsGlobalStateManager::GetInstance()
					->Reset(false,
						StreamElementsGlobalStateManager::
							Import);
			},
			nullptr);
	});

	AddAuxiliaryMenuItems(m_menu, m_auxMenuItems, true);

	m_menu->addSeparator();

	QAction *check_for_updates_action = new QAction(
		obs_module_text("StreamElements.Action.CheckForUpdates"));
	m_menu->addAction(check_for_updates_action);
	check_for_updates_action->connect(
		check_for_updates_action, &QAction::triggered, [this] {
			signal_handler_signal(
				obs_get_signal_handler(),
				"streamelements_request_check_for_updates",
				nullptr);
		});

	m_menu->addSeparator();

	QAction *stop_onboarding_ui = new QAction(
		obs_module_text("StreamElements.Action.StopOnBoardingUI"));
	m_menu->addAction(stop_onboarding_ui);
	stop_onboarding_ui->connect(
		stop_onboarding_ui, &QAction::triggered, [this] {
			StreamElementsGlobalStateManager::GetInstance()
				->SwitchToOBSStudio();
		});

	QAction *uninstall =
		new QAction(obs_module_text("StreamElements.Action.Uninstall"));
	m_menu->addAction(uninstall);
	uninstall->connect(uninstall, &QAction::triggered, [this] {
		StreamElementsGlobalStateManager::GetInstance()
			->UninstallPlugin();
	});

	m_menu->addSeparator();

	QAction *report_issue = new QAction(
		obs_module_text("StreamElements.Action.ReportIssue"));
	m_menu->addAction(report_issue);
	report_issue->connect(report_issue, &QAction::triggered, [this] {
		StreamElementsGlobalStateManager::GetInstance()->ReportIssue();
	});

	addURL(obs_module_text("StreamElements.Action.LiveSupport"),
	       obs_module_text("StreamElements.Action.LiveSupport.URL"));

	m_menu->addSeparator();

	{
		bool isLoggedIn =
			StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN ==
			(StreamElementsConfig::GetInstance()->GetStartupFlags() &
			 StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		QAction *logout_action = new QAction(
			isLoggedIn
				? obs_module_text(
					  "StreamElements.Action.ResetStateSignOut")
				: obs_module_text(
					  "StreamElements.Action.ResetStateSignIn"));
		m_menu->addAction(logout_action);
		logout_action->connect(
			logout_action, &QAction::triggered, [this] {
				QtPostTask(
					[](void *) -> void {
						StreamElementsGlobalStateManager::
							GetInstance()
								->Reset();
					},
					nullptr);
			});
	}
}

bool StreamElementsMenuManager::DeserializeAuxiliaryMenuItemsInternal(
	CefRefPtr<CefValue> input, std::vector<aux_menu_item_t> &result)
{
	if (input->GetType() != VTYPE_LIST)
		return false;

	CefRefPtr<CefListValue> list = input->GetList();

	result.clear();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_DICTIONARY)
			continue;

		CefRefPtr<CefDictionaryValue> d = list->GetDictionary(index);

		if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
			continue;

		aux_menu_item_t item;

		std::string type = d->GetString("type").ToString();

		if (type == "separator") {
			item.type = Separator;
		} else if (type == "command") {
			item.type = Command;

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				continue;

			if (!d->HasKey("invoke") ||
			    d->GetType("invoke") != VTYPE_STRING)
				continue;

			if (!d->HasKey("invokeArgs") ||
			    d->GetType("invokeArgs") != VTYPE_LIST)
				continue;

			item.title = d->GetString("title").ToString();
			item.apiMethod = d->GetString("invoke").ToString();
			item.apiArgs = d->GetList("invokeArgs");
		} else if (type == "container") {
			item.type = Container;

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				continue;

			if (!d->HasKey("items") ||
			    d->GetType("items") != VTYPE_LIST)
				continue;

			item.title = d->GetString("title").ToString();

			DeserializeAuxiliaryMenuItemsInternal(
				d->GetValue("items"), item.items);
		} else
			continue;

		result.push_back(item);
	}

	return true;
}

bool StreamElementsMenuManager::DeserializeAuxiliaryMenuItems(
	CefRefPtr<CefValue> input)
{
	SYNC_ACCESS();

	bool result =
		DeserializeAuxiliaryMenuItemsInternal(input, m_auxMenuItems);

	Update();

	SaveConfig();

	return result;
}

void StreamElementsMenuManager::Reset()
{
	SYNC_ACCESS();

	m_auxMenuItems.clear();

	Update();

	SaveConfig();
}

CefRefPtr<CefListValue>
StreamElementsMenuManager::SerializeAuxiliaryMenuItemsInternal(
	std::vector<aux_menu_item_t> &items)
{
	CefRefPtr<CefListValue> list = CefListValue::Create();

	for (auto item : items) {
		CefRefPtr<CefDictionaryValue> d = CefDictionaryValue::Create();

		switch (item.type) {
		case Separator:
			d->SetString("type", "separator");
			break;

		case Command:
			d->SetString("type", "command");
			d->SetString("title", item.title);
			d->SetString("invoke", item.apiMethod);
			d->SetList("invokeArgs", item.apiArgs);
			break;

		case Container:
			d->SetString("type", "container");
			d->SetString("title", item.title);
			d->SetList("items", SerializeAuxiliaryMenuItemsInternal(
						    item.items));
			break;

		default:
			// Unknown type
			continue;
			break;
		}

		list->SetDictionary(list->GetSize(), d);
	}

	return list;
}

void StreamElementsMenuManager::SerializeAuxiliaryMenuItems(
	CefRefPtr<CefValue> &output)
{
	output->SetList(SerializeAuxiliaryMenuItemsInternal(m_auxMenuItems));
}

void StreamElementsMenuManager::SaveConfig()
{
	SYNC_ACCESS();

	CefRefPtr<CefValue> val = CefValue::Create();

	val->SetList(SerializeAuxiliaryMenuItemsInternal(m_auxMenuItems));

	StreamElementsConfig::GetInstance()->SetAuxMenuItemsConfig(
		CefWriteJSON(val, JSON_WRITER_DEFAULT).ToString());
}

void StreamElementsMenuManager::LoadConfig()
{
	SYNC_ACCESS();

	CefRefPtr<CefValue> val = CefParseJSON(
		StreamElementsConfig::GetInstance()->GetAuxMenuItemsConfig(),
		JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!val.get() || val->GetType() != VTYPE_LIST)
		return;

	DeserializeAuxiliaryMenuItems(val);
}
