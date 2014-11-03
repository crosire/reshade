/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created: Wed Mar 20 10:59:26 2013
**      by: Qt User Interface Compiler version 4.6.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QLabel *wordLabel;
    QLabel *labelAbout;
    QPushButton *buttonInfo;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(400, 300);
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        wordLabel = new QLabel(centralWidget);
        wordLabel->setObjectName(QString::fromUtf8("wordLabel"));
        wordLabel->setGeometry(QRect(260, 70, 58, 15));
        QSizePolicy sizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(wordLabel->sizePolicy().hasHeightForWidth());
        wordLabel->setSizePolicy(sizePolicy);
        QFont font;
        font.setFamily(QString::fromUtf8("Abyssinica SIL"));
        font.setPointSize(28);
        wordLabel->setFont(font);
        wordLabel->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignTop);
        labelAbout = new QLabel(centralWidget);
        labelAbout->setObjectName(QString::fromUtf8("labelAbout"));
        labelAbout->setGeometry(QRect(290, 210, 58, 15));
        QFont font1;
        font1.setFamily(QString::fromUtf8("Abyssinica SIL"));
        font1.setPointSize(12);
        labelAbout->setFont(font1);
        buttonInfo = new QPushButton(centralWidget);
        buttonInfo->setObjectName(QString::fromUtf8("buttonInfo"));
        buttonInfo->setGeometry(QRect(190, 250, 87, 27));
        MainWindow->setCentralWidget(centralWidget);

        retranslateUi(MainWindow);

        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QApplication::translate("MainWindow", "MainWindow"));
        wordLabel->setText(QApplication::translate("MainWindow", "Word"));
        labelAbout->setText(QApplication::translate("MainWindow", "About This sample"));
        buttonInfo->setText(QApplication::translate("MainWindow", "Info"));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
