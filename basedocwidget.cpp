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
#include "equipmentsettingsdialog.h"  // ✅ Для открытия диалога настройки
#include "pollscheduler.h"   // ✅ Для PollScheduler
#include "devicedata.h"      // ✅ Для IDeviceData
#include "alarmregistry.h"   // ✅ Для AlarmRegistry

BaseDocWidget::BaseDocWidget(QWidget *parent)
    : QWidget(parent),
      m_docServer(new QTcpServer(this)),
      m_docPort(0),
      m_docHeartbeatTimer(new QTimer(this))
{
    // ✅ Растягиваться и по горизонтали, и по вертикали
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(m_docHeartbeatTimer, &QTimer::timeout, this, &BaseDocWidget::onDocHeartbeatTimeout);
    m_docHeartbeatTimer->start(10000);

}

BaseDocWidget::~BaseDocWidget()
{
    qDebug() << "[BaseDocWidget] Деструктор НАЧАЛО:" << m_filePath;

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
    // ✅ Сбрасываем флаг первого опроса при загрузке документа
    m_firstPollReceived = false;

    // ✅ Выключаем индикатор при загрузке
    if (m_titleIndicator) {
        m_titleIndicator->setState(LedIndicator::State::Gray);
    }

    QFile file(filePath);
    if (!file.exists()) return false;

    if (file.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        m_fullDocument = doc.object();
        file.close();

        // ✅ 1. ЗАГРУЗКА И ПРОВЕРКА ПОДКЛЮЧЕНИЙ ОБОРУДОВАНИЯ
        loadAndValidateEquipmentConnections();

        // ✅ 2. НОВОЕ: Инициализация планировщика опроса (если ещё не создан)
        if (!m_pollScheduler) {
            m_pollScheduler = new PollScheduler(this, this);
            // Подключаем сигналы
            connect(m_pollScheduler, &PollScheduler::pollResult,
                    this, &BaseDocWidget::processPollResult);
            // ✅ НОВОЕ: Проксируем сигнал начала цикла
            connect(m_pollScheduler, &PollScheduler::cycleStarted,
                    this, &BaseDocWidget::pollCycleStarted);
            // ✅ Подключаем сигнал завершения цикла к обновлению UI
            connect(m_pollScheduler, &PollScheduler::allCyclesFinished,
                    this, &BaseDocWidget::updateUIFromBuffer,
                    Qt::QueuedConnection); // Гарантированно в главном потоке
            connect(m_pollScheduler, &PollScheduler::errorOccurred,
                    this, [this](int chainId, int deviceIndex, const QString &error) {
                        qWarning() << "[BaseDocWidget] Ошибка опроса:"
                                   << "chainId:" << chainId
                                   << "deviceIndex:" << deviceIndex
                                   << "error:" << error;
                    });
            qDebug() << "[BaseDocWidget] PollScheduler создан для:" << filePath;
        }

        // ✅ 3. НОВОЕ: Читаем интервал опроса из секции settings
        QJsonObject settings = m_fullDocument.value("settings").toObject();
        int pollingInterval = settings.value("polling_interval").toInt(1000);

        // ✅ Защита от слишком маленького интервала (минимум 100 мс)
        if (pollingInterval < 100) {
            qWarning() << "[BaseDocWidget] Интервал опроса слишком маленький ("
                       << pollingInterval << "мс), установлено 100 мс";
            pollingInterval = 100;
        }

        // ✅ Применяем интервал к планировщику
        m_pollScheduler->setPollingInterval(pollingInterval);
        qDebug() << "[BaseDocWidget] Интервал опроса:" << pollingInterval << "мс";

        // ✅ 4. Инициализируем цепочки и запускаем опрос
        m_pollScheduler->initializeChains();
        m_pollScheduler->start();

        // ✅ НОВОЕ: Включаем режим Fast для быстрого первоначального опроса
//        setFastPolling(true);

        // 5. Вызываем виртуальный метод для установки параметров
        setDocumentParameters(m_fullDocument.value("parameters").toObject());

        qDebug() << "[BaseDocWidget] Загружен файл и запущен опрос:" << filePath;

        // ✅ НОВОЕ: Автоматическое создание таблицы в БД
        ensureDatabaseTable();

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
        conn.interface = "TCP/IP";
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
        m_equipmentConnections[i].interface = connObj.value("interface").toString("TCP/IP");
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

    // ✅ Только один пункт — "Настройка параметров" (не требует авторизации)
    QAction *settingsAct = menu.addAction("⚙️ Настройка параметров");

    // Показываем меню в позиции клика
    QAction *selected = menu.exec(event->globalPos());

    // Обработка выбора
    if (selected == settingsAct) {
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

    // 1. Перечитываем файл, чтобы получить актуальные настройки
    QFile file(m_filePath);
    if (!file.exists() || !file.open(QIODevice::ReadOnly)) {
        qWarning() << "[BaseDocWidget] Не удалось открыть файл для перечитывания настроек";
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    m_fullDocument = doc.object();
    file.close();

    // 2. Читаем новый интервал опроса
    QJsonObject settings = m_fullDocument.value("settings").toObject();
    int newInterval = settings.value("polling_interval").toInt(1000);

    // Защита от слишком маленького интервала
    if (newInterval < 100) {
        qWarning() << "[BaseDocWidget] Интервал опроса слишком маленький ("
                   << newInterval << "мс), установлено 100 мс";
        newInterval = 100;
    }

    // 3. Останавливаем старый опрос
    m_pollScheduler->stop();

    // 4. Применяем новый интервал
    m_pollScheduler->setPollingInterval(newInterval);

    // 5. Пересоздаём цепочки (на случай, если изменились подключения)
    m_pollScheduler->initializeChains();

    // 6. Запускаем опрос заново
    m_pollScheduler->start();

    // 7. Обновляем состояние заголовка (на случай изменения флага "в работе")
    updateAlarmState();

    qDebug() << "[BaseDocWidget] Настройки опроса перезагружены. Новый интервал:"
             << newInterval << "мс";
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
    if (!data) return;

    int deviceId = data->deviceId();

    // ✅ ПОТОКОБЕЗОПАСНАЯ ЗАПИСЬ: используем write lock
    {
        QWriteLocker locker(&m_dataLock);
        m_lastDeviceData[deviceId] = data;
    }

    // ✅ Логируем только при ошибках
    if (data->hasErrors()) {
        qDebug() << "[BaseDocWidget] Получены данные с ошибками:"
                 << "chainId:" << chainId
                 << "deviceIndex:" << deviceIndex
                 << "deviceId:" << deviceId;
    }

    // ✅ При первом опросе включаем индикатор
    if (!m_firstPollReceived) {
        m_firstPollReceived = true;
        if (m_titleIndicator) {
            m_titleIndicator->setState(LedIndicator::State::Green);
            qDebug() << "[BaseDocWidget] Первый опрос получен, индикатор включён";
        }
    }

    // ✅ Мигание индикатора при каждом цикле опроса
    triggerIndicatorBlink();

    // ✅ Обновление состояния (цвет заголовка)
    updateAlarmState();

    // ✅ НОВОЕ: Формируем список аварий и отправляем сигнал в MainWindow
    QList<int> alarmCodes;
    QList<int> alarmStatuses;

    {
        QReadLocker locker(&m_dataLock);

        for (const auto &deviceData : m_lastDeviceData) {
            if (deviceData && deviceData->hasErrors()) {
                const DeviceErrors &errors = deviceData->getErrors();
                for (int code : errors.codes) {
                    alarmCodes.append(code);
                    alarmStatuses.append(AlarmRegistry::instance().getStatus(code));
                }
            }
        }
    }

    // ✅ Отправляем сигнал с полным списком аварий
    emit alarmsUpdated(m_filePath, alarmCodes, alarmStatuses);
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

    // ✅ Используем встроенный метод trigger() из LedIndicator
    // Он загорается текущим цветом и автоматически гаснет через 300 мс
    m_titleIndicator->trigger(300);
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

// ============================================================================
// ✅ НОВЫЙ МЕТОД: Конвертация "сырых" данных в унифицированную TemperaturaData
// ============================================================================
std::shared_ptr<TemperaturaData> BaseDocWidget::convertToTemperatureData(
    int deviceId,
    const std::shared_ptr<IDeviceData>& rawData) const
{
    if (!rawData) return nullptr;

    auto result = std::make_shared<TemperaturaData>(deviceId);

    // ✅ Случай 1: Данные от Boreas 3 (4 датчика)
    if (auto boreasData = std::dynamic_pointer_cast<Boreas3Data>(rawData)) {
        result->activeCount = 4;
        for (int i = 0; i < 4; ++i) {
            result->temperatures[i] = boreasData->temperatures[i];
            result->alarms[i] = boreasData->alarms[i];
        }
        // Копируем ошибки
        result->errors = boreasData->errors;
        return result;
    }

    // ✅ Случай 2: Данные от TE Monitor V6 (8 датчиков)
    if (auto teData = std::dynamic_pointer_cast<TEMonitorV6Data>(rawData)) {
        result->activeCount = 8;
        for (int i = 0; i < 8; ++i) {
            result->temperatures[i] = teData->temperatures[i];
            // Распаковываем битовую маску в массив статусов
            result->alarms[i] = (teData->alarmStatus >> i) & 0x01;
        }
        // Копируем ошибки
        result->errors = teData->errors;
        return result;
    }

    // ✅ Случай 3: Неизвестный тип драйвера
    qWarning() << "[BaseDocWidget] convertToTemperatureData: неизвестный тип данных для deviceId:" << deviceId;
    return nullptr;
}

// ============================================================================
// ✅ НОВЫЙ МЕТОД: Получить унифицированные данные температуры для слота
// ============================================================================
std::shared_ptr<TemperaturaData> BaseDocWidget::getTemperatureDataForSlot(int slotIndex) const
{
    // 1. Проверка корректности индекса
    if (slotIndex < 0 || slotIndex >= m_equipmentConnections.size()) {
        qWarning() << "[BaseDocWidget] getTemperatureDataForSlot: некорректный slotIndex:" << slotIndex;
        return nullptr;
    }

    // 2. Получаем deviceId из слота
    const EquipmentConnection &conn = m_equipmentConnections[slotIndex];
    if (conn.deviceId == ID_UNKNOWN) {
        return nullptr;  // Устройство не выбрано
    }

    // 3. Получаем "сырые" данные из буфера
    auto rawData = getDeviceData(conn.deviceId);
    if (!rawData) {
        return nullptr;  // Данные ещё не получены
    }

    // 4. Конвертируем в унифицированный формат
    return convertToTemperatureData(conn.deviceId, rawData);
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

    qDebug() << "[DB] Попытка создания таблицы:" << tableName
             << "для docType =" << code;

    bool result = DatabaseManager::instance().ensureDocumentTableExists(tableName, code);
    if (result) {
        qDebug() << "[DB] Таблица" << tableName << "готова к работе.";
    } else {
        qWarning() << "[DB] Не удалось создать таблицу" << tableName;
    }
}
