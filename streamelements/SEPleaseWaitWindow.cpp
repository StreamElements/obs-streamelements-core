#include "SEPleaseWaitWindow.hpp"
#include "ui_SEPleaseWaitWindow.h"

#include "SEUtils.hpp"

#include <QMainWindow>

#include <obs-frontend-api.h>
#include <util/threading.h>

#include <mutex>

SEPleaseWaitWindow::SEPleaseWaitWindow(QWidget *parent) : QDialog(parent),
    ui(new Ui::SEPleaseWaitWindow)
{
    ui->setupUi(this);

    // Remove title bar
    setWindowFlags(Qt::CustomizeWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
}

SEPleaseWaitWindow::~SEPleaseWaitWindow()
{
    delete ui;
}

std::shared_ptr<SEPleaseWaitWindow>
SEPleaseWaitWindow::Create(QWidget *parent)
{
	//
	// Two owners, and only one of them may delete: Qt owns it through
	// the parent, and the services bundle holds this shared_ptr. The
	// QPointer is how the deleter tells which got there first -- Qt
	// zeroes it when the main window takes the widget down, and then
	// there is nothing for us to free.
	//
	QPointer<SEPleaseWaitWindow> tracked = new SEPleaseWaitWindow(parent);

	return std::shared_ptr<SEPleaseWaitWindow>(
		tracked.data(), [tracked](SEPleaseWaitWindow *) {
			if (tracked)
				tracked->deleteLater();
		});
}

void SEPleaseWaitWindow::Show()
{
	if (os_atomic_inc_long(&m_showCount) == 1) {
		QtPostTask([this]() { this->open(); });
	}
}

void SEPleaseWaitWindow::Hide()
{
	if (os_atomic_dec_long(&m_showCount) == 0) {
		QtPostTask([this]() { this->hide(); });
	}
}
