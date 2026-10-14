#include <string.h>

#include "SELocalFilesystemHttpServer.hpp"
#include "SEUtils.hpp"
#include "SEConfig.hpp"
#include "SEGlobalStateManager.hpp"
#include "SERemoteIconLoader.hpp"
#include "SEPleaseWaitWindow.hpp"
#include "Version.hpp"
#include "wide-string.hpp"
#include "deps/utf8.h"

#define GLOBAL_ENV_CONFIG_FILE_NAME "obs-studio/streamelements-env.ini"

#include <cstdint>
#include <atomic>
#include <chrono>
#include <vector>
#include <regex>
#include <unordered_map>
#include <filesystem>
#include <regex>

#include <curl/curl.h>

#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/config-file.h>
#include <obs.hpp>

#include <QUrl>
#include <QUrlQuery>
#include <QFile>
#include <QDir>
#include <QUrl>
#include <QApplication>
#include <QProcess>
#include <QLatin1String>
#include <QStringList>

#include "deps/picosha2/picosha2.h"

#ifndef WIN32
#include <mach/mach_types.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/host_info.h>
#include <mach/mach_time.h>
#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#endif

#ifndef WIN32
#define sprintf_s sprintf

#ifndef WCHAR
typedef wchar_t WCHAR;
#endif

#ifndef DWORD
typedef unsigned long DWORD;
#endif

#ifndef HKEY
typedef void *HKEY;
#endif

#include <sys/syscall.h>
#endif

/* ========================================================= */


/* ========================================================= */

static config_t *obs_fe_user_config()
{
	auto config = SEConfig::GetInstance();

	if (config) {
		return config->GetObsUserConfig();
	}

	return nullptr;
}

/* ========================================================= */


/* ========================================================= */


/* ========================================================= */

bool IsTraceLogLevel()
{
	static bool has_result = false;
	static bool result = false;

	if (!has_result) {
		std::string search = "--setrace";

		QStringList args = QCoreApplication::instance()->arguments();

		for (int i = 0; i < args.size() && !has_result; ++i) {
			std::string arg = args.at(i).toStdString();

			if (arg.substr(0, search.size()) == search) {
				result = true;
				break;
			}
		}
	}

	has_result = true;

	return result;
}

/* ========================================================= */

// convert wstring to UTF-8 string
static std::vector<std::string> tokenizeString(const std::string &str,
					       const std::string &delimiters)
{
	std::vector<std::string> tokens;
	// Skip delimiters at beginning.
	std::string::size_type lastPos = str.find_first_not_of(delimiters, 0);
	// Find first "non-delimiter".
	std::string::size_type pos = str.find_first_of(delimiters, lastPos);

	while (std::string::npos != pos ||
	       std::string::npos !=
		       lastPos) { // Found a token, add it to the vector.
		tokens.push_back(str.substr(lastPos, pos - lastPos));
		// Skip delimiters.  Note the "not_of"
		lastPos = str.find_first_not_of(delimiters, pos);
		// Find next "non-delimiter"
		pos = str.find_first_of(delimiters, lastPos);
	}
	return tokens;
}

/* ========================================================= */


/* ========================================================= */








/* ========================================================= */



/* ========================================================= */

std::string DockWidgetAreaToString(const Qt::DockWidgetArea area)
{
	switch (area) {
	case Qt::LeftDockWidgetArea:
		return "left";
	case Qt::RightDockWidgetArea:
		return "right";
	case Qt::TopDockWidgetArea:
		return "top";
	case Qt::BottomDockWidgetArea:
		return "bottom";
	case Qt::NoDockWidgetArea:
	default:
		return "floating";
	}
}

std::string GetCommandLineOptionValue(const std::string key)
{
	QStringList args = QCoreApplication::instance()->arguments();

	std::string search = "--" + key + "=";

	for (int i = 0; i < args.size(); ++i) {
		std::string arg = args.at(i).toStdString();

		if (arg.substr(0, search.size()) == search) {
			return arg.substr(search.size());
		}
	}

	return std::string();
}

