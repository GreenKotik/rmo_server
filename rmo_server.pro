QT       += core gui widgets network sql
CONFIG   += c++17
TARGET    = rmo_server
TEMPLATE  = app

SOURCES += \
    alarmpanel.cpp \
    alarmregistry.cpp \
    changepassworddialog.cpp \
    equipmentconfigdialog.cpp \
    equipmentsettingsdialog.cpp \
    ledindicator.cpp \
    main.cpp \
    mainwindow.cpp \
    networkcontroller.cpp \
    basedocwidget.cpp \
    antennawidget.cpp \
    peredatchikwidget.cpp \
    modulatorwidget.cpp \
    energowidget.cpp \
    pollscheduler.cpp \
    screenconfigdialog.cpp \
    databasemanager.cpp

HEADERS += \
    alarmpanel.h \
    alarmregistry.h \
    authdialog.h \
    authrequireddialog.h \
    changepassworddialog.h \
    connectionparamsdialog.h \
    databaseconfigdialog.h \
    databaseinitwizard.h \
    devicedata.h \
    deviceids.h \
    driverfactory.h \
    equipmentconfigdialog.h \
    equipmentsettingsdialog.h \
    idevicedriver.h \
    ledindicator.h \
    mainwindow.h \
    networkcontroller.h \
    basedocwidget.h \
    antennawidget.h \
    peredatchikwidget.h \
    modulatorwidget.h \
    energowidget.h \
    pollchain.h \
    pollscheduler.h \
    screenconfigdialog.h \
    databasemanager.h \
    serverclientconfigdialog.h \
    temperaturedriver.h \
    usermanagementdialog.h

FORMS += \
    antennaform.ui \
    peredatchikform.ui \
    modulatorform.ui \
    energoform.ui \
    screenconfigdialog.ui