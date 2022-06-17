/********************************************************************************
** Form generated from reading UI file 'StreamElementsProgressDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STREAMELEMENTSPROGRESSDIALOG_H
#define UI_STREAMELEMENTSPROGRESSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_StreamElementsProgressDialog
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QSpacerItem *verticalSpacer_2;
    QLabel *txtOp;
    QProgressBar *progressBar;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer;
    QPushButton *cmdCancel;
    QSpacerItem *horizontalSpacer_2;
    QSpacerItem *verticalSpacer;

    void setupUi(QDialog *StreamElementsProgressDialog)
    {
        if (StreamElementsProgressDialog->objectName().isEmpty())
            StreamElementsProgressDialog->setObjectName(QString::fromUtf8("StreamElementsProgressDialog"));
        StreamElementsProgressDialog->setWindowModality(Qt::WindowModal);
        StreamElementsProgressDialog->resize(601, 131);
        StreamElementsProgressDialog->setModal(true);
        verticalLayoutWidget = new QWidget(StreamElementsProgressDialog);
        verticalLayoutWidget->setObjectName(QString::fromUtf8("verticalLayoutWidget"));
        verticalLayoutWidget->setGeometry(QRect(0, 0, 601, 153));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(20, 15, 20, 15);
        verticalSpacer_2 = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_2);

        txtOp = new QLabel(verticalLayoutWidget);
        txtOp->setObjectName(QString::fromUtf8("txtOp"));
        txtOp->setAlignment(Qt::AlignCenter);
        txtOp->setWordWrap(true);

        verticalLayout->addWidget(txtOp);

        progressBar = new QProgressBar(verticalLayoutWidget);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        QFont font;
        font.setStyleStrategy(QFont::PreferDefault);
        progressBar->setFont(font);
        progressBar->setValue(0);
        progressBar->setAlignment(Qt::AlignCenter);
        progressBar->setInvertedAppearance(false);

        verticalLayout->addWidget(progressBar);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        cmdCancel = new QPushButton(verticalLayoutWidget);
        cmdCancel->setObjectName(QString::fromUtf8("cmdCancel"));

        horizontalLayout->addWidget(cmdCancel);

        horizontalSpacer_2 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer);


        retranslateUi(StreamElementsProgressDialog);
        QObject::connect(cmdCancel, SIGNAL(clicked()), StreamElementsProgressDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(StreamElementsProgressDialog);
    } // setupUi

    void retranslateUi(QDialog *StreamElementsProgressDialog)
    {
        StreamElementsProgressDialog->setWindowTitle(QCoreApplication::translate("StreamElementsProgressDialog", "StreamElements.ProgressDialog.Title", nullptr));
        txtOp->setText(QCoreApplication::translate("StreamElementsProgressDialog", "StreamElements.ProgressDialog.Message.Default", nullptr));
        cmdCancel->setText(QCoreApplication::translate("StreamElementsProgressDialog", "StreamElements.ProgressDialog.Action.Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StreamElementsProgressDialog: public Ui_StreamElementsProgressDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STREAMELEMENTSPROGRESSDIALOG_H
