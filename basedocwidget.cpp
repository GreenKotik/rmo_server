#include "basedocwidget.h"
#include "databasemanager.h"  // Добавьте в начало файла
#include "mainwindow.h"       // Для доступа к MainWindow и getEquipmentConnectionNames
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDebug>
#include <QFileInfo>
#include <QDir>               // ✅ ДОБАВИТЬ для QDir().mkpath()
#include <QTextStream>
#include <QDateTime>
#include <QCoreApplication>   // ✅ ДОБАВИТЬ для QCoreApplication::applicationDirPath()
#include <QTextCodec>  // ✅ Нужен для QTextCodec в Qt5
#include <QStyle>    // ✅ Нужен для style()->unpolish/polish
#include <QPalette>  // ✅ Нужен для QPalette
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <algorithm>
#include "equipmentsettingsdialog.h"  // ✅ Для открытия диалога настройки
#include "pollscheduler.h"   // ✅ Для PollScheduler
#include "devicedata.h"      // ✅ Для IDeviceData
#include "alarmregistry.h"   // ✅ Для AlarmRegistry
#include "dataviewdialog.h"  // ✅ Добавляем после других includes
#include "alarmsoundplayer.h"

BaseDocWidget::BaseDocWidget(QWidget *parent)
    : QWidget(parent),
      m_docServer(new QTcpServer(this)),
      m_docPort(0),
      m_docHeartbeatTimer(new QTimer(this))
{
    // ✅ Растягиваться и по горизонтали, и по вертикали
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(this, &BaseDocWidget::pollCycleStarted,
            this, &BaseDocWidget::triggerIndicatorBlink);

    connect(m_docHeartbeatTimer, &QTimer::timeout, this, &BaseDocWidget::onDocHeartbeatTimeout);
    m_docHeartbeatTimer->start(10000);
}

BaseDocWidget::~BaseDocWidget()
{
    qDebug() << "[BaseDocWidget] Деструктор НАЧАЛО:" << m_filePath;

    // ✅ Убираем документ из очереди глобального плеера.
    AlarmSoundPlayer::instance().unregisterDoc(
        QFileInfo(m_filePath).completeBaseName());

    // 1. Останавливаем таймер heartbeat
    if (m_docHeartbeatTimer) {
        m_docHeartbeatTimer->stop();
        qDebug() << "[BaseDocWidget] Heartbeat timer остановлен";
    }

    // 2. Отключаем все сигналы от сервера
    if (m_docServer) {
        m_docServer->disconnect();
        m_docServer->close();
        qDebug() << "[BaseDocWidget] DocServer закрыт";
    }

    // 3. Отключаем и удаляем все клиентские сокеты
    for (QTcpSocket *sock : m_docClients) {
        if (sock) {
            sock->disconnect();
            sock->abort();
            sock->deleteLater();
        }
    }
    m_docClients.clear();
    m_docBuffers.clear();
    m_docStates.clear();
    qDebug() << "[BaseDocWidget] Клиентские сокеты очищены";

    // 4. Корректная остановка PollScheduler
    if (m_pollScheduler) {
        qDebug() << "[BaseDocWidget] Остановка PollScheduler...";
        m_pollScheduler->stop();

        // Отключаем все сигналы от PollScheduler
        m_pollScheduler->disconnect();

        delete m_pollScheduler;
        m_pollScheduler = nullptr;
        qDebug() << "[BaseDocWidget] PollScheduler удалён";
    }

    qDebug() << "[BaseDocWidget] Деструктор ЗАВЕРШЕНИЕ:" << m_filePath;
}


void BaseDocWidget::initDocumentNetwork(AppMode mode, quint16 listenPort, const QString &connectTo, quint16 connectPort)
{
    if (mode == AppMode::Server) {
        quint16 port = listenPort;
        while (!m_docServer->listen(QHostAddress::Any, port)) { if (m_docServer->serverError() == QAbstractSocket::AddressInUseError) port++; else return; }
        m_docPort = port;
        connect(m_docServer, &QTcpServer::newConnection, this, &BaseDocWidget::onDocNewConnection);
        qDebug() << "[DocServer] Listening on port" << m_docPort << "for" << filePath();
    } else {
        QTcpSocket *sock = new QTcpSocket(this);
        connect(sock, &QTcpSocket::readyRead, [this, sock]() {
            m_docBuffers[sock].append(sock->readAll()); m_docStates[sock].lastActivity = QDateTime::currentMSecsSinceEpoch();
            QJsonObject obj; while (parsePacket(m_docBuffers[sock], obj)) {
                if (obj["type"] == "ping") { QJsonObject pong; pong["type"] = "pong"; sock->write(serializePacket(pong)); continue; }
                processDocClientData(obj);
            }
        });
        connect(sock, &QTcpSocket::disconnected, sock, &QTcpSocket::deleteLater);
        sock->connectToHost(connectTo, connectPort);
        m_docClients.append(sock);
    }
}

void BaseDocWidget::onDocNewConnection() {
    while (m_docServer->hasPendingConnections()) {
        QTcpSocket *sock = m_docServer->nextPendingConnection();
        m_docClients.append(sock); m_docStates[sock] = {QDateTime::currentMSecsSinceEpoch(), 0, false};
        connect(sock, &QTcpSocket::readyRead, this, &BaseDocWidget::onDocClientReadyRead);
        connect(sock, &QTcpSocket::disconnected, this, &BaseDocWidget::onDocClientDisconnected);
    }
}

void BaseDocWidget::onDocClientReadyRead() {
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender()); if (!sock) return;
    m_docStates[sock].lastActivity = QDateTime::currentMSecsSinceEpoch();
    m_docBuffers[sock].append(sock->readAll()); QJsonObject obj;
    while (parsePacket(m_docBuffers[sock], obj)) {
        if (obj["type"] == "pong") { m_docStates[sock].pingPending = false; continue; }
        processDocClientData(obj);
    }
}

void BaseDocWidget::onDocClientDisconnected() {
    QTcpSocket *sock = qobject_cast<QTcpSocket*>(sender());
    if (sock) { m_docBuffers.remove(sock); m_docStates.remove(sock); sock->deleteLater(); }
}

