QT       += core
greaterThan(QT_MAJOR_VERSION, 4)

CONFIG += static
DEFINES += _ELPP_QT_LOGGING    \
          _ELPP_STL_LOGGING   \
          _ELPP_STRICT_SIZE_CHECK _ELPP_UNICODE \
          _ELPP_MULTI_LOGGER_SUPPORT \
          _ELPP_THREAD_SAFE

TARGET = main.cpp.bin
TEMPLATE = app
QMAKE_CXXFLAGS += -std=c++11
SOURCES += main.cpp
HEADERS += \
           mythread.h \
    easylogging++.h

OTHER_FILES += \
    test_conf.conf
