#include "StreamElementsPleaseWaitWindow.hpp"
#include "ui_streamelementspleasewaitwindow.h"

#include "StreamElementsUtils.hpp"

#include <QMainWindow>

#include <obs-frontend-api.h>
#include <util/threading.h>

#include <mutex>

StreamElementsPleaseWaitWindow *StreamElementsPleaseWaitWindow::s_instance = nullptr;

StreamElementsPleaseWaitWindow::StreamElementsPleaseWaitWindow(QWidget *parent) : QDialog(parent),
    ui(new Ui::StreamElementsPleaseWaitWindow)
{
    ui->setupUi(this);

    // Remove title bar
    setWindowFlags(Qt::CustomizeWindowHint);
    setWindowModality(Qt::ApplicationModal);
    setModal(true);
}

StreamElementsPleaseWaitWindow::~StreamElementsPleaseWaitWindow()
{
    delete ui;
}

StreamElementsPleaseWaitWindow* StreamElementsPleaseWaitWindow::GetInstance()
{
	static std::mutex s_mutex;

	if (!s_instance) {
		std::lock_guard<std::mutex> guard(s_mutex);

		if (!s_instance) {
			QMainWindow *mainWindow =
				(QMainWindow *)obs_frontend_get_main_window();

			s_instance = new StreamElementsPleaseWaitWindow(mainWindow);
		}
	}

	return s_instance;
}

void StreamElementsPleaseWaitWindow::Show()
{
	if (os_atomic_inc_long(&m_showCount) == 1) {
		QtPostTask([this]() { this->open(); });
	}
}

void StreamElementsPleaseWaitWindow::Hide()
{
	if (os_atomic_dec_long(&m_showCount) == 0) {
		QtPostTask([this]() { this->hide(); });
	}
}