void BaseDocWidget::onDocHeartbeatTimeout() {
    const qint64 now = QDateTime::currentMSecsSinceEpoch(); QList<QTcpSocket*> stale;
    for (QTcpSocket *sock : m_docClients) {
        if (!sock || sock->state() != QTcpSocket::ConnectedState) continue;
        DocClientState &st = m_docStates[sock];
        if (!st.pingPending && (now - st.lastActivity > 15000)) {
            QJsonObject pingMsg; pingMsg["type"] = "ping"; sock->write(serializePacket(pingMsg));
            st.pingPending = true; st.pingSent = now;
        } else if (st.pingPending && (now - st.pingSent > 10000)) { stale.append(sock); }
    }
    for (QTcpSocket *sock : stale) { qWarning() << "[DocHeartbeat] Client stale, disconnecting."; sock->disconnectFromHost(); m_docBuffers.remove(sock); m_docStates.remove(sock); }
}

void BaseDocWidget::broadcastDocUpdate(const QJsonObject &payload) {
    if (m_docClients.isEmpty()) return;
    QJsonObject msg; msg["type"] = "doc_update"; msg["path"] = filePath(); msg["payload"] = payload;
    QByteArray packet = serializePacket(msg);
    for (QTcpSocket *s : m_docClients) if (s->isOpen()) s->write(packet);
}

QByteArray BaseDocWidget::serializePacket(const QJsonObject &obj)
{
    QByteArray payload;
    QDataStream outP(&payload, QIODevice::WriteOnly);
    outP.setVersion(QDataStream::Qt_5_15);
    outP << obj;

    QByteArray packet;
    QDataStream out(&packet, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << static_cast<quint32>(payload.size());
    packet.append(payload);

    return packet;
}

bool BaseDocWidget::parsePacket(QByteArray &buffer, QJsonObject &outObj)
{
    if (buffer.size() < 4) {
        return false;
    }

    QDataStream in(buffer);
    in.setVersion(QDataStream::Qt_5_15);
    quint32 size;
    in >> size;

    if (buffer.size() < 4 + static_cast<int>(size)) {
        return false;
    }

    QByteArray payload = buffer.mid(4, size);
    QDataStream inP(payload);
    inP.setVersion(QDataStream::Qt_5_15);
    inP >> outObj;

    buffer.remove(0, 4 + size);
    return true;
}

bool BaseDocWidget::saveToFilePreserveSections(const QString &filePath)
{
    QFile file(filePath);
    QJsonObject docObj;

    // 1. Читаем существующий файл (чтобы сохранить все секции)
    if (file.exists() && file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        docObj = doc.object();
        file.close();
    }

    // 2. Обновляем только секцию "parameters"
    docObj["parameters"] = getDocumentParameters();

    // 3. Сохраняем обратно
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(docObj).toJson());
        file.close();

        // Сохраняем полный документ в памяти
        m_fullDocument = docObj;

        qDebug() << "[BaseDocWidget] Сохранён файл с сохранением всех секций:" << filePath;
        return true;
    } else {
        qWarning() << "[BaseDocWidget] Ошибка сохранения файла:" << filePath;
        return false;
    }
}

bool BaseDocWidget::loadFromFilePreserveSections(const QString &filePath)
{
    // ========================================================================
    // ✅ 1. СБРОС СОСТОЯНИЯ ПЕРЕД ЗАГРУЗКОЙ НОВОГО ДОКУМЕНТА
    // ========================================================================

    // Флаг первого опроса — чтобы индикатор заголовка не мигал при старте
    m_firstPollReceived = false;

    // Кэш последних отправленных аварий — чтобы первый finalizePollCycle()
    // гарантированно эмитил alarmsUpdated (даже если список пуст).
    m_lastSentAlarmCodes.clear();
    m_lastSentAlarmStatuses.clear();
    m_alarmsCacheInitialized = false;
    // ✅ Сброс кэша подтверждённых аварий при загрузке нового документа.
    m_acknowledgedAlarms.clear();

    // ✅ НОВОЕ: счётчики записи в БД.
    //   m_msSinceLastDbWrite       — накопление до db_write_interval;
    //   m_msSinceAlarmWindowStart  — накопление окна аварийной записи
    //                                (alarm_write_duration_min).
    //   Оба должны стартовать с нуля при загрузке нового файла.
    m_msSinceLastDbWrite      = 0;
    m_msSinceAlarmWindowStart = 0;

    // Индикатор заголовка — выключаем, пока не получим первый ответ от устройства
    if (m_titleIndicator) {
        m_titleIndicator->setState(LedIndicator::State::Gray);
    }

    // ========================================================================
    // ✅ 2. ОТКРЫТИЕ И ЧТЕНИЕ ФАЙЛА
    // ========================================================================
    QFile file(filePath);
    if (!file.exists()) return false;

    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_fullDocument = doc.object();
        file.close();

        // ====================================================================
        // ✅ 3. ЗАГРУЗКА И ПРОВЕРКА ПОДКЛЮЧЕНИЙ ОБОРУДОВАНИЯ
        // ====================================================================
        loadAndValidateEquipmentConnections();

        // ====================================================================
        // ✅ 4. ИНИЦИАЛИЗАЦИЯ ПЛАНИРОВЩИКА ОПРОСА (если ещё не создан)
        // ====================================================================
        if (!m_pollScheduler) {
            m_pollScheduler = new PollScheduler(this, this);

            connect(m_pollScheduler, &PollScheduler::pollResult,
                    this, &BaseDocWidget::processPollResult);

            // Проксируем сигнал начала цикла (для синхронного мигания индикатора)
            connect(m_pollScheduler, &PollScheduler::cycleStarted,
                    this, &BaseDocWidget::pollCycleStarted);

            // Завершение всех цепочек Fast Pool → обновление UI/БД/рассылки
            connect(m_pollScheduler, &PollScheduler::allCyclesFinished,
                    this, &BaseDocWidget::updateUIFromBuffer,
                    Qt::QueuedConnection);  // Гарантированно в главном потоке

            connect(m_pollScheduler, &PollScheduler::errorOccurred,
                    this, [this](int chainId, int deviceIndex, const QString &error) {
                        qWarning() << "[BaseDocWidget] Ошибка опроса:"
                                   << "chainId:" << chainId
                                   << "deviceIndex:" << deviceIndex
                                   << "error:" << error;
                    });

            qDebug() << "[BaseDocWidget] PollScheduler создан для:" << filePath;
        }

        // ====================================================================
        // ✅ 5. ЧТЕНИЕ ИНТЕРВАЛА ОПРОСА ИЗ СЕКЦИИ settings
        // ====================================================================
        QJsonObject settings = m_fullDocument.value("settings").toObject();
        int pollingInterval = settings.value("polling_interval").toInt(1000);

        // Защита от слишком маленького интервала
        if (pollingInterval < 100) {
            qWarning() << "[BaseDocWidget] Интервал опроса слишком маленький ("
                       << pollingInterval << "мс), установлено 100 мс";
            pollingInterval = 100;
        }

        m_pollScheduler->setPollingInterval(pollingInterval);
        qDebug() << "[BaseDocWidget] Интервал опроса:" << pollingInterval << "мс";

        // ====================================================================
        // ✅ 6. ИНИЦИАЛИЗАЦИЯ ЦЕПОЧЕК И ЗАПУСК ОПРОСА
        // ====================================================================
        m_pollScheduler->initializeChains();
        m_pollScheduler->start();

        // При необходимости — можно включить Fast-режим (закомментировано):
        // setFastPolling(true);

        // ====================================================================
        // ✅ 7. УСТАНОВКА ПАРАМЕТРОВ ДОКУМЕНТА (виртуальный метод наследника)
        // ====================================================================
        setDocumentParameters(m_fullDocument.value("parameters").toObject());

        qDebug() << "[BaseDocWidget] Загружен файл и запущен опрос:" << filePath;

        // ====================================================================
        // ✅ 8. АВТОМАТИЧЕСКОЕ СОЗДАНИЕ ТАБЛИЦЫ В БД
        // ====================================================================
        ensureDatabaseTable();

        // ====================================================================
        // ✅ 9. ХУК ДЛЯ НАСЛЕДНИКОВ (загрузка кэша настроек, UI и т.п.)
        // ====================================================================
        onFileLoaded();

        return true;
    }

    return false;
}

