#ifndef STREAMELEMENTSPLEASEWAITWINDOW_H
#define STREAMELEMENTSPLEASEWAITWINDOW_H

#include <QWidget>
#include <QDialog>

#include <mutex>

namespace Ui {
class StreamElementsPleaseWaitWindow;
}

class StreamElementsPleaseWaitWindow : public QDialog {
    Q_OBJECT

public:
	static StreamElementsPleaseWaitWindow *GetInstance();

	void Show();
	void Hide();

protected:
	explicit StreamElementsPleaseWaitWindow(QWidget *parent = 0);
	~StreamElementsPleaseWaitWindow();

private:
	Ui::StreamElementsPleaseWaitWindow *ui;

	long m_showCount = 0;

private:
	static StreamElementsPleaseWaitWindow *s_instance;
};

#endif // STREAMELEMENTSPLEASEWAITWINDOW_H
