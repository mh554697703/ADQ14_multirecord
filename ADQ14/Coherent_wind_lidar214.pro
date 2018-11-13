#-------------------------------------------------
#
# Project created by QtCreator 2015-07-15T14:51:26
#
#-------------------------------------------------

QT       += core gui
QT       += serialport

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = Coherent_wind_lidar214
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp \
    paradialog.cpp \
    informationleft.cpp \
    plotwidget.cpp \
    threadstore.cpp \
    settingfile.cpp \
    serialportthread.cpp \
    plotdialog.cpp \
    helpdialog.cpp

HEADERS  += mainwindow.h \
    paradialog.h \
    Acqsettings.h \
    informationleft.h \
    plotwidget.h \
    threadstore.h \
    settingfile.h \
    serialportthread.h \
    plotdialog.h \
    helpdialog.h

FORMS    += mainwindow.ui \
    paradialog.ui \
    informationleft.ui \
    plotdialog.ui \
    helpdialog.ui

win32: LIBS += -L$$quote(C:/Program Files/SP Devices/ADQAPI/) -lADQAPI
INCLUDEPATH += $$quote(C:/Program Files/SP Devices/ADQAPI/)

RESOURCES += \
    images.qrc

CONFIG    += qwt

RC_ICONS = images\Convert.ico
