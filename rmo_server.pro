QT       += core gui widgets network sql 
QT += serialport  # ✅ Для поддержки COM-портов
CONFIG   += c++17
TARGET    = rmo_server
TEMPLATE  = app

SOURCES += \
    alarmpanel.cpp \
    alarmregistry.cpp \
    boreas3driver.cpp \
    changepassworddialog.cpp \
    equipmentconfigdialog.cpp \
    equipmentsettingsdialog.cpp \
    ledindicator.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusclient.cpp \
    networkcontroller.cpp \
    basedocwidget.cpp \
    antennawidget.cpp \
    peredatchikwidget.cpp \
    modulatorwidget.cpp \
    energowidget.cpp \
    pollscheduler.cpp \
    screenconfigdialog.cpp \
    databasemanager.cpp \
    serialportclient.cpp \
    snmpclient.cpp \
    tcpasciiclient.cpp \
    temonitorv6driver.cpp

HEADERS += \
    alarmpanel.h \
    alarmregistry.h \
    authdialog.h \
    authrequireddialog.h \
    boreas3driver.h \
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
    modbusclient.h \
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
    serialportclient.h \
    serverclientconfigdialog.h \
    snmpclient.h \
    tcpasciiclient.h \
    temonitorv6driver.h \
    temperaturedriver.h \
    usermanagementdialog.h

FORMS += \
    antennaform.ui \
    peredatchikform.ui \
    modulatorform.ui \
    energoform.ui \
    screenconfigdialog.ui