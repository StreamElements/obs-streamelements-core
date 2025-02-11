#include "confirmpendingupdatedialog.hpp"
#include "ui_confirmpendingupdatedialog.h"
#include "../StreamElementsUtils.hpp"
#include <obs-frontend-api.h>

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

int ConfirmPendingUpdateDialog::ExecDialog()
{
	int result = 0;

	m_skipVersionClicked = false;

	QtExecSync([&]() {
		setModal(true);
		result = exec();
		setModal(false);
	});

	return result;
}