QJsonObject BaseDocWidget::getDocumentParameters() const
{
    // Базовая реализация — пустой объект
    // Наследники должны переопределить этот метод
    return QJsonObject();
}

void BaseDocWidget::setDocumentParameters(const QJsonObject &params)
{
    // Базовая реализация — ничего не делаем
    // Наследники должны переопределить этот метод
    Q_UNUSED(params);
}

// ✅ НОВЫЙ МЕТОД: Установка состояния заголовка (0-3)
void BaseDocWidget::setEquipmentState(int state)
{
    // Нормализуем значение в диапазон 0-3
    if (state < 0) state = 0;
    if (state > 3) state = 3;

    if (m_equipmentState != state) {
        m_equipmentState = state;
        QString stateName;
        switch (state) {
            case 0: stateName = "СИНИЙ (дежурный)"; break;
            case 1: stateName = "ЗЕЛЁНЫЙ (в работе)"; break;
            case 2: stateName = "ОРАНЖЕВЫЙ (авария)"; break;
            case 3: stateName = "КРАСНЫЙ (критическая ошибка)"; break;
        }
        qDebug() << "[BaseDocWidget] setEquipmentState:" << stateName;
        setTitleBarState(state);
    }
}

// ✅ Покраска заголовка через paintEvent (РАБОЧИЙ ВАРИАНТ)
void BaseDocWidget::setTitleBarState(int state)
{
    if (m_titleBarWidget) {
        if (CustomTitleBar *titleBar = qobject_cast<CustomTitleBar*>(m_titleBarWidget)) {
            QColor color;
            switch (state) {
                case 0: color = QColor("#005A9E"); break; // Синий (дежурный)
                case 1: color = QColor("#2E7D32"); break; // Зелёный (в работе)
                case 2: color = QColor("#FF6F00"); break; // Оранжевый (авария)
                case 3: color = QColor("#C62828"); break; // Красный (критическая)
                default: color = QColor("#005A9E"); break;
            }
            titleBar->setBackgroundColor(color);
        }
    }
}

