#-------------------------------------------------
#
# Project created by QtCreator 2013-03-19T22:31:41
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = fast-dictionary
TEMPLATE = app

COMPILER = g++
QMAKE_CC = $$COMPILER
QMAKE_CXX = $$COMPILER
QMAKE_LINK = $$COMPILER

QMAKE_CXXFLAGS += -std=c++0x
DEFINES += _ELPP_STACKTRACE_ON_CRASH \
    _ELPP_MULTI_LOGGER_SUPPORT
    
SOURCES += main.cc\
        mainwindow.cc \
    listwithsearch.cc

HEADERS  += mainwindow.hh \
    listwithsearch.hh \
    easylogging++.h

FORMS    += mainwindow.ui
