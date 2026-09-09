QT       += core gui widgets network sql serialport
CONFIG   += c++17
TARGET    = rmo_server
TEMPLATE  = app

# =====================================================
# LINUX: Настройка PostgreSQL драйвера
# =====================================================
unix:!macx {
    # 1. Пути к плагинам SQL
    QMAKE_LFLAGS += -Wl,-rpath,/usr/lib/x86_64-linux-gnu/qt5/plugins/sqldrivers
    QMAKE_LFLAGS += -Wl,-rpath,/usr/lib/qt5/plugins/sqldrivers
    QMAKE_LFLAGS += -Wl,-rpath,/usr/lib/x86_64-linux-gnu

    # 2. Подключение библиотеки PostgreSQL
    LIBS += -lpq
    LIBS += -L/usr/lib/x86_64-linux-gnu
    LIBS += -L/usr/lib/postgresql/*/lib

    # 3. Пути к заголовкам PostgreSQL
    INCLUDEPATH += /usr/include/postgresql
    INCLUDEPATH += /usr/include/postgresql/*/server

    # 4. Копирование драйвера в папку сборки (для автономной работы)
    system(mkdir -p $$OUT_PWD/sqldrivers)
    system(cp /usr/lib/x86_64-linux-gnu/qt5/plugins/sqldrivers/libqsqlpsql.so $$OUT_PWD/sqldrivers/ 2>/dev/null || true)
    system(cp /usr/lib/qt5/plugins/sqldrivers/libqsqlpsql.so $$OUT_PWD/sqldrivers/ 2>/dev/null || true)

    message("✅ Linux: PostgreSQL driver configured")
}

# =====================================================
# СОРСЫ
# =====================================================
SOURCES += \
    alarmpanel.cpp \
    alarmregistry.cpp \
    boreas3driver.cpp \
    changepassworddialog.cpp \
    commandconfirmdialog.cpp \
    devswdriver.cpp \
    equipmentconfigdialog.cpp \
    equipmentsettingsdialog.cpp \
    ledindicator.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusclient.cpp \
    modulatorcontroldialog.cpp \
    modulatortab.cpp \
    modulatortabbase.cpp \
    networkcontroller.cpp \
    basedocwidget.cpp \
    antennawidget.cpp \
    peredatchikwidget.cpp \
    modulatorwidget.cpp \
    energowidget.cpp \
    pollscheduler.cpp \
    ringindicator.cpp \
    screenconfigdialog.cpp \
    databasemanager.cpp \
    serialportclient.cpp \
    snmpclient.cpp \
    stubtab.cpp \
    switchtab.cpp \
    tcpasciiclient.cpp \
    teletehdriver.cpp \
    temonitorv6driver.cpp

HEADERS += \
    alarmpanel.h \
    alarmregistry.h \
    authdialog.h \
    authrequireddialog.h \
    boreas3driver.h \
    changepassworddialog.h \
    commandconfirmdialog.h \
    connectionparamsdialog.h \
    databaseconfigdialog.h \
    databaseinitwizard.h \
    databuffer.h \
    devicedata.h \
    deviceids.h \
    devswdriver.h \
    driverfactory.h \
    equipmentconfigdialog.h \
    equipmentsettingsdialog.h \
    idevicedriver.h \
    ledindicator.h \
    mainwindow.h \
    modbusclient.h \
    modulatorcontroldialog.h \
    modulatortab.h \
    modulatortabbase.h \
    networkcontroller.h \
    basedocwidget.h \
    antennawidget.h \
    peredatchikwidget.h \
    modulatorwidget.h \
    energowidget.h \
    pollchain.h \
    pollscheduler.h \
    ringindicator.h \
    screenconfigdialog.h \
    databasemanager.h \
    serialportclient.h \
    serverclientconfigdialog.h \
    snmpclient.h \
    stubtab.h \
    switchtab.h \
    tcpasciiclient.h \
    teletehdriver.h \
    temonitorv6driver.h \
    temperaturedriver.h \
    usermanagementdialog.h

FORMS += \
    antennaform.ui \
    modulatortab.ui \
    peredatchikform.ui \
    modulatorform.ui \
    energoform.ui \
    screenconfigdialog.ui \
    stubtab.ui \
    switchtab.ui