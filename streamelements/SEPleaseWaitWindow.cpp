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

//
// Both of these post to the Qt thread, and the window can be gone by the time
// the task runs. Create() says so in as many words -- two owners, either of
// which may get there first -- and it protects the DELETER with a QPointer
// while these two captured a raw `this`.
//
// What that costs: open() and hide() both reach QWidget::setVisible, which is
// virtual. A queued call landing on a destroyed QDialog dispatches through a
// vtable that destruction has already rewound, and the CRT calls _purecall ->
// abort. That is the exact shape of SELIVE-3J, whose innermost real frames are
// QMetaCallEvent::placeMetaCall -> the lambda -> QWidget::setVisible ->
// QLayout::activate -> _purecall.
//
// A QPointer is zeroed by Qt the moment the object is destroyed, so the task
// becomes a no-op instead of a virtual call on a corpse. Same idiom Create()
// already uses, now applied on the path that actually dispatches.
//
void SEPleaseWaitWindow::Show()
{
	if (os_atomic_inc_long(&m_showCount) == 1) {
		QPointer<SEPleaseWaitWindow> self(this);

		QtPostTask([self]() {
			if (self)
				self->open();
		});
	}
}

void SEPleaseWaitWindow::Hide()
{
	if (os_atomic_dec_long(&m_showCount) == 0) {
		QPointer<SEPleaseWaitWindow> self(this);

		QtPostTask([self]() {
			if (self)
				self->hide();
		});
	}
}