std::string LoadResourceString(std::string path)
{
	std::string result = "";

	QFile file(QString(path.c_str()));

	if (file.open(QFile::ReadOnly | QFile::Text)) {
		QTextStream stream(&file);

		result = stream.readAll().toStdString();
	}

	return result;
}

/* ========================================================= */

#ifdef WIN32
#endif





#ifdef _WIN32
#endif

/* ========================================================= */







std::string SerializeAppStyleSheet()
{
	std::string result = qApp->styleSheet().toStdString();

	if (result.compare(0, 8, "file:///") == 0) {
		QUrl url(result.c_str());

		if (url.isLocalFile()) {
			result = result.substr(8);

			QFile file(result.c_str());

			if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
				QTextStream in(&file);

				result = in.readAll().toStdString();
			}
		}
	}

	return result;
}

std::string GetAppStyleSheetSelectorContent(std::string selector)
{
	std::string result;

	std::string css = SerializeAppStyleSheet();

	std::replace(css.begin(), css.end(), '\n', ' ');

	css = std::regex_replace(css, std::regex("/\\*.*?\\*/"), "");

	std::regex selector_regex("[^\\s]*" + selector + "[\\s]*\\{(.*?)\\}");
	std::smatch selector_match;

	if (std::regex_search(css, selector_match, selector_regex)) {
		result = std::string(selector_match[1].first,
				     selector_match[1].second);
	}

	return result;
}

std::string GetCurrentThemeName()
{
	std::string result = "Default";

	config_t *globalConfig =
		obs_fe_user_config(); // does not increase refcount

	if (globalConfig) {
		const char *themeName = config_get_string(
			globalConfig, "General", "CurrentTheme");
		if (!themeName) {
			/* Use deprecated "Theme" value if available */
			themeName = config_get_string(globalConfig, "General",
						      "Theme");
			if (!themeName) {
				themeName = "Default";
			}

			result = themeName;
		}
	}

	std::string appStyle = qApp->styleSheet().toStdString();

	if (appStyle.substr(0, 7) == "file://") {
		QUrl url(appStyle.c_str());

		if (url.isLocalFile()) {
			result = url.fileName().split('.')[0].toStdString();
		}
	}

	return result;
}






/* ========================================================= */

#ifdef WIN32
#include <winhttp.h>
#pragma comment(lib, "Winhttp.lib")
void SetGlobalCURLOptions(CURL *curl, const char *url)
{
	// TODO: TBD: MacOS: http://mirror.informatimago.com/next/developer.apple.com/qa/qa2001/qa1234.html
	std::string proxy =
		GetCommandLineOptionValue("streamelements-http-proxy");

	if (!proxy.size()) {
		WINHTTP_CURRENT_USER_IE_PROXY_CONFIG config;

		if (WinHttpGetIEProxyConfigForCurrentUser(&config)) {
			// http=127.0.0.1:8888;https=127.0.0.1:8888
			if (config.lpszProxy) {
				proxy = wstring_to_utf8(config.lpszProxy);

				std::map<std::string, std::string> schemes;
				for (auto kvstr : tokenizeString(proxy, ";")) {
					std::vector<std::string> kv =
						tokenizeString(kvstr, "=");

					if (kv.size() == 2) {
						std::transform(kv[0].begin(),
							       kv[0].end(),
							       kv[0].begin(),
							       tolower);
						schemes[kv[0]] = kv[1];
					}
				}

				std::string scheme =
					tokenizeString(url, ":")[0];
				std::transform(scheme.begin(), scheme.end(),
					       scheme.begin(), tolower);

				if (schemes.count(scheme)) {
					proxy = schemes[scheme];
				} else if (schemes.count("http")) {
					proxy = schemes["http"];
				} else {
					proxy = "";
				}
			}

			if (config.lpszProxy) {
				GlobalFree((HGLOBAL)config.lpszProxy);
			}

			if (config.lpszProxyBypass) {
				GlobalFree((HGLOBAL)config.lpszProxyBypass);
			}

			if (config.lpszAutoConfigUrl) {
				GlobalFree((HGLOBAL)config.lpszAutoConfigUrl);
			}
		}
	}

	if (proxy.size()) {
		curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
		curl_easy_setopt(curl, CURLOPT_PROXY, proxy.c_str());
	}
}
#endif












