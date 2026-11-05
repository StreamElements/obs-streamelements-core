#include "SEAppEnvUtils.hpp"

//
// GetCommandLineOptionValue is declared there; its body is below,
// because reading the list needs QCoreApplication.
//
#include "SEEnvConfigUtils.hpp"
#include "SEStringUtils.hpp"

#include "../SEConfig.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <util/config-file.h>

#include <QApplication>
#include <QUrl>
#include <QCoreApplication>
#include <QDockWidget>
#include <QFile>
#include <QStringList>
#include <QTextStream>

static config_t *obs_fe_user_config()
{
	//
	// Straight from libobs. This used to go through SEConfig, which cached
	// the same call in its constructor and handed the cache back -- a
	// dependency on the plug-in's own config object for something that was
	// never ours (CORE-757).
	//
	{
		config_t *config = obs_frontend_get_user_config();

		if (config)
			return config;
	}

	return nullptr;
}
bool IsTraceLogLevel()
{
	//
	// The answer is argv, which cannot change, so it is computed once.
	//
	// It has to be computed once *safely*: this is called from every
	// thread that logs -- the API dispatcher reads it three times per
	// call -- and the pair of plain bools this replaced did an unsynchro-
	// nised read-then-write of both of them, which is a data race whatever
	// the values happen to settle on. A function-local static with a
	// dynamic initializer is initialized exactly once, with every other
	// thread blocking until it is, and the compiler emits the guard.
	//
	// The flag it replaced also appeared in the loop condition
	// (`i < args.size() && !has_result`), where it could never be true,
	// because it was assigned below the loop rather than inside it. The
	// loop ended on its own `break`, so the memo was correct and argv was
	// walked once -- but the function read as though it were not, which is
	// its own reason to write it the short way.
	//
	// Nothing reaches this before OBS has built its QCoreApplication: the
	// plug-in is loaded by it. The null check is for a future caller that
	// does, and it deliberately answers without caching -- a `false`
	// memoized before argv was reachable would be wrong for the rest of
	// the process, which is a worse failure than asking twice.
	//
	if (!QCoreApplication::instance())
		return false;

	static const bool result = []() -> bool {
		for (const QString &arg : QCoreApplication::arguments()) {
			if (arg.startsWith(QStringLiteral("--setrace")))
				return true;
		}

		return false;
	}();

	return result;
}
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
