#ifndef STREAMELEMENTSPROGRESSDIALOG_HPP
#define STREAMELEMENTSPROGRESSDIALOG_HPP

#include <QDialog>

namespace Ui {
class SEProgressDialog;
}

class SEProgressDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SEProgressDialog(QWidget *parent = 0);
    ~SEProgressDialog();

public:
	virtual void accept();
	virtual void reject();

	bool cancelled() { return m_cancelled; }

public slots:
	void setEnableCancel(bool enable);
	void setMessage(std::string msg);
	void setProgress(int min, int max, int value);

private:
	bool m_cancelled = false;

private:
    Ui::SEProgressDialog *ui;
};

#endif // STREAMELEMENTSPROGRESSDIALOG_HPP