/* ========================================================= */




#ifdef WIN32
#include <shlwapi.h>
#pragma comment(lib, "Shlwapi.lib")
#endif





/* ========================================================= */

#ifndef WIN32
#include <uuid/uuid.h>
#endif


#ifdef WIN32
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")
#endif

#ifdef WIN32
#include <wbemidl.h>
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "OleAut32.lib")
#pragma comment(lib, "Advapi32.lib")
#endif


std::string CreateSessionSignedAbsolutePathURL(std::wstring path)
{
	// static, because constructing a std::wregex compiles the pattern and
	// this is called once per asset. getAllRazerWyvrnEvents signs ~23,700
	// URLs in one call; at two fresh regexes each that was ~47,000 pattern
	// compilations and the dominant cost of the whole request.
	static const std::wregex kHash(L"#");
	static const std::wregex kAmp(L"&");

	path = std::regex_replace(path, kHash, L"%23");
	path = std::regex_replace(path, kAmp, L"%26");

	QUrl parts(SEGlobalStateManager::GetInstance()
			   ->GetLocalFilesystemHttpServer()
			   ->GetBaseUrl()
			   .c_str());

	parts.setPath(
		QString::fromStdWString((std::wstring(L"/") + path).c_str()));

	std::string message = parts.path().toStdString();

	message = message.erase(0, 1);

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return parts.url().toStdString() + std::string("?digest=") +
	       CreateSessionMessageSignature(message);
}

bool VerifySessionSignedAbsolutePathURL(std::string url, std::string &path)
{
	QUrl parts(url.c_str());

	path = parts.path().toStdString();

	path = path.erase(0, 1);

	if (!parts.hasQuery())
		return false;

	QUrlQuery query(parts.query());

	if (!query.hasQueryItem("digest"))
		return false;

	std::string signature = query.queryItemValue("digest").toStdString();

	std::string message = path;

	//message = std::regex_replace(message, std::regex("#"), "%23");
	//message = std::regex_replace(message, std::regex("&"), "%26");

	return VerifySessionMessageSignature(message, signature);
}

/* ========================================================= */

bool IsAlwaysOnTop(QWidget *window)
{
	if (!window)
		return false;

#ifdef WIN32
	DWORD exStyle = GetWindowLong((HWND)window->winId(), GWL_EXSTYLE);
	return (exStyle & WS_EX_TOPMOST) != 0;
#else
	return (window->windowFlags() & Qt::WindowStaysOnTopHint) != 0;
#endif
}

void SetAlwaysOnTop(QWidget *window, bool enable)
{
	if (!window)
		return;

#ifdef WIN32
	HWND hwnd = (HWND)window->winId();
	SetWindowPos(hwnd, enable ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
		     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
	Qt::WindowFlags flags = window->windowFlags();

	if (enable)
		flags |= Qt::WindowStaysOnTopHint;
	else
		flags &= ~Qt::WindowStaysOnTopHint;

	window->setWindowFlags(flags);
	window->show();
#endif
}


void AdviseHostUserInterfaceStateChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {
		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			os_atomic_set_bool(&isBusy, false);

			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostUserInterfaceStateChanged",
					      "null");
		});
	}
}

