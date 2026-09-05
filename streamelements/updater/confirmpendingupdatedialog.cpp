#include "confirmpendingupdatedialog.hpp"
#include "ui_confirmpendingupdatedialog.h"
#include "../StreamElementsUtils.hpp"
#include <obs-frontend-api.h>

#include <QEvent>
#include <QTimer>

ConfirmPendingUpdateDialog::ConfirmPendingUpdateDialog(QWidget *parent)
	: QDialog(parent), ui(new Ui::ConfirmPendingUpdateDialog)
{
	ui->setupUi(this);

	// Disable close button and context help button
	this->setWindowFlags(((windowFlags() | Qt::CustomizeWindowHint) &
			      ~Qt::WindowContextHelpButtonHint));

	//QPixmap pixmapTarget = QPixmap(":/images/logo.png");
	//pixmapTarget = pixmapTarget.scaled(128, 128, Qt::KeepAspectRatio, Qt::SmoothTransformation);
	//ui->ctl_imageContainer->setPixmap(pixmapTarget);

	// setStyleSheet("background-color: #eeeeee; color: #000000;");

	if (obs_frontend_is_theme_dark()) {
		QPixmap pixmapTarget =
			QPixmap(":/images/updater_logo_dark.png");

		ui->ctl_imageContainer->setPixmap(pixmapTarget);
	}

	setModal(false);

	QObject::connect(ui->ctl_rejectSkipVersionButton, &QPushButton::clicked,
			 [this]() {
				 m_skipVersionClicked = true;

				 reject();
			 });
}

ConfirmPendingUpdateDialog::~ConfirmPendingUpdateDialog()
{
	delete ui;
}

bool ConfirmPendingUpdateDialog::IsSkipVersionClicked()
{
	return m_skipVersionClicked;
}

void ConfirmPendingUpdateDialog::SetReleaseNotes(std::string release_notes)
{
	QString qReleaseNotesString(release_notes.c_str());

	// ui->releaseNotes->setStyleSheet("background-color: white; color: black");

	QMetaObject::invokeMethod(ui->releaseNotes, "setText",
				  Qt::QueuedConnection,
				  Q_ARG(QString, qReleaseNotesString));
}

bool ConfirmPendingUpdateDialog::eventFilter(QObject *watched, QEvent *event)
{
	if (event->type() != QEvent::Close || watched != parentWidget() ||
	    !m_inExecDialog)
		return QDialog::eventFilter(watched, event);

	//
	// OBS is being closed while this prompt is up, and the close must not
	// be delivered from in here.
	//
	// exec() below is running a nested event loop, and this event is being
	// dispatched by it -- so OBS's entire shutdown would run inside that
	// loop. It cannot finish there: finishing means joining the updater's
	// worker thread, and that thread is blocked waiting for this exec() to
	// return. Both halves of that standoff were captured in one live dump
	// (CORE-1193), with the whole of StreamElementsGlobalStateManager::
	// Shutdown() sitting on the stack between QDialog::exec and
	// os_event_wait.
	//
	// So answer the prompt on the user's behalf -- as "not now", which is
	// the safe answer, and without recording a skipped version -- swallow
	// this close, and ask for it again once the stack has unwound back to
	// the main event loop. ExecDialog() below does the asking, because the
	// re-post has to happen after exec() has returned -- see the comment
	// there. By then the worker has been released and shutdown runs with
	// nothing left to wait for.
	//
	m_closeRequestedDuringExec = true;

	// Only the first close of a burst has a dialog left to answer. Every
	// one of them still has to be swallowed: exec() does not unwind
	// instantly, and a later close let through in that gap reaches OBS
	// from inside this loop, which is the whole problem.
	if (isVisible())
		reject();

	event->ignore();

	return true;
}

int ConfirmPendingUpdateDialog::ExecDialog()
{
	int result = 0;

	m_skipVersionClicked = false;

	QtExecSync([&]() {
		// Installed only for the life of the modal loop. Outside it
		// there is no nested loop to protect against, and a filter left
		// on the main window would swallow a close it has no business
		// touching.
		QWidget *mainWindow = parentWidget();

		if (mainWindow)
			mainWindow->installEventFilter(this);

		m_inExecDialog = true;

		setModal(true);
		result = exec();
		setModal(false);

		m_inExecDialog = false;

		if (mainWindow)
			mainWindow->removeEventFilter(this);

		if (m_closeRequestedDuringExec) {
			m_closeRequestedDuringExec = false;

			// Posted only now, once exec() has returned.
			//
			// A timer started from inside the filter is serviced
			// by exec()'s own event loop, which at that point is
			// still unwinding -- so the close comes straight back
			// while this call is on the stack, and with the dialog
			// now hidden the filter waves it through into the same
			// deadlock. Measured, not theorised. Posting here puts
			// it on the main loop instead, which is not reached
			// until this task has finished and the updater's
			// worker has been released.
			QTimer::singleShot(0, mainWindow, [mainWindow]() {
				mainWindow->close();
			});
		}
	});

	return result;
}
