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
	// The macOS branch below used to be guarded on `#ifdef APPLE`, which
	// is a CMake variable and not a compiler define -- nothing in
	// CMakeLists.txt passes it, so the branch had never been compiled and
	// openFileLocationInHostFileManager answered false on macOS without
	// doing anything. It is `__APPLE__` now, which is the commit of its
	// own that the previous note here asked for.
	//
	// So this is a behaviour change on macOS: the method now launches
	// osascript and reveals the file, where before it was a silent
	// no-op. The Windows branch is untouched.
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

#ifdef __APPLE__
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
