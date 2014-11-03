#include "mainwindow.hh"
#include <QApplication>
#include "easylogging++.h"

_INITIALIZE_EASYLOGGINGPP

TIMED_SCOPE(app, "app");

int main(int argc, char *argv[])
{
    _START_EASYLOGGINGPP(argc, argv);
    el::Loggers::reconfigureAllLoggers(el::ConfigurationType::Format, "%datetime{%d/%M/%y} %msg");
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    for (int i = 1; i <= 10; ++i) {
        CLOG_EVERY_N(2, INFO, "default", "performance") << ELPP_COUNTER_POS;
    }
    return a.exec();
}