// ✅ НОВЫЙ МЕТОД: Загрузка и проверка подключений оборудования
void BaseDocWidget::loadAndValidateEquipmentConnections()
{
    // 1. Получаем ожидаемые названия из MainWindow
    MainWindow *mainWin = qobject_cast<MainWindow*>(parent());
    QStringList expectedNames;
    if (mainWin) {
        expectedNames = mainWin->getEquipmentConnectionNames(docType());
    } else {
        qWarning() << "[BaseDocWidget] ВНИМАНИЕ: mainWin = nullptr! Невозможно проверить подключения";
        return;
    }

    // 2. Создаём базовый массив с ожидаемыми названиями
    // ✅ ПО УМОЛЧАНИЮ errorCode = 4 (строка отсутствует в файле)
    m_equipmentConnections.clear();
    for (const QString &name : expectedNames) {
        EquipmentConnection conn;
        conn.name = name;
        conn.connected = false;
        conn.address = "";
        conn.equipmentType = "";
        conn.deviceId = ID_UNKNOWN;  // ✅ НОВОЕ: по умолчанию неизвестно
        conn.useStream = false;
        conn.connectionInterface = "TCP/IP";
        conn.ipAddress = "";
        conn.port = 502;
        conn.snmpV1Enabled = false;
        conn.snmpV1CommunityRead = "public";
        conn.snmpV1CommunityWrite = "private";
        conn.snmpV3Enabled = false;
        conn.snmpV3Username = "";
        conn.snmpV3AuthProtocol = "MD5";
        conn.snmpV3AuthPassword = "";
        conn.snmpV3PrivProtocol = "DES";
        conn.snmpV3PrivPassword = "";
        conn.errorCode = 4; // ✅ ПО УМОЛЧАНИЮ — отсутствует в файле
        m_equipmentConnections.append(conn);
    }

    // 3. Загружаем данные из JSON и проверяем индексы + имена
    QJsonArray jsonConnections = m_fullDocument.value("connections").toArray();
    bool hasError = false;

    for (int i = 0; i < jsonConnections.size(); ++i) {
        if (i >= m_equipmentConnections.size()) {
            qWarning() << "[BaseDocWidget] В JSON больше подключений, чем ожидается!";
            hasError = true;
            break;
        }

        QJsonObject connObj = jsonConnections[i].toObject();

        // ✅ ПРОВЕРКА ИНДЕКСА
        int jsonIndex = connObj.value("index").toInt(-1);
        bool indexOk = (jsonIndex == i);

        // ✅ ПРОВЕРКА ИМЕНИ
        QString jsonName = connObj.value("name").toString();
        QString expectedName = m_equipmentConnections[i].name;
        bool nameOk = (jsonName == expectedName);

        // ✅ Устанавливаем код ошибки
        if (indexOk && nameOk) {
            m_equipmentConnections[i].errorCode = 0; // ✅ Всё ОК
        } else if (!indexOk && !nameOk) {
            m_equipmentConnections[i].errorCode = 3; // ✅ Оба не совпадают
            qWarning() << "[BaseDocWidget] Ошибка в строке" << i
                       << ": индекс (ожидается" << i << ", получено" << jsonIndex
                       << "), имя (ожидается \"" << expectedName
                       << "\", получено \"" << jsonName << "\")";
        } else if (!indexOk) {
            m_equipmentConnections[i].errorCode = 1; // ✅ Только индекс не совпадает
            qWarning() << "[BaseDocWidget] Ошибка индекса в строке" << i
                       << ": ожидается" << i << ", получено" << jsonIndex;
        } else {
            m_equipmentConnections[i].errorCode = 2; // ✅ Только имя не совпадает
            qWarning() << "[BaseDocWidget] Ошибка имени в строке" << i
                       << ": ожидается \"" << expectedName
                       << "\", получено \"" << jsonName << "\"";
        }

        if (m_equipmentConnections[i].errorCode != 0) {
            hasError = true;
        }

        // Загружаем остальные поля
        m_equipmentConnections[i].connected = connObj.value("connected").toBool(false);
        m_equipmentConnections[i].address = connObj.value("address").toString();
        m_equipmentConnections[i].equipmentType = connObj.value("equipmentType").toString();
        m_equipmentConnections[i].deviceId = connObj.value("deviceId").toInt(ID_UNKNOWN);  // ✅ НОВОЕ
        m_equipmentConnections[i].useStream = connObj.value("useStream").toBool(false);
        m_equipmentConnections[i].slowPool = connObj.value("slowPool").toBool(false);  // ✅ НОВОЕ
        m_equipmentConnections[i].connectionInterface = connObj.value("connectionInterface").toString("TCP/IP");
        m_equipmentConnections[i].ipAddress = connObj.value("ipAddress").toString();
        m_equipmentConnections[i].port = connObj.value("port").toInt(502);
        m_equipmentConnections[i].snmpV1Enabled = connObj.value("snmpV1Enabled").toBool(false);
        m_equipmentConnections[i].snmpV1CommunityRead = connObj.value("snmpV1CommunityRead").toString("public");
        m_equipmentConnections[i].snmpV1CommunityWrite = connObj.value("snmpV1CommunityWrite").toString("private");
        m_equipmentConnections[i].snmpV3Enabled = connObj.value("snmpV3Enabled").toBool(false);
        m_equipmentConnections[i].snmpV3Username = connObj.value("snmpV3Username").toString();
        m_equipmentConnections[i].snmpV3AuthProtocol = connObj.value("snmpV3AuthProtocol").toString("MD5");
        m_equipmentConnections[i].snmpV3AuthPassword = connObj.value("snmpV3AuthPassword").toString();
        m_equipmentConnections[i].snmpV3PrivProtocol = connObj.value("snmpV3PrivProtocol").toString("DES");
        m_equipmentConnections[i].snmpV3PrivPassword = connObj.value("snmpV3PrivPassword").toString();
        // ✅ НОВОЕ: Modbus/COM параметры
        m_equipmentConnections[i].comPort = connObj.value("modbusComPort").toString();
        m_equipmentConnections[i].baudRate = connObj.value("modbusBaudRate").toInt(9600);
        m_equipmentConnections[i].dataBits = connObj.value("modbusDataBits").toInt(8);
        m_equipmentConnections[i].parity = connObj.value("modbusParity").toString("None");
        m_equipmentConnections[i].stopBits = connObj.value("modbusStopBits").toInt(1);
    }
    // ========================================================================
    // ✅ НОВОЕ: Заполняем deviceSettings для температурных устройств
    // (для устройств других типов — по аналогии, когда появятся настройки)
    // ========================================================================
    QJsonArray tempSensors = m_fullDocument.value("temperature_sensors").toArray();

    for (int i = 0; i < m_equipmentConnections.size(); ++i) {
        EquipmentConnection &conn = m_equipmentConnections[i];

        if (conn.deviceId == ID_TEMP_SNS_BOREAS_3 ||
            conn.deviceId == ID_TEMP_SNS_TE_MONITOR_V6) {

            QJsonObject settings;
            settings["sensors"] = tempSensors;
            conn.deviceSettings = settings;

            qDebug() << "[BaseDocWidget] deviceSettings заполнены для"
                     << conn.name << "(deviceId:" << conn.deviceId
                     << ", датчиков:" << tempSensors.size() << ")";
        }
    }

    // ✅ Проверяем строки, которых нет в JSON (они остались с errorCode = 4)
    for (int i = jsonConnections.size(); i < m_equipmentConnections.size(); ++i) {
        qWarning() << "[BaseDocWidget] Строка" << i << "(\"" << m_equipmentConnections[i].name
                   << "\") отсутствует в файле конфигурации!";
        hasError = true;
    }

    // 4. Устанавливаем состояние заголовка с учётом флага "в работе"
    if (hasError) {
        // ✅ Если есть ошибка — оранжевый (авария имеет приоритет)
        setEquipmentState(2);
    } else if (isInWork()) {
        // ✅ Нет ошибки + оборудование в работе → ЗЕЛЁНЫЙ
        setEquipmentState(1);
    } else {
        // ✅ Нет ошибки + не в работе → СИНИЙ
        setEquipmentState(0);
    }

    qDebug() << "[BaseDocWidget] Загружено подключений:" << m_equipmentConnections.size()
             << ", ошибок:" << (hasError ? "ДА" : "нет");
}

