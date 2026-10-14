#include "SEAuxiliaryControlsBuilder.hpp"

#include "SEAnalyticsEventsManager.hpp"
#include "SEBrowserWidgetManager.hpp"
#include "SEMenuManager.hpp"
#include "SERemoteIconLoader.hpp"

#include <QAction>
#include <QCursor>
#include <QDockWidget>
#include <QMenu>
#include <QPushButton>
#include <QStyle>
#include <QStyleOptionButton>
#include <QWidget>

#include <algorithm>

class QRemoteIconMenu : public QMenu {
public:
	QRemoteIconMenu(const char *iconUrl, QPixmap *defaultPixmap = nullptr)
		: QMenu(),
		  loader(SERemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		QObject::connect(this, &QObject::destroyed,
				 [this]() { loader->Cancel(); });
	}

private:
	CefRefPtr<SERemoteIconLoader> loader;
};
class QRemoteIconAction : public QAction {
public:
	QRemoteIconAction(const char *iconUrl, QPixmap *defaultPixmap = nullptr)
		: QAction(),
		  loader(SERemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		QObject::connect(this, &QObject::destroyed,
				 [this]() { loader->Cancel(); });
	}

private:
	CefRefPtr<SERemoteIconLoader> loader;
};
class QRemoteIconPushButton : public QPushButton {
public:
	QRemoteIconPushButton(const char *iconUrl,
			      QPixmap *defaultPixmap = nullptr)
		: QPushButton(),
		  loader(SERemoteIconLoader::Create(
			  [this](const QIcon &m_icon) { setIcon(m_icon); },
			  iconUrl, defaultPixmap, false))
	{
		setMouseTracking(true);

		setStyleSheet("background: none; padding: 0;");

		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Minimum);

		CefRefPtr<SERemoteIconLoader> loaderCopy = loader;

		QObject::connect(this, &QObject::destroyed,
				 [loaderCopy]() { loaderCopy->Cancel(); });
	}

	virtual QSize minimumSizeHint() const override { return sizeHint(); }
	virtual QSize sizeHint() const override
	{
		if (text().size()) {
			QSize textSize = fontMetrics().size(
				Qt::TextShowMnemonic, text());

			QStyleOptionButton opt;
			opt.initFrom(this);
			opt.rect.setSize(textSize);

			QSize size = style()->sizeFromContents(
				QStyle::CT_PushButton, &opt, textSize, this);

			return size;
		} else {
			return QSize(16, 16);
		}
	}

private:
	CefRefPtr<SERemoteIconLoader> loader;
};

///////////////////////////////////////////////////////////////////////

SEAuxiliaryControlsBuilder::SEAuxiliaryControlsBuilder(
	std::weak_ptr<SEApiServices> apiServices)
	: m_services(apiServices)
{
}

SEAuxiliaryControlsBuilder::~SEAuxiliaryControlsBuilder() {}

std::shared_ptr<SEApiMessageHandler::InvokeHandler>
SEAuxiliaryControlsBuilder::EnsureInvokeHandler()
{
	std::lock_guard<decltype(m_invokeHandlerMutex)> guard(
		m_invokeHandlerMutex);

	if (!m_invokeHandler) {
		//
		// The handler keeps the bundle weakly itself, so this is the
		// only place that has to be able to see it at all.
		//
		auto services = m_services.lock();

		if (!services)
			return nullptr;

		m_invokeHandler =
			std::make_shared<SEApiMessageHandler::InvokeHandler>(
				"globalInvokeHandler", services);
	}

	return m_invokeHandler;
}

QWidget *SEAuxiliaryControlsBuilder::DeserializeRemoteIconWidget(
	CefRefPtr<CefValue> input, QPixmap *defaultPixmap)
{
	std::string iconUrl = "";

	if (input.get() && input->GetType() == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

		if (d->HasKey("url") && d->GetType("url") == VTYPE_STRING) {
			iconUrl = d->GetString("url").ToString();
		}
	}

	return new QRemoteIconPushButton(iconUrl.c_str(), defaultPixmap);
}

