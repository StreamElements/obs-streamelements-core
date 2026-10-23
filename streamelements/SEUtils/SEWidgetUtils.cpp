#include "SEWidgetUtils.hpp"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QString>
#include <QStringList>

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
