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

	mainWindow()->menuBar()->setFocusPolicy(Qt::NoFocus);

	mainWindow()->menuBar()->addMenu(m_menu);
	m_editMenu = mainWindow()->menuBar()->findChild<QMenu *>(
		"menuBasic_MainMenu_Edit");

	m_nativeOBSEditMenuCopySourceAction = m_editMenu->findChild<QAction *>("actionCopySource");

	//
	// Watch for clipboard content changes.
	//
	// This is necessary to enable/disable our CEF Paste action based
	// on the contents of the clipboard.
	//
	QObject::connect(
		qApp->clipboard(), &QClipboard::dataChanged, this,
		&StreamElementsMenuManager::HandleClipboardDataChanged);

	UpdateOBSEditMenuInternal();

	LoadConfig();
}

StreamElementsMenuManager::~StreamElementsMenuManager()
{
	RemoveOBSEditMenuActions();

	QObject::disconnect(
		qApp->clipboard(), &QClipboard::dataChanged, this,
		&StreamElementsMenuManager::HandleClipboardDataChanged);

	//mainWindow()->menuBar()->removeAction((QAction *)m_menu->menuAction()); // -> crash
	m_menu->menuAction()->setVisible(false);
	m_menu = nullptr;
}

//
// Add our own OBS-native Edit menu items.
//
// Items will be enabled based on whether an editable DOM
// node is currently selected in the browser.
//
void StreamElementsMenuManager::AddOBSEditMenuActions()
{
	RemoveOBSEditMenuActions();

	m_cefOBSEditMenuActionCopy =
		new QAction(obs_module_text("StreamElements.Action.Copy"));
	m_cefOBSEditMenuActionCut =
		new QAction(obs_module_text("StreamElements.Action.Cut"));
	m_cefOBSEditMenuActionPaste =
		new QAction(obs_module_text("StreamElements.Action.Paste"));
	m_cefOBSEditMenuActionSelectAll =
		new QAction(obs_module_text("StreamElements.Action.SelectAll"));

	m_cefOBSEditMenuActionCopy->setShortcut(QKeySequence::Copy);
	m_cefOBSEditMenuActionCut->setShortcut(QKeySequence::Cut);
	m_cefOBSEditMenuActionPaste->setShortcut(QKeySequence::Paste);
	m_cefOBSEditMenuActionSelectAll->setShortcut(QKeySequence::SelectAll);

	const bool isEditable =
		m_focusedBrowserWidget->isBrowserFocusedDOMNodeEditable();

	m_cefOBSEditMenuActionCopy->setEnabled(isEditable);
	m_cefOBSEditMenuActionCut->setEnabled(isEditable);
	m_cefOBSEditMenuActionSelectAll->setEnabled(isEditable);

	auto mimeData = qApp->clipboard()->mimeData();

	const bool hasTextToPaste = mimeData && mimeData->hasText();

	m_cefOBSEditMenuActionPaste->setEnabled(isEditable && hasTextToPaste);

	m_cefOBSEditMenuActions.push_back(m_cefOBSEditMenuActionCopy);
	m_cefOBSEditMenuActions.push_back(m_cefOBSEditMenuActionCut);
	m_cefOBSEditMenuActions.push_back(m_cefOBSEditMenuActionPaste);
	m_cefOBSEditMenuActions.push_back(m_cefOBSEditMenuActionSelectAll);

	m_cefOBSEditMenuActions.push_back(
		m_editMenu->insertSeparator(m_editMenu->actions().at(0)));
	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefOBSEditMenuActionSelectAll);
	m_cefOBSEditMenuActions.push_back(
		m_editMenu->insertSeparator(m_editMenu->actions().at(0)));
	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefOBSEditMenuActionPaste);
	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefOBSEditMenuActionCopy);
	m_editMenu->insertAction(m_editMenu->actions().at(0),
				 m_cefOBSEditMenuActionCut);

	QObject::connect(m_cefOBSEditMenuActionCopy, &QAction::triggered, this,
			 &StreamElementsMenuManager::HandleCefCopy);
	QObject::connect(m_cefOBSEditMenuActionCut, &QAction::triggered, this,
			 &StreamElementsMenuManager::HandleCefCut);
	QObject::connect(m_cefOBSEditMenuActionPaste, &QAction::triggered, this,
			 &StreamElementsMenuManager::HandleCefPaste);
	QObject::connect(m_cefOBSEditMenuActionSelectAll, &QAction::triggered,
			 this,
			 &StreamElementsMenuManager::HandleCefSelectAll);
}

//
// Remove our own OBS-native Edit menu items.
//
void StreamElementsMenuManager::RemoveOBSEditMenuActions()
{
	if (!m_cefOBSEditMenuActions.size())
		return;

	QObject::disconnect(m_cefOBSEditMenuActionCopy, &QAction::triggered, this,
			    &StreamElementsMenuManager::HandleCefCopy);
	QObject::disconnect(m_cefOBSEditMenuActionCut, &QAction::triggered, this,
			    &StreamElementsMenuManager::HandleCefCut);
	QObject::disconnect(m_cefOBSEditMenuActionPaste, &QAction::triggered, this,
			    &StreamElementsMenuManager::HandleCefPaste);
	QObject::disconnect(m_cefOBSEditMenuActionSelectAll, &QAction::triggered,
			    this,
			    &StreamElementsMenuManager::HandleCefSelectAll);

	for (auto i : m_cefOBSEditMenuActions) {
		m_editMenu->removeAction(i);
	}
	m_cefOBSEditMenuActions.clear();
}

