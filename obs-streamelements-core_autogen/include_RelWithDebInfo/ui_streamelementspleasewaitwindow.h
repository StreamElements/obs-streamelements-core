/********************************************************************************
** Form generated from reading UI file 'StreamElementsPleaseWaitWindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_STREAMELEMENTSPLEASEWAITWINDOW_H
#define UI_STREAMELEMENTSPLEASEWAITWINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QLabel>

QT_BEGIN_NAMESPACE

class Ui_StreamElementsPleaseWaitWindow
{
public:
    QLabel *label;

    void setupUi(QDialog *StreamElementsPleaseWaitWindow)
    {
        if (StreamElementsPleaseWaitWindow->objectName().isEmpty())
            StreamElementsPleaseWaitWindow->setObjectName(QString::fromUtf8("StreamElementsPleaseWaitWindow"));
        StreamElementsPleaseWaitWindow->resize(415, 143);
        label = new QLabel(StreamElementsPleaseWaitWindow);
        label->setObjectName(QString::fromUtf8("label"));
        label->setGeometry(QRect(0, 0, 415, 143));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(label->sizePolicy().hasHeightForWidth());
        label->setSizePolicy(sizePolicy);
        label->setMinimumSize(QSize(200, 100));
        QFont font;
        font.setPointSize(19);
        font.setBold(false);
        font.setWeight(50);
        font.setKerning(true);
        label->setFont(font);
        label->setCursor(QCursor(Qt::WaitCursor));
        label->setContextMenuPolicy(Qt::NoContextMenu);
        label->setAlignment(Qt::AlignCenter);

        retranslateUi(StreamElementsPleaseWaitWindow);

        QMetaObject::connectSlotsByName(StreamElementsPleaseWaitWindow);
    } // setupUi

    void retranslateUi(QDialog *StreamElementsPleaseWaitWindow)
    {
        StreamElementsPleaseWaitWindow->setWindowTitle(QCoreApplication::translate("StreamElementsPleaseWaitWindow", "Form", nullptr));
        label->setText(QCoreApplication::translate("StreamElementsPleaseWaitWindow", "Please wait...", nullptr));
    } // retranslateUi

};

namespace Ui {
    class StreamElementsPleaseWaitWindow: public Ui_StreamElementsPleaseWaitWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_STREAMELEMENTSPLEASEWAITWINDOW_H