// ✅ НОВЫЙ МЕТОД: Запись ошибки в лог-файл
void BaseDocWidget::writeEquipmentErrorLog()
{
    QString logDir = QCoreApplication::applicationDirPath() + "/log";
    QDir().mkpath(logDir);

    QString logFile = logDir + "/" + QFileInfo(m_filePath).completeBaseName() + ".log";
    QFile f(logFile);

    if (f.open(QIODevice::Append | QIODevice::Text)) {
        // ✅ Формируем детальное сообщение об ошибках
        QStringList errorDetails;
        for (int i = 0; i < m_equipmentConnections.size(); ++i) {
            const EquipmentConnection &conn = m_equipmentConnections[i];
            if (conn.errorCode == 0) continue;

            QString issue;
            switch (conn.errorCode) {
                case 1:
                    issue = "не совпадает индекс";
                    break;
                case 2:
                    issue = "не совпадает имя";
                    break;
                case 3:
                    issue = "не совпадают индекс и имя";
                    break;
                case 4:
                    issue = "строка отсутствует в файле конфигурации";
                    break;
                default:
                    issue = QString("неизвестная ошибка (код: %1)").arg(conn.errorCode);
                    break;
            }

            errorDetails << QString("  - строка %1 (\"%2\"): %3")
                .arg(i)
                .arg(conn.name)
                .arg(issue);
        }

        QString message = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")
            + " | АВАРИЙНОЕ СООБЩЕНИЕ: Обнаружено повреждение конфигурации оборудования. "
              "Файл: " + QFileInfo(m_filePath).fileName() + "\n"
              "Несовпадения с ожидаемой конфигурацией:\n"
            + errorDetails.join("\n") + "\n\n";

        QByteArray data = message.toLocal8Bit();
        f.write(data);
        f.close();

        qDebug() << "[BaseDocWidget] Записана ошибка в лог:" << logFile;
    } else {
        qWarning() << "[BaseDocWidget] Не удалось записать лог:" << logFile;
    }
}

// ✅ НОВЫЙ МЕТОД: Контекстное меню при правом клике в области документа
void BaseDocWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // ✅ Поиск MainWindow в иерархии родителей
    // Используем parentWidget() — он возвращает QWidget*, в отличие от parent() → QObject*
    MainWindow *mainWin = nullptr;
    QWidget *p = parentWidget();
    while (p) {
        mainWin = qobject_cast<MainWindow*>(p);
        if (mainWin) break;
        p = p->parentWidget();  // ✅ Используем parentWidget()
    }

    if (!mainWin) {
        qWarning() << "[BaseDocWidget] Не удалось найти MainWindow в иерархии родителей!";
        return;
    }

    // Создаём контекстное меню
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { "
        "  background-color: #FFFFFF; "
        "  border: 1px solid #B0B0B0; "
        "  padding: 4px; "
        "} "
        "QMenu::item { "
        "  padding: 6px 24px 6px 12px; "
        "  background-color: transparent; "
        "} "
        "QMenu::item:selected { "
        "  background-color: #005A9E; "
        "  color: white; "
        "}"
    );

    // ✅ НОВОЕ: Пункт "Просмотр графика"
    QAction *graphAct = menu.addAction("📊 Просмотр графика");
    // ✅ Только один пункт — "Настройка параметров" (не требует авторизации)
    QAction *settingsAct = menu.addAction("⚙️ Настройка параметров");

    // Показываем меню в позиции клика
    QAction *selected = menu.exec(event->globalPos());

    // Обработка выбора
    if (selected == graphAct) {
        // ✅ ОТКРЫВАЕМ ДИАЛОГ ПРОСМОТРА ГРАФИКА
        onViewDataGraph();
    }
    else if (selected == settingsAct) {
        EquipmentSettingsDialog dlg(mainWin);
        dlg.setDocumentFilePath(m_filePath);
        dlg.setCurrentUser(mainWin->getCurrentUser());
        dlg.exec();
    }
}

// ✅ НОВЫЙ МЕТОД: Запуск опроса оборудования
void BaseDocWidget::startPolling()
{
    if (!m_pollScheduler) {
        qWarning() << "[BaseDocWidget] PollScheduler не создан!";
        return;
    }

    m_pollScheduler->initializeChains();
    m_pollScheduler->start();

    qDebug() << "[BaseDocWidget] Опрос оборудования запущен для:" << filePath();
}

// ✅ НОВЫЙ МЕТОД: Остановка опроса оборудования
void BaseDocWidget::stopPolling()
{
    if (m_pollScheduler) {
        m_pollScheduler->stop();
        qDebug() << "[BaseDocWidget] Опрос оборудования остановлен для:" << filePath();
    }
}

// ✅ НОВЫЙ МЕТОД: Перезагрузка настроек опроса из файла
void BaseDocWidget::reloadPollingSettings()
{
    if (!m_pollScheduler) {
        qWarning() << "[BaseDocWidget] PollScheduler не создан, перезапуск невозможен";
        return;
    }

    // ========================================================================
    // 1. Перечитываем файл, чтобы получить актуальные настройки
    // ========================================================================
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "[BaseDocWidget] Не удалось открыть файл для перечитывания настроек";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_fullDocument = doc.object();
    file.close();

    // ========================================================================
    // ✅ 2. НОВОЕ: Обновляем deviceSettings в m_equipmentConnections
    //    (чтобы драйверы получили актуальные настройки после пересоздания)
    // ========================================================================
    QJsonArray tempSensors = m_fullDocument.value("temperature_sensors").toArray();

    for (int i = 0; i < m_equipmentConnections.size(); ++i) {
        EquipmentConnection &conn = m_equipmentConnections[i];

        if (conn.deviceId == ID_TEMP_SNS_BOREAS_3 ||
            conn.deviceId == ID_TEMP_SNS_TE_MONITOR_V6) {

            QJsonObject settings;
            settings["sensors"] = tempSensors;
            conn.deviceSettings = settings;

            qDebug() << "[BaseDocWidget] reloadPollingSettings: deviceSettings обновлены для"
                     << conn.name << "(датчиков:" << tempSensors.size() << ")";
        }
    }

    // ========================================================================
    // 3. Читаем новый интервал опроса
    // ========================================================================
    QJsonObject settings = m_fullDocument.value("settings").toObject();
    int newInterval = settings.value("polling_interval").toInt(1000);

    if (newInterval < 100) {
        qWarning() << "[BaseDocWidget] Интервал опроса слишком маленький ("
                   << newInterval << "мс), установлено 100 мс";
        newInterval = 100;
    }

    // ========================================================================
    // 4. Останавливаем старый опрос
    // ========================================================================
    m_pollScheduler->stop();

    // ========================================================================
    // 5. Применяем новый интервал
    // ========================================================================
    m_pollScheduler->setPollingInterval(newInterval);

    // ========================================================================
    // 6. Пересоздаём цепочки (драйверы получат обновлённые deviceSettings)
    // ========================================================================
    m_pollScheduler->initializeChains();

    // ========================================================================
    // 7. Запускаем опрос заново
    // ========================================================================
    m_pollScheduler->start();

    // ========================================================================
    // 8. Обновляем состояние заголовка (на случай изменения флага "в работе")
    // ========================================================================
    updateAlarmState();

    qDebug() << "[BaseDocWidget] Настройки опроса перезагружены. Новый интервал:"
             << newInterval << "мс";

    // ✅ НОВОЕ: Вызываем хук для наследников
    // (перечитать кэшированные настройки, обновить UI и т.п.)
    onSettingsReloaded();
}