bool SEAuxiliaryControlsBuilder::DeserializeAndInvokeAction(
	CefRefPtr<CefValue> input, std::function<void()> defaultAction,
	std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return false;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("invoke") || d->GetType("invoke") != VTYPE_STRING)
		return false;

	std::string invoke = d->GetString("invoke");

	CefRefPtr<CefListValue> invokeArgs = CefListValue::Create();

	if (d->HasKey("invokeArgs") && d->GetType("invokeArgs") == VTYPE_LIST)
		invokeArgs = d->GetList("invokeArgs")->Copy();

	if (invoke == ":defaultAction") {
		defaultAction();

		return true;
	} else if (invoke == ":defaultContextMenu") {
		defaultContextMenu();

		return true;
	} else if (invoke == ":none") {
		// No action
		return true;
	} else {
		auto invokeHandler = EnsureInvokeHandler();

		if (!invokeHandler)
			return false;

		return invokeHandler->InvokeApiCallAsync(
			invoke, invokeArgs, [](CefRefPtr<CefValue>) {});
	}
}

bool SEAuxiliaryControlsBuilder::DeserializeDocksMenu(QMenu &menu)
{
	auto services = m_services.lock();

	if (!services)
		return true;

	auto widgetManager = services->widgetManager;

	if (!widgetManager)
		return true;

	widgetManager->EnterCriticalSection();

	std::vector<std::string> widgetIds;

	widgetManager->GetDockBrowserWidgetIdentifiers(widgetIds);

	std::vector<SEBrowserWidgetManager::DockBrowserWidgetInfo *> widgets;

	for (auto id : widgetIds) {
		auto info = widgetManager->GetDockBrowserWidgetInfo(id.c_str());

		if (info) {
			widgets.push_back(info);
		}
	}

	std::sort(widgets.begin(), widgets.end(),
		  [](SEBrowserWidgetManager::DockBrowserWidgetInfo *a,
		     SEBrowserWidgetManager::DockBrowserWidgetInfo *b) {
			  return a->m_title < b->m_title;
		  });

	//
	// Weak: the actions below live on a menu Qt owns, so they can fire
	// after this service is gone.
	//
	std::weak_ptr<SEApiServices> weakServices = m_services;

	for (auto widget : widgets) {
		// widget->m_visible
		QAction *widget_action =
			new QAction(QString(widget->m_title.c_str()));
		menu.addAction(widget_action);

		std::string id = widget->m_id;
		bool isVisible = widget->m_visible;

		widget_action->setCheckable(true);
		widget_action->setChecked(isVisible);

		QObject::connect(widget_action, &QAction::triggered, [id, isVisible, weakServices] {
			auto services = weakServices.lock();

			if (!services)
				return;

			auto widgetManager = services->widgetManager;

			if (!widgetManager)
				return;

			widgetManager->EnterCriticalSection();

			QDockWidget *dock =
				widgetManager->GetDockWidget(id.c_str());

			if (dock) {
				if (services->analyticsEventsManager) {
					services->analyticsEventsManager->trackEvent(
						isVisible
							? "se_live_dock_hide_click"
							: "se_live_dock_show_click",
						json11::Json::object{
							{"type",
							 "button_click"},
							{"placement", "menu"}},
						json11::Json::array{json11::Json::array{
							"dock_widget_title",
							dock->windowTitle()
								.toStdString()}});
				}

				dock->setVisible(!isVisible);

				if (services->menuManager)
					services->menuManager->Update();
			}

			widgetManager->LeaveCriticalSection();
		});
	}

	for (auto widget : widgets) {
		delete widget;
	}

	widgetManager->LeaveCriticalSection();

	return true;
}

