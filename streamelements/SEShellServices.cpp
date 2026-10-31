#include "SEShellServices.hpp"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QUrl>

SEShellServices::SEShellServices() {}

SEShellServices::~SEShellServices() {}

bool SEShellServices::OpenDefaultBrowser(std::string url)
{
	QDesktopServices::openUrl(QUrl(url.c_str(), QUrl::TolerantMode));

	return true;
}

void SEShellServices::RevealFileInGraphicalShell(CefRefPtr<CefValue> input,
						 CefRefPtr<CefValue> &output)
{
	//
	// Moved verbatim from SEUtils/SEWidgetUtils.cpp, which had nothing
	// else in it.
	//
	// Including the macOS branch's guard, which is `#ifdef APPLE` and not
	// `#ifdef __APPLE__`. APPLE is a CMake variable, not a compiler
	// define, and nothing in CMakeLists.txt passes it -- so that branch
	// has never been compiled and openFileLocationInHostFileManager
	// answers false on macOS without doing anything. Left as it is on
	// purpose: correcting it turns a no-op into a process launch on a
	// platform this commit cannot test, which wants a commit and a build
	// of its own. Two other sites spell it the same way,
	// SEBrowserWidget.hpp and updater/updater.cpp.
	//
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