void AdviseHostHotkeyBindingsChanged()
{
	static bool isBusy = false;

	if (!os_atomic_set_bool(&isBusy, true)) {

		QTimer::singleShot(std::chrono::milliseconds(250), qApp, [&]() {
			// Advise guest code of user interface state changes
			DispatchJSEventGlobal("hostHotkeyBindingsChanged",
					      "null");

			os_atomic_set_bool(&isBusy, false);
		});
	}
}

bool ParseStreamElementsOverlayURL(std::string url, std::string &overlayId,
				   std::string &accountId)
{
	std::regex url_regex(
		"^https://streamelements.com/overlay/([^/]+)/([^/]+)$");
	std::smatch match;

	if (std::regex_match(url, match, url_regex)) {
		overlayId = match[1].str();
		accountId = match[2].str();

		return true;
	}

	return false;
}

std::string GetStreamElementsOverlayEditorURL(std::string overlayId,
					      std::string accountId)
{
	return std::string("https://streamelements.com/overlay/") + overlayId +
	       std::string("/editor");
}







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

bool DeserializeDocksMenu(QMenu &menu)
{
	auto widgetManager = SEGlobalStateManager::GetInstance()
				     ->GetWidgetManager();

	if (!widgetManager)
		return true;

	widgetManager->EnterCriticalSection();

	std::vector<std::string> widgetIds;

	widgetManager->GetDockBrowserWidgetIdentifiers(widgetIds);

	std::vector<SEBrowserWidgetManager::DockBrowserWidgetInfo *>
		widgets;

	if (widgetManager) {
		for (auto id : widgetIds) {
			auto info = widgetManager->GetDockBrowserWidgetInfo(
				id.c_str());

			if (info) {
				widgets.push_back(info);
			}
		}
	}

	std::sort(
		widgets.begin(), widgets.end(),
		[](SEBrowserWidgetManager::DockBrowserWidgetInfo *a,
		   SEBrowserWidgetManager::DockBrowserWidgetInfo
			   *b) { return a->m_title < b->m_title; });

	for (auto widget : widgets) {
		// widget->m_visible
		QAction *widget_action =
			new QAction(QString(widget->m_title.c_str()));
		menu.addAction(widget_action);

		std::string id = widget->m_id;
		bool isVisible = widget->m_visible;

		widget_action->setCheckable(true);
		widget_action->setChecked(isVisible);

		QObject::connect(widget_action, &QAction::triggered, [id, isVisible, widget_action] {
			auto widgetManager =
				SEGlobalStateManager::GetInstance()
					->GetWidgetManager();

			if (!widgetManager)
				return;

			widgetManager->EnterCriticalSection();

			QDockWidget *dock =
				widgetManager->GetDockWidget(id.c_str());

			if (dock) {
				if (isVisible) {
					// Hide
					SEGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackEvent(
							"se_live_dock_hide_click",
							json11::Json::object{
								{"type",
								 "button_click"},
								{"placement",
								 "menu"}},
							json11::Json::array{json11::Json::array{
								"dock_widget_title",
								dock->windowTitle()
									.toStdString()}});
				} else {
					// Show
					SEGlobalStateManager::GetInstance()
						->GetAnalyticsEventsManager()
						->trackEvent(
							"se_live_dock_show_click",
							json11::Json::object{
								{"type",
								 "button_click"},
								{"placement",
								 "menu"}},
							json11::Json::array{json11::Json::array{
								"dock_widget_title",
								dock->windowTitle()
									.toStdString()}});
				}

				dock->setVisible(!isVisible);

				SEGlobalStateManager::GetInstance()
					->GetMenuManager()
					->Update();
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

bool DeserializeMenu(CefRefPtr<CefValue> input, QMenu &menu,
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
				[action, defaultAction, defaultContextMenu]() {
					DeserializeAndInvokeAction(
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

QWidget *
DeserializeAuxiliaryControlWidget(CefRefPtr<CefValue> input,
				  std::function<void()> defaultAction,
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

		auto HandleClick = [items, control](bool /*checked*/) {
			QMenu menu;

			if (!DeserializeMenu(items, menu))
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

		auto HandleClick = [action, defaultAction,
				    defaultContextMenu](bool /*checked*/) {
			DeserializeAndInvokeAction(action, defaultAction,
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

QWidget *DeserializeRemoteIconWidget(CefRefPtr<CefValue> input,
				     QPixmap *defaultPixmap)
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

bool DeserializeAndInvokeAction(CefRefPtr<CefValue> input,
				std::function<void()> defaultAction,
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
		if (!SEGlobalStateManager::IsInstanceAvailable())
			return false;

		return SEApiMessageHandler::InvokeHandler::GetInstance(
			       SEGlobalStateManager::GetInstance()
				       ->GetApiServices())
			->InvokeApiCallAsync(invoke, invokeArgs,
					     [](CefRefPtr<CefValue>) {});
	}
}










/* ========================================================= */

static bool GetBool(CefRefPtr<CefDictionaryValue> input, std::string key,
		    bool defaultValue = false)
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_BOOL)
		return defaultValue;

	return input->GetBool(key);
}

static int GetInt(CefRefPtr<CefDictionaryValue> input, std::string key,
		  int defaultValue = 0)
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_INT)
		return defaultValue;

	return input->GetInt(key);
}

static std::string GetString(CefRefPtr<CefDictionaryValue> input,
			     std::string key, std::string defaultValue = "")
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_STRING)
		return defaultValue;

	return input->GetString(key).ToString();
}

static std::wstring GetWString(CefRefPtr<CefDictionaryValue> input,
			       std::string key, std::wstring defaultValue = L"")
{
	if (!input->HasKey(key) || input->GetType(key) != VTYPE_STRING)
		return defaultValue;

	return input->GetString(key).ToWString();
}


#ifdef WIN32
#endif


void DispatchClientMessage(std::string target, CefRefPtr<CefProcessMessage> msg)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetClientMessage("system", target, msg);
}

void DispatchJSEventGlobal(std::string event, std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchJSEvent("system", event, eventArgsJson);
}

void DispatchJSEventContainer(std::string target, std::string event,
			      std::string eventArgsJson)
{
	if (!SEGlobalStateManager::IsInstanceAvailable())
		return;

	auto apiServer = SEGlobalStateManager::GetInstance()
				 ->GetWebsocketApiServer();

	if (!apiServer)
		return;

	apiServer->DispatchTargetJSEvent("system", target, event,
					 eventArgsJson);
}

















void DeserializeRevealFileInGraphicalShell(CefRefPtr<CefValue> input,
					   CefRefPtr<CefValue> &output)
{
	output->SetBool(false);

	if (!input.get() || input->GetType() != VTYPE_STRING)
		return;

	const QString pathIn = input->GetString().ToString().c_str();

	const QFileInfo fileInfo(pathIn);

	if (!fileInfo.exists())
		return;

#ifdef WIN32
	QString explorer = QStandardPaths::findExecutable(
		QString::fromLatin1("explorer.exe"));

	if (explorer.isEmpty()) {
		return;
	}

	QStringList param;
	if (!fileInfo.isDir())
		param += QLatin1String("/select,");
	param += QDir::toNativeSeparators(fileInfo.canonicalFilePath());

	output->SetBool(QProcess::startDetached(explorer, param));
#endif

#ifdef APPLE
	QStringList scriptArgs;
	scriptArgs
		<< QLatin1String("-e")
		<< QString::fromLatin1(
			   "tell application \"Finder\" to reveal POSIX file \"%1\"")
			   .arg(fileInfo.canonicalFilePath());
	QProcess::execute(QLatin1String("/usr/bin/osascript"), scriptArgs);
	scriptArgs.clear();
	scriptArgs << QLatin1String("-e")
		   << QLatin1String("tell application \"Finder\" to activate");
	output->SetBool(QProcess::execute(QLatin1String("/usr/bin/osascript"),
					  scriptArgs) == 0);
#endif
}
