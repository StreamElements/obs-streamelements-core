#pragma once

#include <QDialog>

namespace Ui {
class ConfirmPendingUpdateDialog;
}

///
// Updata available dialog
//
// Prompt the user to update obs-browser-SE
//
class ConfirmPendingUpdateDialog : public QDialog
{
	Q_OBJECT

public:
	explicit ConfirmPendingUpdateDialog(QWidget *parent = 0);
	~ConfirmPendingUpdateDialog();

public:
	///
	// Is "don't ask again" checkbox checked?
	//
	// @return	true if checked, otherwise false
	//
	bool IsSkipVersionClicked();

	///
	// Set "release notes" text
	//
	// @param release_notes	- release notes text
	//
	void SetReleaseNotes(std::string release_notes);

public slots:
	///
	// We need this instead of QDialog::exec() to make certain that
	// the dialog is set to non-modal when hidden.
	// This is necessary due to an issue in OBS Studio main code which
	// prevents OBS minimizing to system tray icon if a modal dialog
	// (even hidden) exists in the widget tree.
	//
	int ExecDialog();

private slots:
	///
	// User clicked "update"
	//
	void on_ctl_acceptButton_clicked()
	{
		accept();
	}

	///
	// User clicked "not now"
	//
	void on_ctl_rejectButton_clicked()
	{
		reject();
	}

private:
	Ui::ConfirmPendingUpdateDialog *ui;
	bool m_skipVersionClicked = false;
};
