#-------------------------------------------------
#
# Project created by QtCreator 2014-01-13T17:48:26
#
#-------------------------------------------------

QT       += core gui webkitwidgets webkit widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = test1
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    ctools.cpp

HEADERS  += mainwindow.h \
    ctools.h \
    imx_adc.h	

FORMS    += mainwindow.ui