// ✅ НОВЫЙ МЕТОД: Отправка команды управления устройству
void BaseDocWidget::sendCommandToDevice(const QString &deviceName, const ControlCommand &cmd)
{
    if (m_pollScheduler) {
        m_pollScheduler->sendCommand(deviceName, cmd);
    }
}

void BaseDocWidget::processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data)
{
    Q_UNUSED(chainId);
    Q_UNUSED(deviceIndex);

    if (!data) return;

    int deviceId = data->deviceId();
    const DeviceErrors devErrors = data->getErrors();

    // ========================================================================
    // ✅ ТОЛЬКО СБОР ДАННЫХ — ничего не эмитим, UI не трогаем, в БД не пишем.
    // Вся обработка цикла — в updateUIFromBuffer() (вызывается один раз
    // после завершения всех цепочек Fast Pool).
    // ========================================================================
    {
        QWriteLocker locker(&m_dataLock);

        // Сохраняем последние данные от устройства
        m_lastDeviceData[deviceId] = data;

        // Обновляем накопленные ошибки по этому устройству.
        // При каждом опросе ЗАМЕНЯЕМ список — аварии автоматически
        // исчезают из m_deviceErrors, когда пропадают у устройства.
        if (devErrors.codes.empty()) {
            m_deviceErrors.remove(deviceId);
        } else {
            QList<int> codes;
            codes.reserve(static_cast<int>(devErrors.codes.size()));
            for (int c : devErrors.codes) codes.append(c);
            m_deviceErrors[deviceId] = codes;
        }
    }

    // Логируем только при наличии ошибок
    if (data->hasErrors()) {
        qDebug() << "[BaseDocWidget] Получены данные с ошибками:"
                 << "chainId:" << chainId
                 << "deviceIndex:" << deviceIndex
                 << "deviceId:" << deviceId;
    }

    // ✅ Первый опрос — включаем индикатор заголовка
    if (!m_firstPollReceived) {
        m_firstPollReceived = true;
        if (m_titleIndicator) {
            m_titleIndicator->setState(LedIndicator::State::Green);
            qDebug() << "[BaseDocWidget] Первый опрос получен, индикатор включён";
        }
    }
}


// ============================================================================
// ✅ ФИНАЛИЗАЦИЯ ЦИКЛА ОПРОСА
//
// Делает:
//   1. Обновляет цвет заголовка документа (синий/зелёный/оранжевый/красный).
//   2. Собирает суммарный список аварий по всем устройствам документа.
//   3. Эмитит сигнал alarmsUpdated для AlarmPanel в MainWindow,
//      ТОЛЬКО если список аварий изменился с прошлого раза.
// ============================================================================
void BaseDocWidget::finalizePollCycle()
{
    // 1. Обновляем цвет заголовка
    updateAlarmState();

    // 2. Собираем суммарный список аварий
    QList<int> alarmCodes;
    QList<int> alarmStatuses;
    {
        QReadLocker locker(&m_dataLock);
        for (auto it = m_deviceErrors.constBegin();
             it != m_deviceErrors.constEnd(); ++it) {
            for (int code : it.value()) {
                alarmCodes.append(code);
                alarmStatuses.append(AlarmRegistry::instance().getStatus(code));
            }
        }
    }

    // 3. Автоочистка подтверждений
    {
        QSet<int> currentCodes(alarmCodes.begin(), alarmCodes.end());
        for (auto it = m_acknowledgedAlarms.begin();
             it != m_acknowledgedAlarms.end(); ) {
            if (!currentCodes.contains(*it)) {
                it = m_acknowledgedAlarms.erase(it);
            } else {
                ++it;
            }
        }
    }

    // 4. Фильтрация
    QList<int> filteredCodes;
    QList<int> filteredStatuses;
    filteredCodes.reserve(alarmCodes.size());
    filteredStatuses.reserve(alarmStatuses.size());

    for (int i = 0; i < alarmCodes.size(); ++i) {
        if (!m_acknowledgedAlarms.contains(alarmCodes[i])) {
            filteredCodes.append(alarmCodes[i]);
            filteredStatuses.append(alarmStatuses[i]);
        }
    }

    // 5. Сортировка в currentPairs
    QList<QPair<int, int>> currentPairs;
    currentPairs.reserve(filteredCodes.size());
    for (int i = 0; i < filteredCodes.size(); ++i) {
        currentPairs.append(qMakePair(filteredCodes[i], filteredStatuses[i]));
    }
    std::sort(currentPairs.begin(), currentPairs.end(),
              [](const QPair<int,int> &a, const QPair<int,int> &b) {
                  if (a.first != b.first) return a.first < b.first;
                  return a.second < b.second;
              });

    // ========================================================================
    // ✅ РЕГИСТРАЦИЯ ОЗВУЧКИ — ВСЕГДА (каждый цикл).
    //    Плеер сам решит, играть ли прямо сейчас, или подождать,
    //    пока закончится текущая фраза. Если аварий нет — документ
    //    удалится из очереди плеера.
    // ========================================================================
    {
        const QString docBaseName = QFileInfo(m_filePath).completeBaseName();

        QStringList alarmTexts;
        alarmTexts.reserve(currentPairs.size());
        for (const auto &p : currentPairs) {
            alarmTexts.append(AlarmRegistry::instance().getMessage(p.first));
        }

        AlarmSoundPlayer::instance().registerDocAlarms(
            docBaseName,
            alarmTexts,
            m_alarmSoundEnabled);
    }

    // 6. Сравнение с кэшем — эмитим alarmsUpdated только при изменении
    bool changed = false;
    if (!m_alarmsCacheInitialized) {
        changed = true;
    } else if (currentPairs.size() != m_lastSentAlarmCodes.size()) {
        changed = true;
    } else {
        for (int i = 0; i < currentPairs.size(); ++i) {
            if (currentPairs[i].first != m_lastSentAlarmCodes[i] ||
                currentPairs[i].second != m_lastSentAlarmStatuses[i]) {
                changed = true;
                break;
            }
        }
    }

    if (!changed) {
        return;
    }

    // 7. Обновление кэша
    m_lastSentAlarmCodes.clear();
    m_lastSentAlarmStatuses.clear();
    m_lastSentAlarmCodes.reserve(currentPairs.size());
    m_lastSentAlarmStatuses.reserve(currentPairs.size());
    for (const auto &p : qAsConst(currentPairs)) {
        m_lastSentAlarmCodes.append(p.first);
        m_lastSentAlarmStatuses.append(p.second);
    }
    m_alarmsCacheInitialized = true;

    // 8. Эмит сигнала
    emit alarmsUpdated(m_filePath,
                       m_lastSentAlarmCodes,
                       m_lastSentAlarmStatuses,
                       lastCycleTime());
}



