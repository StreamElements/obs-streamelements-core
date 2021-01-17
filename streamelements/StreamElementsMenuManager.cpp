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
	m_auxMenuItems = CefValue::Create();
	m_auxMenuItems->SetNull();

	m_menu = new QMenu("St&reamElements");

	mainWindow()->menuBar()->addMenu(m_menu);

	LoadConfig();
}

StreamElementsMenuManager::~StreamElementsMenuManager()
{
	m_menu->menuAction()->setVisible(false);
	m_menu = nullptr;
}

void StreamElementsMenuManager::Update()
{
	QtPostTask([this]() { UpdateInternal(); });
}

void StreamElementsMenuManager::UpdateInternal()
{
	if (!m_menu)
		return;

	m_menu->clear();

	auto createURL = [this](QString title, QString url) -> QAction * {
		QAction *menu_action = new QAction(title);

		menu_action->connect(
			menu_action, &QAction::triggered, [this, url] {
				QUrl navigate_url =
					QUrl(url, QUrl::TolerantMode);
				QDesktopServices::openUrl(navigate_url);
			});

		return menu_action;
	};

	if (m_showBuiltInMenuItems) {
		QMenu *setupMenu = new QMenu(obs_module_text(
			"StreamElements.Action.SetupContainer"));

		m_menu->addMenu(setupMenu);

		QAction *onboarding_action = new QAction(obs_module_text(
			"StreamElements.Action.ForceOnboarding"));
		setupMenu->addAction(onboarding_action);
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

		setupMenu->addAction(createURL(
			obs_module_text("StreamElements.Action.Overlays"),
			obs_module_text("StreamElements.Action.Overlays.URL")));

		// Docks
		{
			QMenu *docksMenu = new QMenu(obs_module_text(
				"StreamElements.Action.DocksContainer"));

			DeserializeDocksMenu(*docksMenu);

			m_menu->addMenu(docksMenu);
		}

		QAction *import_action = new QAction(
			obs_module_text("StreamElements.Action.Import"));
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

		m_menu->addSeparator();
	}

	DeserializeMenu(m_auxMenuItems, *m_menu);

	m_menu->addSeparator();

	QAction *check_for_updates_action = new QAction(
		obs_module_text("StreamElements.Action.CheckForUpdates"));
	m_menu->addAction(check_for_updates_action);
	check_for_updates_action->connect(
		check_for_updates_action, &QAction::triggered, [this] {
			calldata_t *cd = calldata_create();
			calldata_set_bool(cd, "allow_downgrade", false);
			calldata_set_bool(cd, "force_install", false);
			calldata_set_bool(cd, "allow_use_last_response", false);

			signal_handler_signal(
				obs_get_signal_handler(),
				"streamelements_request_check_for_updates",
				cd);

			calldata_free(cd);
		});

	QMenu *helpMenu = new QMenu(
		obs_module_text("StreamElements.Action.HelpContainer"));

	m_menu->addMenu(helpMenu);

	QAction *report_issue = new QAction(
		obs_module_text("StreamElements.Action.ReportIssue"));
	helpMenu->addAction(report_issue);
	report_issue->connect(report_issue, &QAction::triggered, [this] {
		StreamElementsGlobalStateManager::GetInstance()->ReportIssue();
	});

	helpMenu->addAction(createURL(
		obs_module_text("StreamElements.Action.LiveSupport.MenuItem"),
		obs_module_text("StreamElements.Action.LiveSupport.URL")));

	m_menu->addSeparator();

	{
		bool isLoggedIn =
			StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN ==
			(StreamElementsConfig::GetInstance()->GetStartupFlags() &
			 StreamElementsConfig::STARTUP_FLAGS_SIGNED_IN);

		auto reset_action_handler = [this] {
			QtPostTask(
				[](void *) -> void {
					StreamElementsGlobalStateManager::
						GetInstance()
							->Reset();
				},
				nullptr);
		};

		if (isLoggedIn) {
			m_menu->setStyleSheet(
				"QMenu::item:default { color: #F53656; }");

			QAction *logout_action = new QAction(obs_module_text(
				"StreamElements.Action.ResetStateSignOut"));
			logout_action->setObjectName("signOutAction");

			m_menu->addAction(logout_action);
			logout_action->connect(logout_action,
					       &QAction::triggered,
					       reset_action_handler);
			m_menu->setDefaultAction(logout_action);
		} else {
			QAction *login_action = new QAction(obs_module_text(
				"StreamElements.Action.ResetStateSignIn"));
			m_menu->addAction(login_action);
			login_action->connect(login_action,
					       &QAction::triggered,
					       reset_action_handler);
		}
	}
}

bool StreamElementsMenuManager::DeserializeAuxiliaryMenuItems(
	CefRefPtr<CefValue> input)
{
	QMenu menu;
	bool result = DeserializeMenu(input, menu);

	if (result) {
		m_auxMenuItems = input->Copy();
	}

	Update();

	SaveConfig();

	return result;
}

void StreamElementsMenuManager::Reset()
{
	m_auxMenuItems->SetNull();
	m_showBuiltInMenuItems = true;

	Update();

	SaveConfig();
}

void StreamElementsMenuManager::SerializeAuxiliaryMenuItems(
	CefRefPtr<CefValue> &output)
{
	output = m_auxMenuItems->Copy();
}

void StreamElementsMenuManager::SaveConfig()
{
	StreamElementsConfig::GetInstance()->SetAuxMenuItemsConfig(
		CefWriteJSON(m_auxMenuItems, JSON_WRITER_DEFAULT).ToString());

	StreamElementsConfig::GetInstance()->SetShowBuiltInMenuItems(
		m_showBuiltInMenuItems);
}

void StreamElementsMenuManager::LoadConfig()
{
	m_showBuiltInMenuItems =
		StreamElementsConfig::GetInstance()->GetShowBuiltInMenuItems();

	CefRefPtr<CefValue> val = CefParseJSON(
		StreamElementsConfig::GetInstance()->GetAuxMenuItemsConfig(),
		JSON_PARSER_ALLOW_TRAILING_COMMAS);

	if (!val.get() || val->GetType() != VTYPE_LIST)
		return;

	DeserializeAuxiliaryMenuItems(val);
}

void StreamElementsMenuManager::SetShowBuiltInMenuItems(bool show)
{
	m_showBuiltInMenuItems = show;

	Update();
}

bool StreamElementsMenuManager::GetShowBuiltInMenuItems()
{
	return m_showBuiltInMenuItems;
}