bool SEAuxiliaryControlsBuilder::DeserializeMenu(
	CefRefPtr<CefValue> input, QMenu &menu,
	std::function<void()> defaultAction,
	std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_LIST)
		return false;

	auto getIconUrl =
		[](CefRefPtr<CefDictionaryValue> parent) -> std::string {
		if (!parent->HasKey("icon") ||
		    parent->GetType("icon") != VTYPE_DICTIONARY)
			return "";

		CefRefPtr<CefDictionaryValue> d = parent->GetDictionary("icon");

		if (!d->HasKey("url") || d->GetType("url") != VTYPE_STRING)
			return "";

		std::string url = d->GetString("url").ToString();

		return url;
	};

	std::weak_ptr<SEAuxiliaryControlsBuilder> weakSelf = weak_from_this();

	CefRefPtr<CefListValue> list = input->GetList();

	for (size_t index = 0; index < list->GetSize(); ++index) {
		if (list->GetType(index) != VTYPE_DICTIONARY)
			return false;

		CefRefPtr<CefDictionaryValue> d = list->GetDictionary(index);

		if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
			return false;

		std::string type = d->GetString("type");

		if (type == "separator") {
			menu.addSeparator();
		} else if (type == "command") {
			bool enabled = true;

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				return false;

			if (enabled) {
				if (!d->HasKey("invoke") ||
				    d->GetType("invoke") != VTYPE_STRING)
					return false;
			}

			std::string title = d->GetString("title");

			std::string iconUrl = getIconUrl(d);

			QAction *auxAction = new QRemoteIconAction(
				iconUrl.size() ? iconUrl.c_str() : nullptr);

			auxAction->setText(title.c_str());
			auxAction->setEnabled(enabled);

			menu.addAction(auxAction);

			CefRefPtr<CefValue> action = CefValue::Create();
			action->SetDictionary(d->Copy(false));

			auxAction->connect(
				auxAction, &QAction::triggered,
				[action, defaultAction, defaultContextMenu,
				 weakSelf]() {
					auto self = weakSelf.lock();

					if (self)
						self->DeserializeAndInvokeAction(
							action, defaultAction,
							defaultContextMenu);

					return true;
				});
		} else if (type == "container") {
			bool enabled = true;

			if (d->HasKey("enabled") &&
			    d->GetType("enabled") == VTYPE_BOOL) {
				enabled = d->GetBool("enabled");
			}

			if (!d->HasKey("title") ||
			    d->GetType("title") != VTYPE_STRING)
				return false;

			std::string iconUrl = getIconUrl(d);

			if (d->HasKey("items") &&
			    d->GetType("items") == VTYPE_LIST) {
				QMenu *submenu = new QRemoteIconMenu(
					iconUrl.size() ? iconUrl.c_str()
						       : nullptr);

				submenu->setTitle(d->GetString("title")
							  .ToString()
							  .c_str());

				submenu->setEnabled(enabled);

				menu.addMenu(submenu);

				if (!DeserializeMenu(d->GetValue("items"),
						     *submenu))
					return false;
			} else if (d->HasKey("itemsSource") &&
				   d->GetType("itemsSource") == VTYPE_STRING) {
				std::string itemsSource =
					d->GetString("itemsSource");

				if (itemsSource == ":dockingWidgets") {
					QMenu *submenu = new QRemoteIconMenu(
						iconUrl.size() ? iconUrl.c_str()
							       : nullptr);

					submenu->setTitle(d->GetString("title")
								  .ToString()
								  .c_str());

					submenu->setEnabled(enabled);

					menu.addMenu(submenu);

					if (!DeserializeDocksMenu(*submenu))
						return false;
				} else {
					return false;
				}
			} else {
				return false;
			}
		} else {
			return false;
		}
	}

	return true;
}