// ✅ НОВЫЙ МЕТОД: Обновление цвета заголовка на основе аварий
void BaseDocWidget::updateAlarmState()
{
    // 1. Проверяем наличие критических аварий (Status=1) от устройств
    bool hasCriticalAlarm = false;

    // ✅ ПОТОКОБЕЗОПАСНОЕ ЧТЕНИЕ: используем read lock
    {
        QReadLocker locker(&m_dataLock);

        for (const auto &data : m_lastDeviceData) {
            if (data && data->hasErrors()) {
                const DeviceErrors &errors = data->getErrors();
                for (int code : errors.codes) {
                    if (AlarmRegistry::instance().getStatus(code) == 1) {
                        hasCriticalAlarm = true;
                        break;
                    }
                }
            }
            if (hasCriticalAlarm) break;
        }
    }

    // 2. Проверяем наличие ошибок конфигурации
    bool hasConfigError = false;
    for (const auto &conn : m_equipmentConnections) {
        if (conn.errorCode != 0) {
            hasConfigError = true;
            break;
        }
    }

    // 3. Устанавливаем состояние с учётом приоритетов:
    //    Красный (авария) > Оранжевый (ошибка конфигурации) > Зелёный (в работе) > Синий (дежурный)
    if (hasCriticalAlarm) {
        setEquipmentState(3); // Красный (критическая авария от устройства)
    } else if (hasConfigError) {
        setEquipmentState(2); // Оранжевый (ошибка конфигурации)
    } else if (isInWork()) {
        setEquipmentState(1); // Зелёный (оборудование в работе)
    } else {
        setEquipmentState(0); // Синий (дежурный режим)
    }
}

// ✅ НОВЫЙ МЕТОД: Одноразовый триггер индикатора на 300 мс
void BaseDocWidget::triggerIndicatorBlink()
{
    if (!m_titleIndicator) return;

    // ✅ В Fast режиме цикл ~100 мс — триггер 100 мс,
    //    чтобы индикатор мигал, а не горел постоянно.
    //    В обычном — цикл ~1000 мс — триггер 300 мс.
    const int duration = isFastPolling() ? 100 : 300;
    m_titleIndicator->trigger(duration);
}

// ✅ НОВЫЙ МЕТОД: Включить/выключить режим быстрого опроса
void BaseDocWidget::setFastPolling(bool enabled)
{
    if (!m_pollScheduler) {
        qWarning() << "[BaseDocWidget] PollScheduler не создан, режим Fast недоступен";
        return;
    }

    m_pollScheduler->setFastMode(enabled);
    qDebug() << "[BaseDocWidget] Режим Fast" << (enabled ? "включён" : "выключен")
             << "для:" << filePath();
}

// ✅ НОВЫЙ МЕТОД: Проверить, включён ли режим быстрого опроса
bool BaseDocWidget::isFastPolling() const
{
    if (!m_pollScheduler) return false;
    return m_pollScheduler->isFastMode();
}




int BaseDocWidget::databaseDocTypeCode() const
{
    // Маппинг DocType → int код для БД (как в MFC)
    // 1 = Антенна, 2 = Передатчик, 3 = Энерго, 4 = Модулятор
    switch (docType()) {
        case DocType::Antenna:     return 1;
        case DocType::Transmitter: return 2;
        case DocType::Energo:      return 3;
        case DocType::Modulator:   return 4;
        default:                   return 0;
    }
}

void BaseDocWidget::ensureDatabaseTable()
{
    if (m_filePath.isEmpty()) return;

    int code = databaseDocTypeCode();
    if (code == 0) {
        qWarning() << "[DB] Неизвестный тип документа, таблица не создана.";
        return;
    }

    QString tableName = QFileInfo(m_filePath).completeBaseName();
    if (tableName.isEmpty()) return;

    qDebug() << "[DB] Попытка создания таблицы для файла:" << m_filePath
             << "docType =" << code;
    bool result = DatabaseManager::instance().ensureDocumentTableExists(m_filePath, code);
    if (result) {
        qDebug() << "[DB] Таблица для" << m_filePath << "готова к работе.";
    } else {
        qWarning() << "[DB] Не удалось создать таблицу для" << m_filePath;
    }
}

