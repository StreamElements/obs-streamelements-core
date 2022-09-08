/********************************************************************************
** Form generated from reading UI file 'StreamElementsReportIssueDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STREAMELEMENTSREPORTISSUEDIALOG_H
#define UI_STREAMELEMENTSREPORTISSUEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>

QT_BEGIN_NAMESPACE

class Ui_StreamElementsReportIssueDialog
{
public:
    QLabel *label;
    QPlainTextEdit *txtIssue;
    QCheckBox *checkCollectLogsAndSettings;
    QPushButton *cmdOK;
    QPushButton *cmdCancel;

    void setupUi(QDialog *StreamElementsReportIssueDialog)
    {
        if (StreamElementsReportIssueDialog->objectName().isEmpty())
            StreamElementsReportIssueDialog->setObjectName(QString::fromUtf8("StreamElementsReportIssueDialog"));
        StreamElementsReportIssueDialog->setWindowModality(Qt::WindowModal);
        StreamElementsReportIssueDialog->resize(651, 480);
        StreamElementsReportIssueDialog->setAutoFillBackground(false);
        StreamElementsReportIssueDialog->setModal(true);
        label = new QLabel(StreamElementsReportIssueDialog);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(30, 20, 591, 41));
        label->setWordWrap(true);
        txtIssue = new QPlainTextEdit(StreamElementsReportIssueDialog);
        txtIssue->setObjectName(QString::fromUtf8("txtIssue"));
        txtIssue->setGeometry(QRect(30, 70, 591, 291));
        checkCollectLogsAndSettings = new QCheckBox(StreamElementsReportIssueDialog);
        checkCollectLogsAndSettings->setObjectName(QString::fromUtf8("checkCollectLogsAndSettings"));
        checkCollectLogsAndSettings->setGeometry(QRect(30, 370, 591, 41));
        checkCollectLogsAndSettings->setChecked(true);
        cmdOK = new QPushButton(StreamElementsReportIssueDialog);
        cmdOK->setObjectName(QString::fromUtf8("cmdOK"));
        cmdOK->setEnabled(false);
        cmdOK->setGeometry(QRect(130, 420, 231, 31));
        cmdCancel = new QPushButton(StreamElementsReportIssueDialog);
        cmdCancel->setObjectName(QString::fromUtf8("cmdCancel"));
        cmdCancel->setGeometry(QRect(380, 420, 141, 31));
        cmdCancel->setAutoDefault(false);
#if QT_CONFIG(shortcut)
        label->setBuddy(txtIssue);
#endif // QT_CONFIG(shortcut)

        retranslateUi(StreamElementsReportIssueDialog);
        QObject::connect(cmdOK, SIGNAL(clicked()), StreamElementsReportIssueDialog, SLOT(accept()));
        QObject::connect(cmdCancel, SIGNAL(clicked()), StreamElementsReportIssueDialog, SLOT(reject()));
        QObject::connect(txtIssue, SIGNAL(textChanged()), StreamElementsReportIssueDialog, SLOT(update()));

        cmdOK->setDefault(true);
        cmdCancel->setDefault(false);


        QMetaObject::connectSlotsByName(StreamElementsReportIssueDialog);
    } // setupUi

    void retranslateUi(QDialog *StreamElementsReportIssueDialog)
    {
        StreamElementsReportIssueDialog->setWindowTitle(QCoreApplication::translate("StreamElementsReportIssueDialog", "StreamElements.ReportIssue.Title", nullptr));
        label->setText(QCoreApplication::translate("StreamElementsReportIssueDialog", "StreamElements.ReportIssue.Issue.Label", nullptr));
        checkCollectLogsAndSettings->setText(QCoreApplication::translate("StreamElementsReportIssueDialog", "StreamElements.ReportIssue.SendLogsAndSettings.Label", nullptr));
        cmdOK->setText(QCoreApplication::translate("StreamElementsReportIssueDialog", "StreamElements.ReportIssue.Action.Send", nullptr));
        cmdCancel->setText(QCoreApplication::translate("StreamElementsReportIssueDialog", "StreamElements.ReportIssue.Action.Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StreamElementsReportIssueDialog: public Ui_StreamElementsReportIssueDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STREAMELEMENTSREPORTISSUEDIALOG_H
