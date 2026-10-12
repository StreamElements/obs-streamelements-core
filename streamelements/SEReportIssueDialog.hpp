#pragma once

#ifndef STREAMELEMENTSREPORTISSUEDIALOG_H
#define STREAMELEMENTSREPORTISSUEDIALOG_H

#include <QDialog>

namespace Ui {
class SEReportIssueDialog;
}

class SEReportIssueDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SEReportIssueDialog(QWidget *parent = 0);
    ~SEReportIssueDialog();

public slots:
	virtual void update();
	virtual void accept();

private:
    Ui::SEReportIssueDialog *ui;
};

#endif // STREAMELEMENTSREPORTISSUEDIALOG_H
