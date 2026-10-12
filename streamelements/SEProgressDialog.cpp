#include "SEProgressDialog.hpp"
#include "ui_SEProgressDialog.h"

SEProgressDialog::SEProgressDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SEProgressDialog)
{
    ui->setupUi(this);
}

SEProgressDialog::~SEProgressDialog()
{
    delete ui;
}

void SEProgressDialog::accept()
{
	QDialog::accept();
}

void SEProgressDialog::reject()
{
	m_cancelled = true;

	QDialog::reject();
}

void SEProgressDialog::setEnableCancel(bool enable)
{
	QMetaObject::invokeMethod(ui->cmdCancel, "setEnabled", Qt::QueuedConnection, Q_ARG(bool, enable));
}

void SEProgressDialog::setMessage(std::string msg)
{
	QMetaObject::invokeMethod(ui->txtOp, "setText", Qt::QueuedConnection, Q_ARG(QString, QString(msg.c_str())));
}

void SEProgressDialog::setProgress(int min, int max, int value)
{
	if (ui->progressBar->minimum() != min) {
		QMetaObject::invokeMethod(ui->progressBar, "setMinimum", Qt::QueuedConnection, Q_ARG(int, min));
	}

	if (ui->progressBar->maximum() != max) {
		QMetaObject::invokeMethod(ui->progressBar, "setMaximum", Qt::QueuedConnection, Q_ARG(int, max));
	}

	if (ui->progressBar->value() != value) {
		QMetaObject::invokeMethod(ui->progressBar, "setValue", Qt::QueuedConnection, Q_ARG(int, value));
	}
}