//
// Called by StreamElementsBrowserWidget to indicate which browser
// widget is currently in focus.
//
// Setting <widget> = <nullptr> indicates that no browser widget
// is in focus at the moment.
//
void StreamElementsMenuManager::SetFocusedBrowserWidget(
	StreamElementsBrowserWidget *widget)
{
	// Must be called on QT app thread

	if (m_focusedBrowserWidget == widget)
		return;

	if (!widget && !!m_focusedBrowserWidget) {
		//
		// We are signalled that there is no focused widget:
		// stop tracking focused DOM node on the currently focused widget.
		//
		QObject::disconnect(
			m_focusedBrowserWidget,
			&StreamElementsBrowserWidget::
				browserFocusedDOMNodeEditableChanged,
			this,
			&StreamElementsMenuManager::
				HandleFocusedBrowserWidgetDOMNodeEditableChanged);
	}

	m_focusedBrowserWidget = widget;

	if (!!m_focusedBrowserWidget) {
		//
		// We have a focused widget: begin tracking focused DOM node on the
		// currently focused widget.
		//
		// This is necessary to enable Cut/Copy/Paste/Select All operations
		// only when an editable field is selected.
		//
		QObject::connect(m_focusedBrowserWidget,
				 &StreamElementsBrowserWidget::
					 browserFocusedDOMNodeEditableChanged,
				 this,
				 &StreamElementsMenuManager::
					 HandleFocusedBrowserWidgetDOMNodeEditableChanged);
	}

	UpdateOBSEditMenuInternal();
}

void StreamElementsMenuManager::HandleCefCopy()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserCopy();
}

void StreamElementsMenuManager::HandleCefCut()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserCut();
}

void StreamElementsMenuManager::HandleCefPaste()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserPaste();
}

void StreamElementsMenuManager::HandleCefSelectAll()
{
	// Must be called on QT app thread

	if (!m_focusedBrowserWidget)
		return;

	m_focusedBrowserWidget->BrowserSelectAll();
}

//
// Invoked when focused browser widget editable state changes
//
void StreamElementsMenuManager::HandleFocusedBrowserWidgetDOMNodeEditableChanged(
	bool isEditable)
{
	// Must be called on QT app thread

	UpdateOBSEditMenuInternal();
}

//
// Invoked when data on clipboard changes
//
void StreamElementsMenuManager::HandleClipboardDataChanged()
{
	// Must be called on QT app thread

	UpdateOBSEditMenuInternal();
}

void StreamElementsMenuManager::Update()
{
	SYNC_ACCESS();

	UpdateInternal();
}

//
// Recalculate OBS-native Edit menu items' availability & state.
//
void StreamElementsMenuManager::UpdateOBSEditMenuInternal()
{
	// Must be called on QT app thread

	const bool editMenuVisible = !!m_focusedBrowserWidget;

	m_nativeOBSEditMenuCopySourceAction->setVisible(!editMenuVisible);

	//
	// On macOS, having a shortcut assigned to two menu items will result in the
	// second item's shortcut not being respected, even if the first menu item is disabled.
	//
	// We must remove the native menu item shortcut when our "Copy" menu item is active
	// and restore the native menu item shortcut when our menu item is removed.
	//
	if (editMenuVisible) {
		m_nativeOBSEditMenuCopySourceAction->setShortcut(QKeySequence());
	} else {
		m_nativeOBSEditMenuCopySourceAction->setShortcut(
			QKeySequence::Copy);
	}

	//
	// On macOS it is not enough to alter menu items' visibility and/or enabled
	// state for the changes to take effect.
	//
	// We must remove our items and add them dynamically when changes are required.
	//
	RemoveOBSEditMenuActions();

	if (editMenuVisible) {
		AddOBSEditMenuActions();
	}
}

void StreamElementsMenuManager::UpdateInternal()
{
	SYNC_ACCESS();

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
			StreamElementsGlobalStateManager::GetInstance()
				->Reset(false,
					StreamElementsGlobalStateManager::
						OnBoarding);
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
			StreamElementsGlobalStateManager::GetInstance()
				->Reset(false,
					StreamElementsGlobalStateManager::
						Import);
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
			StreamElementsGlobalStateManager::
				GetInstance()
					->Reset();
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
	SYNC_ACCESS();

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
	SYNC_ACCESS();

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
	SYNC_ACCESS();

	StreamElementsConfig::GetInstance()->SetAuxMenuItemsConfig(
		CefWriteJSON(m_auxMenuItems, JSON_WRITER_DEFAULT).ToString());

	StreamElementsConfig::GetInstance()->SetShowBuiltInMenuItems(
		m_showBuiltInMenuItems);
}

void StreamElementsMenuManager::LoadConfig()
{
	SYNC_ACCESS();

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
