QT += core gui widgets network sql serialport multimedia printsupport
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
    alarmsoundplayer.cpp \
    boreas3driver.cpp \
    changepassworddialog.cpp \
    commandconfirmdialog.cpp \
    dataviewdialog.cpp \
    dataviewproxies.cpp \
    dbloaderworker.cpp \
    devswdriver.cpp \
    driverconfigeditordialog.cpp \
    driverconfigmodel.cpp \
    equipmentconfigdialog.cpp \
    equipmentsettingsdialog.cpp \
    highlightrowdelegate.cpp \
    ledindicator.cpp \
    main.cpp \
    mainwindow.cpp \
    measurementsmodel.cpp \
    modbusclient.cpp \
    modulatorconfig.cpp \
    modulatorcontroldialog.cpp \
    modulatordriver.cpp \
    modulatortab.cpp \
    modulatortabbase.cpp \
    networkcontroller.cpp \
    basedocwidget.cpp \
    antennawidget.cpp \
    peredatchikwidget.cpp \
    modulatorwidget.cpp \
    energowidget.cpp \
    pollchain.cpp \
    pollscheduler.cpp \
    qcustomplot.cpp \
    ringindicator.cpp \
    screenconfigdialog.cpp \
    databasemanager.cpp \
    serialportclient.cpp \
    snmpclient.cpp \
    stubtab.cpp \
    switchtab.cpp \
    tcpasciiclient.cpp \
    teletehdriver.cpp \
    temonitorv6driver.cpp \
    useractionlogdialog.cpp

HEADERS += \
    alarmpanel.h \
    alarmregistry.h \
    alarmsoundplayer.h \
    authdialog.h \
    authrequireddialog.h \
    borderedcustomplot.h \
    boreas3driver.h \
    changepassworddialog.h \
    columnheaders.h \
    commandconfirmdialog.h \
    connectionparamsdialog.h \
    databaseconfigdialog.h \
    databaseinitwizard.h \
    databuffer.h \
    dataviewdialog.h \
    dataviewproxies.h \
    dbloaderworker.h \
    devicedata.h \
    deviceids.h \
    devswdriver.h \
    driverconfigeditordialog.h \
    driverconfigmodel.h \
    driverfactory.h \
    equipmentconfigdialog.h \
    equipmentsettingsdialog.h \
    highlightrowdelegate.h \
    idevicedriver.h \
    ledindicator.h \
    mainwindow.h \
    measurementsmodel.h \
    modbusclient.h \
    modulatorconfig.h \
    modulatorcontroldialog.h \
    modulatordriver.h \
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
    progressdialog.h \
    qcustomplot.h \
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
    useractionlogdialog.h \
    usermanagementdialog.h

FORMS += \
    antennaform.ui \
    dataviewdialog.ui \
    modulatortab.ui \
    peredatchikform.ui \
    modulatorform.ui \
    energoform.ui \
    stubtab.ui \
    switchtab.ui