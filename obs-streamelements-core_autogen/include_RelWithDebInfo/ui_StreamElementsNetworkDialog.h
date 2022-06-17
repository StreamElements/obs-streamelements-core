/********************************************************************************
** Form generated from reading UI file 'StreamElementsNetworkDialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STREAMELEMENTSNETWORKDIALOG_H
#define UI_STREAMELEMENTSNETWORKDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_StreamElementsNetworkDialog
{
public:
    QWidget *verticalLayoutWidget;
    QVBoxLayout *verticalLayout;
    QLabel *ctl_message;
    QProgressBar *ctl_progressBar;
    QGridLayout *gridLayout;
    QPushButton *ctl_cancelButton;

    void setupUi(QDialog *StreamElementsNetworkDialog)
    {
        if (StreamElementsNetworkDialog->objectName().isEmpty())
            StreamElementsNetworkDialog->setObjectName(QString::fromUtf8("StreamElementsNetworkDialog"));
        StreamElementsNetworkDialog->resize(640, 120);
        StreamElementsNetworkDialog->setMinimumSize(QSize(640, 120));
        StreamElementsNetworkDialog->setMaximumSize(QSize(640, 120));
        StreamElementsNetworkDialog->setSizeGripEnabled(false);
        StreamElementsNetworkDialog->setModal(true);
        verticalLayoutWidget = new QWidget(StreamElementsNetworkDialog);
        verticalLayoutWidget->setObjectName(QString::fromUtf8("verticalLayoutWidget"));
        verticalLayoutWidget->setGeometry(QRect(-1, -1, 641, 131));
        verticalLayout = new QVBoxLayout(verticalLayoutWidget);
        verticalLayout->setSpacing(6);
        verticalLayout->setContentsMargins(11, 11, 11, 11);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setSizeConstraint(QLayout::SetNoConstraint);
        verticalLayout->setContentsMargins(20, 20, 20, 20);
        ctl_message = new QLabel(verticalLayoutWidget);
        ctl_message->setObjectName(QString::fromUtf8("ctl_message"));
        QFont font;
        font.setPointSize(8);
        font.setBold(false);
        font.setWeight(50);
        ctl_message->setFont(font);
        ctl_message->setAlignment(Qt::AlignCenter);
        ctl_message->setWordWrap(true);

        verticalLayout->addWidget(ctl_message);

        ctl_progressBar = new QProgressBar(verticalLayoutWidget);
        ctl_progressBar->setObjectName(QString::fromUtf8("ctl_progressBar"));
        ctl_progressBar->setValue(24);
        ctl_progressBar->setAlignment(Qt::AlignCenter);

        verticalLayout->addWidget(ctl_progressBar);

        gridLayout = new QGridLayout();
        gridLayout->setSpacing(6);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        ctl_cancelButton = new QPushButton(verticalLayoutWidget);
        ctl_cancelButton->setObjectName(QString::fromUtf8("ctl_cancelButton"));
        QSizePolicy sizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(ctl_cancelButton->sizePolicy().hasHeightForWidth());
        ctl_cancelButton->setSizePolicy(sizePolicy);

        gridLayout->addWidget(ctl_cancelButton, 0, 0, 1, 1);


        verticalLayout->addLayout(gridLayout);


        retranslateUi(StreamElementsNetworkDialog);

        QMetaObject::connectSlotsByName(StreamElementsNetworkDialog);
    } // setupUi

    void retranslateUi(QDialog *StreamElementsNetworkDialog)
    {
        StreamElementsNetworkDialog->setWindowTitle(QCoreApplication::translate("StreamElementsNetworkDialog", "StreamElements.NetworkDialog.Title", nullptr));
        ctl_message->setText(QCoreApplication::translate("StreamElementsNetworkDialog", "StreamElements.NetworkDialog.Message.Default", nullptr));
        ctl_cancelButton->setText(QCoreApplication::translate("StreamElementsNetworkDialog", "StreamElements.NetworkDialog.Action.Cancel", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StreamElementsNetworkDialog: public Ui_StreamElementsNetworkDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STREAMELEMENTSNETWORKDIALOG_H