// ============================================================================
// ✅ НОВЫЙ МЕТОД: Открыть диалог просмотра графика
// ============================================================================
void BaseDocWidget::onViewDataGraph()
{
    // Определяем тип документа
    int docType = databaseDocTypeCode();
    if (docType == 0) {
        QMessageBox::warning(this, "Ошибка", "Неизвестный тип документа");
        return;
    }

    // ✅ Модальный диалог — создаём на стеке и вызываем exec()
    DataViewDialog dlg(m_filePath, docType, this);
    dlg.exec();
}

// ============================================================================
// ✅ ЕДИНАЯ проверка: пора ли писать в БД.
//
// Флаг выставляется, если сработала ХОТЯ БЫ ОДНА причина:
//   1) сработал таймер db_write_interval;
//   2) есть авария (ErrorsAll не пуст) и «окно аварийной записи»
//      (alarm_write_duration_min минут) ещё не истекло.
//
// Окно аварийной записи:
//   • открывается при первом появлении аварии (m_msSinceAlarmWindowStart = 0 → += polling),
//   • накапливает интервалы опроса, пока авария есть,
//   • сбрасывается при пропадании аварии,
//   • при истечении alarm_write_duration_min — окно перестаёт
//     возвращать true, но счётчик НЕ сбрасывается (иначе окно
//     переоткроется на следующем цикле, пока авария ещё висит).
// ============================================================================
bool BaseDocWidget::shouldWriteToDb(const QString &errorsAll)
{
    const QJsonObject settings = m_fullDocument.value("settings").toObject();
    int pollingInterval   = settings.value("polling_interval").toInt(1000);
    int dbWriteInterval   = settings.value("db_write_interval").toInt(5000);
    int alarmWriteMinutes = settings.value("alarm_write_duration_min").toInt(5);

    if (pollingInterval   < 1)    pollingInterval   = 1;
    if (dbWriteInterval   < 1)    dbWriteInterval   = 1;
    if (alarmWriteMinutes < 0)    alarmWriteMinutes = 0;
    if (alarmWriteMinutes > 1440) alarmWriteMinutes = 1440;

    // ------------------------------------------------------------------------
    // 1. Причина по таймеру
    // ------------------------------------------------------------------------
    bool byTimer = false;
    if (dbWriteInterval <= pollingInterval) {
        byTimer = true;
    } else {
        m_msSinceLastDbWrite += pollingInterval;
        if (m_msSinceLastDbWrite >= dbWriteInterval) {
            byTimer = true;
        }
    }

    // ------------------------------------------------------------------------
    // 2. Причина по аварии
    // ------------------------------------------------------------------------
    bool byAlarm = false;

    int alarmCount = 0;
    if (!errorsAll.isEmpty()) {
        const QStringList parts = errorsAll.split(';', Qt::SkipEmptyParts);
        if (!parts.isEmpty()) {
            bool ok = false;
            alarmCount = parts.first().toInt(&ok);
            if (!ok) alarmCount = 0;
        }
    }
    const bool hasAlarm = (alarmCount > 0);

    if (!hasAlarm) {
        // Аварии нет — окно закрыто.
        m_msSinceAlarmWindowStart = 0;
    } else {
        // Авария есть.
        const qint64 alarmWriteMs = static_cast<qint64>(alarmWriteMinutes) * 60 * 1000;

        if (m_msSinceAlarmWindowStart == 0) {
            // Окно ещё не открыто — открываем (записываем первый интервал).
            m_msSinceAlarmWindowStart = pollingInterval;
            byAlarm = true;
        } else {
            // Окно открыто — накапливаем интервал.
            m_msSinceAlarmWindowStart += pollingInterval;
            // Пишем, пока не истекло.
            // Счётчик НЕ сбрасываем при истечении — иначе окно
            // переоткроется на следующем цикле, пока авария ещё висит.
            byAlarm = (m_msSinceAlarmWindowStart < alarmWriteMs);
        }
    }

    // ------------------------------------------------------------------------
    // 3. Итог: пишем, если сработала любая причина
    // ------------------------------------------------------------------------
    const bool write = (byTimer || byAlarm);

    // Сбрасываем счётчик таймера, если пишем — «сдвигаем» момент
    // следующей записи по таймеру, чтобы не было двух подряд.
    if (write) {
        m_msSinceLastDbWrite = 0;
    }

    return write;
}

// ============================================================================
// Собирает строку ErrorsAll по ВСЕМ устройствам документа.
// Формат: "N;code1;code2;...;"  (совместим с MeasurementsModel)
// Пустой список → "0;"
// ============================================================================
QString BaseDocWidget::buildErrorsAllString() const
{
    // Собираем все коды со всех устройств
    QList<int> allCodes;

    {
        QReadLocker locker(&m_dataLock);
        for (auto it = m_deviceErrors.constBegin();
             it != m_deviceErrors.constEnd(); ++it) {
            allCodes.append(it.value());
        }
    }

    if (allCodes.isEmpty()) {
        return QStringLiteral("0;");
    }

    QStringList parts;
    parts.reserve(allCodes.size() + 1);
    parts << QString::number(allCodes.size());
    for (int code : allCodes) {
        parts << QString::number(code);
    }
    return parts.join(';') + ';';
}

QDateTime BaseDocWidget::lastCycleTime() const
{
    if (m_pollScheduler) {
        return m_pollScheduler->lastTimePoolTime();
    }
    return QDateTime();
}

void BaseDocWidget::onAlarmAcknowledged(int code)
{
    // 1. Добавляем в кэш.
    m_acknowledgedAlarms.insert(code);

    // 2. Запись в журнал действий пользователей.
    //    Журнал фиксирует ФАКТ подтверждения для аудита,
    //    даже если подтверждение не переживёт перезапуск.
    const QString docName = QFileInfo(m_filePath).completeBaseName();
    const QString alarmText = AlarmRegistry::instance().getMessage(code);

    DatabaseManager::instance().logUserAction(
        "Аварии",
        docName,
        QString("Подтверждена авария [%1]: %2")
            .arg(code)
            .arg(alarmText)
    );

    // 3. Форсируем пересборку списка аварий — это исключит подтверждённые
    //    из alarmsUpdated и уберёт панель, если аварий больше нет.
    m_lastSentAlarmCodes.clear();
    m_lastSentAlarmStatuses.clear();
    m_alarmsCacheInitialized = false;

    finalizePollCycle();
}