QWidget *SEAuxiliaryControlsBuilder::DeserializeAuxiliaryControlWidget(
	CefRefPtr<CefValue> input, std::function<void()> defaultAction,
	std::function<void()> defaultContextMenu)
{
	if (!input.get() || input->GetType() != VTYPE_DICTIONARY)
		return nullptr;

	CefRefPtr<CefDictionaryValue> d = input->GetDictionary();

	if (!d->HasKey("type") || d->GetType("type") != VTYPE_STRING)
		return nullptr;

	std::string type = d->GetString("type");

	std::string iconUrl = "";

	if (d->HasKey("icon") && d->GetType("icon") == VTYPE_DICTIONARY) {
		CefRefPtr<CefDictionaryValue> m_icon = d->GetDictionary("icon");

		if (m_icon->HasKey("url") &&
		    m_icon->GetType("url") == VTYPE_STRING) {
			iconUrl = m_icon->GetString("url");
		}
	}

	std::string styleSheet =
		"QToolTip { color: palette(text); background-color: palette(dark); } ";

	if (d->HasKey("color") && d->GetType("color") == VTYPE_STRING) {
		styleSheet += "QPushButton { color: ";
		styleSheet += d->GetString("color").ToString() + ";";
		styleSheet += " } ";
	}

	styleSheet +=
		"QPushButton { background-color: palette(button); padding: 1; padding-left: 1em; padding-right: 1em; } ";
	styleSheet +=
		"QPushButton:hover { background-color: palette(midlight); color: palette(dark); } ";
	styleSheet +=
		"QPushButton:pressed { background-color: palette(shadow); color: palette(bright-text); } ";

	std::weak_ptr<SEAuxiliaryControlsBuilder> weakSelf = weak_from_this();

	if (type == "container") {
		if (!d->HasKey("items") || d->GetType("items") != VTYPE_LIST)
			return nullptr;

		CefRefPtr<CefValue> items = d->GetValue("items")->Copy();

		QRemoteIconPushButton *control =
			new QRemoteIconPushButton(iconUrl.c_str());

		//control->setContentsMargins(0, 0, 0, 0);
		//control->setMinimumSize(16, 16);
		control->setSizePolicy(QSizePolicy::Minimum,
				       QSizePolicy::Minimum);

		auto HandleClick = [items, control,
				    weakSelf](bool /*checked*/) {
			auto self = weakSelf.lock();

			if (!self)
				return;

			QMenu menu;

			if (!self->DeserializeMenu(items, menu))
				return;

			// menu.exec(control->mapToGlobal(QPoint(0, 0)));
			menu.exec(QCursor::pos());
		};

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());
		}

		if (d->HasKey("tooltip") &&
		    d->GetType("tooltip") == VTYPE_STRING) {
			std::string tooltip =
				d->GetString("tooltip").ToString();

			control->setToolTip(tooltip.c_str());
		}

		control->setStyleSheet(styleSheet.c_str());

		QObject::connect(control, &QPushButton::clicked, HandleClick);

		return control;
	} else if (type == "command") {
		if (!d->HasKey("invoke") ||
		    d->GetType("invoke") != VTYPE_STRING)
			return nullptr;

		CefRefPtr<CefValue> action = input->Copy();

		auto HandleClick = [action, defaultAction, defaultContextMenu,
				    weakSelf](bool /*checked*/) {
			auto self = weakSelf.lock();

			if (!self)
				return;

			self->DeserializeAndInvokeAction(action, defaultAction,
							 defaultContextMenu);
		};

		QRemoteIconPushButton *control =
			new QRemoteIconPushButton(iconUrl.c_str());

		//control->setContentsMargins(0, 0, 0, 0);
		//control->setMinimumSize(16, 16);
		//control->setSizePolicy(QSizePolicy::Minimum,
		//		       QSizePolicy::Minimum);

		if (d->HasKey("title") && d->GetType("title") == VTYPE_STRING) {
			control->setText(
				d->GetString("title").ToString().c_str());

			control->setWindowIconText(
				d->GetString("title").ToString().c_str());
		}

		if (d->HasKey("tooltip") &&
		    d->GetType("tooltip") == VTYPE_STRING) {
			std::string tooltip =
				d->GetString("tooltip").ToString();

			control->setToolTip(tooltip.c_str());
		}

		control->setStyleSheet(styleSheet.c_str());

		QObject::connect(control, &QPushButton::clicked, HandleClick);

		return control;
	} else if (type == "separator") {
		QWidget *control = new QWidget();
		control->setContentsMargins(0, 0, 0, 0);
		control->setStyleSheet("background: none");
		control->setFixedSize(2, 2);

		return control;
	}

	return nullptr;
}
