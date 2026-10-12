#ifndef STREAMELEMENTSPLEASEWAITWINDOW_H
#define STREAMELEMENTSPLEASEWAITWINDOW_H

#include <QWidget>
#include <QDialog>

#include <mutex>

namespace Ui {
class SEPleaseWaitWindow;
}

class SEPleaseWaitWindow : public QDialog {
    Q_OBJECT

public:
	static SEPleaseWaitWindow *GetInstance();

	void Show();
	void Hide();

protected:
	explicit SEPleaseWaitWindow(QWidget *parent = 0);
	~SEPleaseWaitWindow();

private:
	Ui::SEPleaseWaitWindow *ui;

	long m_showCount = 0;

private:
	static SEPleaseWaitWindow *s_instance;
};

#endif // STREAMELEMENTSPLEASEWAITWINDOW_H
