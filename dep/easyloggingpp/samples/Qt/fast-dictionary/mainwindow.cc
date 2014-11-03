#include "mainwindow.hh"
#include "ui_mainwindow.h"
#include "listwithsearch.hh"
#include "easylogging++.h"

#include <QFile>
#include <QTextStream>
#include <QMessageBox>
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    this->setWindowTitle("Fast Dictionary Sample - Easylogging++");
    list = new ListWithSearch(ListWithSearch::kCaseInsensative, this);
    this->setGeometry(0, 0, 800, 600);
    initializeDictionary("../fast-dictionary/words.txt");
    list->setFocus();
    connect(list, SIGNAL(selectionMade(QString)), this, SLOT(onSelectionMade(QString)));
    ui->labelAbout->setText("Easylogging++ Info\n================\n" + QString::fromStdString(el::VersionInfo::version()));

}

MainWindow::~MainWindow()
{
    delete list;
    delete ui;
}

void MainWindow::resizeEvent(QResizeEvent *)
{
    list->setGeometry(0, 0, 300, height());
    int contentsX = list->geometry().x() + list->geometry().width() + 10;
    ui->wordLabel->setGeometry(contentsX, 0, width() - list->width(), height());
    ui->labelAbout->setGeometry(contentsX, height() - 150 - 10, width(), 200);
    ui->buttonInfo->setGeometry (width() - ui->buttonInfo->width() - 5, height() - ui->buttonInfo->height() - 5, ui->buttonInfo->width(), ui->buttonInfo->height());
}

void MainWindow::onSelectionMade(const QString &word)
{
    ui->wordLabel->setText(word);
}

void MainWindow::initializeDictionary(const QString& wordsFile) {
    TIMED_FUNC(initializeDictionaryTimer);

    QFile file(wordsFile);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream inStream(&file);
        while (!file.atEnd()) {
            VLOG_EVERY_N(10000, 1) << "Still loading dictionary, this is iteration #" <<  ELPP_COUNTER_POS ;
            list->add(inStream.readLine());
        }
    } else {
        LOG(ERROR) << "Unable to open words.txt";
    }

}

void MainWindow::on_buttonInfo_clicked()
{
    QString infoText = QString("") +
            QString("This sample is to demonstrate a some usage of Easylogging++ and other possibilities.") +
            QString("You may use this sample as starting point of how you may log your Qt/C++ application.") +
            QString("Dictionary application has nothing to do with what happens internally in Easylogging++, in fact") +
            QString("this is just another application made for sample purpose.\n\n") +
            QString("This sample was made on 16G ram and 3.9GHz processor running Linux Mint 14 (Cinnamon) so it might") +
            QString("perform slow on your machine. But regardless of performance of this sample, Easylogging++") +
            QString(" itself should perform pretty good.");
    QMessageBox::information(this, "Information about this sample", infoText);
}
