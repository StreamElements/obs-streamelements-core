#include "SEPleaseWaitWindow.hpp"
#include "ui_SEPleaseWaitWindow.h"

#include "SEUtils.hpp"

#include <QMainWindow>

#include <obs-frontend-api.h>
#include <util/threading.h>

#include <mutex>

SEPleaseWaitWindow *SEPleaseWaitWindow::s_instance = nullptr;

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

SEPleaseWaitWindow* SEPleaseWaitWindow::GetInstance()
{
	static std::mutex s_mutex;

	if (!s_instance) {
		std::lock_guard<std::mutex> guard(s_mutex);

		if (!s_instance) {
			QMainWindow *mainWindow =
				(QMainWindow *)obs_frontend_get_main_window();

			s_instance = new SEPleaseWaitWindow(mainWindow);
		}
	}

	return s_instance;
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
