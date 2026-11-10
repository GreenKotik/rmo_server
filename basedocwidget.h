#ifndef BASEDOCWIDGET_H
#define BASEDOCWIDGET_H

#include <QWidget>
#include <QString>
#include <QTcpServer>
#include <QTcpSocket>
#include <QHash>
#include <QTimer>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDataStream>
#include <QList>
#include <QContextMenuEvent>
#include <QReadWriteLock>
#include <QMap>
#include <memory>
#include <QSet>
#include "ledindicator.h"
#include "networkcontroller.h"
#include "deviceids.h"  // ✅ Коды типов оборудования
#include "idevicedriver.h"  // ✅ НОВОЕ: для ControlCommand и IDeviceData
#include "devicedata.h"      // ✅ Для IDeviceData
#include "alarmregistry.h"   // ✅ Для AlarmRegistry

class PollScheduler;  // ✅ Forward declaration вместо #include
class DataViewDialog;  // ✅ Forward declaration

enum class DocType { Antenna, Transmitter, Modulator, Energo };

// ✅ СТРУКТУРА ДЛЯ ХРАНЕНИЯ ПАРАМЕТРОВ ПОДКЛЮЧЕНИЯ ОБОРУДОВАНИЯ
struct EquipmentConnection {
    QString name;
    bool connected;
    QString address;              // Адрес устройства (Slave ID для Modbus, адрес для RS-485/422)
    QString equipmentType;
    int deviceId = ID_UNKNOWN;
    bool useStream;
    bool slowPool = false;

    // === Сетевые параметры ===
    QString connectionInterface;  // ✅ БЫЛО: interface
    QString ipAddress;            // IP-адрес (для TCP/SNMP/Modbus TCP)
    int port;                     // Порт (для TCP/SNMP/Modbus TCP)

    // === SNMP параметры ===
    bool snmpV1Enabled;
    QString snmpV1CommunityRead;
    QString snmpV1CommunityWrite;
    bool snmpV3Enabled;
    QString snmpV3Username;
    QString snmpV3AuthProtocol;
    QString snmpV3AuthPassword;
    QString snmpV3PrivProtocol;
    QString snmpV3PrivPassword;

    // === Параметры последовательного интерфейса (для Modbus RTU, RS-232/422/485) ===
    QString comPort;              // COM-порт (например, "COM1", "/dev/ttyUSB0")
    int baudRate = 9600;          // Скорость передачи (9600, 19200, 38400, 57600, 115200)
    int dataBits = 8;             // Биты данных (7, 8)
    QString parity = "None";      // Чётность ("None", "Even", "Odd")
    int stopBits = 1;             // Стоп-биты (1, 2)

    // ✅ НОВОЕ: Специфичные настройки устройства из файла документа
    // (тип датчика, поправка, пределы и т.п.). Хранит произвольный JSON,
    // специфика определяется типом устройства (deviceId).
    QJsonObject deviceSettings;

    int errorCode = 4;

    EquipmentConnection()
        : connected(false),
          deviceId(ID_UNKNOWN),
          useStream(false),
          slowPool(false),
          connectionInterface("TCP/IP"),
          port(502),
          snmpV1Enabled(false),
          snmpV1CommunityRead("public"),
          snmpV1CommunityWrite("private"),
          snmpV3Enabled(false),
          snmpV3AuthProtocol("MD5"),
          snmpV3PrivProtocol("DES"),
          comPort(""),
          baudRate(9600),
          dataBits(8),
          parity("None"),
          stopBits(1),
          errorCode(4) {}
};

struct DocClientState {
    qint64 lastActivity = 0;
    qint64 pingSent = 0;
    bool pingPending = false;
};

class BaseDocWidget : public QWidget {
    Q_OBJECT
public:
    explicit BaseDocWidget(QWidget *parent = nullptr);
    virtual ~BaseDocWidget();  // ✅ Убираем = default, реализуем в .cpp
    virtual void loadFromFile(const QString &path) = 0;
    virtual void saveToFile() = 0;
    virtual DocType docType() const = 0;
    virtual QSize documentFixedSize() const = 0;
    virtual void processDocClientData(const QJsonObject &data) = 0;

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }
    bool isModified() const { return m_modified; }

    void initDocumentNetwork(AppMode mode, quint16 listenPort = 0,
                            const QString &connectTo = QString(), quint16 connectPort = 0);
    quint16 getDocPort() const { return m_docPort; }

    void setTitleIndicator(LedIndicator *indicator) { m_titleIndicator = indicator; }

    bool saveToFilePreserveSections(const QString &filePath);
    bool loadFromFilePreserveSections(const QString &filePath);

    virtual QJsonObject getDocumentParameters() const;
    virtual void setDocumentParameters(const QJsonObject &params);

    // ✅ НОВЫЕ МЕТОДЫ для работы с подключениями оборудования
    QList<EquipmentConnection> getEquipmentConnections() const { return m_equipmentConnections; }
    void setEquipmentConnections(const QList<EquipmentConnection> &connections) {
        m_equipmentConnections = connections;
    }
    // ✅ НОВЫЙ МЕТОД: Получить секцию settings из документа
    QJsonObject getSettings() const {
        return m_fullDocument.value("settings").toObject();
    }
    // ✅ Состояние документа: 0=синий, 1=зелёный, 2=оранжевый, 3=красный
    int getEquipmentState() const { return m_equipmentState; }
    void setEquipmentState(int state);

    // ✅ НОВЫЕ МЕТОДЫ для управления заголовком
    void setTitleBarWidget(QWidget *w) { m_titleBarWidget = w; }
    void setTitleBarState(int state);

    // ✅ НОВЫЕ МЕТОДЫ: Состояние "оборудование в работе"
    bool isInWork() const {
        return m_fullDocument.value("settings").toObject().value("in_work").toBool(false);
    }
    void setInWork(bool inWork) {
        QJsonObject settings = m_fullDocument.value("settings").toObject();
        settings["in_work"] = inWork;
        m_fullDocument["settings"] = settings;
    }
    // ✅ НОВЫЕ МЕТОДЫ для управления опросом оборудования
    void startPolling();
    void stopPolling();
    void sendCommandToDevice(const QString &deviceName, const ControlCommand &cmd);

    // ✅ НОВЫЙ МЕТОД: Включить/выключить режим быстрого опроса (интервал 100 мс)
    void setFastPolling(bool enabled);
    bool isFastPolling() const;

    // ✅ Виртуальный метод для обработки результатов опроса (переопределяется в наследниках)
    virtual void processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data);
    // ✅ НОВЫЙ МЕТОД: Перезагрузка настроек опроса из файла (вызывается после сохранения)
    void reloadPollingSettings();

    // ✅ НОВЫЙ МЕТОД: Потокобезопасное получение данных устройства
    std::shared_ptr<IDeviceData> getDeviceData(int deviceId) const {
        QReadLocker locker(&m_dataLock);
        return m_lastDeviceData.value(deviceId);
    }
    // Получить строку ErrorsAll по всем устройствам документа в формате "N;code1;code2;...;"
    QString buildErrorsAllString() const;
    // ✅ Цикловое время документа, зафиксированное Time Pool-цепочкой
    //    в рабочем потоке. Slow-пул на него не влияет.
    //    Возвращает невалидный QDateTime, если Time Pool ещё не отработал.
    //    Реализация — в basedocwidget.cpp, где pollscheduler.h доступен целиком.
    QDateTime lastCycleTime() const;
    // ✅ Управление звуком аварий (для подключения к кнопке в заголовке).
    void setAlarmSoundEnabled(bool enabled) {
        m_alarmSoundEnabled = enabled;
    }
    bool isAlarmSoundEnabled() const {
        return m_alarmSoundEnabled;
    }

signals:
    void modifiedChanged(bool);
    void alarmsUpdated(const QString &docPath,
                       const QList<int> &alarmCodes,
                       const QList<int> &alarmStatuses,
                       const QDateTime &cycleTime);   // ✅ добавить
    void pollCycleStarted();

protected:
    void setModified(bool m) {
        if (m_modified != m) {
            m_modified = m;
            emit modifiedChanged(m);
        }
    }
    void broadcastDocUpdate(const QJsonObject &payload);
    // ✅ НОВОЕ: Контекстное меню при правом клике в области документа
    void contextMenuEvent(QContextMenuEvent *event) override;

    QString m_filePath;
    QJsonObject m_fullDocument;
    bool m_modified = false;

    QTcpServer *m_docServer;
    QList<QTcpSocket*> m_docClients;
    quint16 m_docPort;
    QHash<QTcpSocket*, QByteArray> m_docBuffers;
    QHash<QTcpSocket*, DocClientState> m_docStates;
    QTimer *m_docHeartbeatTimer;
    LedIndicator *m_titleIndicator = nullptr;
    // ✅ Кэш последнего отправленного состояния аварий
    // Используется в finalizePollCycle, чтобы не эмитить alarmsUpdated,
    // если список не изменился (иначе панель аварий мигает на каждом тике).
    QList<int> m_lastSentAlarmCodes;
    QList<int> m_lastSentAlarmStatuses;
    bool m_alarmsCacheInitialized = false;

    // ✅ НОВЫЕ ПОЛЯ для контроля целостности конфигурации
    QList<EquipmentConnection> m_equipmentConnections;
    int m_equipmentState = 0; // ✅ 0=синий, 1=зелёный, 2=оранжевый, 3=красный
    QWidget *m_titleBarWidget = nullptr;
    // ✅ Планировщик опроса оборудования
    PollScheduler *m_pollScheduler = nullptr;

    // ✅ НОВЫЕ ПОЛЯ для хранения данных от опроса
    // ✅ НОВЫЕ ПОЛЯ для хранения данных от опроса
    QMap<int, std::shared_ptr<IDeviceData>> m_lastDeviceData;  // Последние данные от устройств
    bool m_firstPollReceived = false;  // ✅ НОВОЕ: флаг первого опроса

    // ✅ НОВОЕ: Блокировка для потокобезопасного доступа к m_lastDeviceData
    // ✅ mutable — позволяет использовать в const методах (стандартный паттерн для мьютексов)
    mutable QReadWriteLock m_dataLock;

    void updateAlarmState();      // Обновление цвета заголовка на основе аварий
    void triggerIndicatorBlink(); // Мигание индикатора на 300 мс

    // ✅ НОВЫЙ МЕТОД: Обновление UI из буфера после завершения цикла опроса
    virtual void updateUIFromBuffer() {
        // Базовая реализация пустая, переопределяется в наследниках (например, PeredatchikWidget)
    }
    // ✅ НОВЫЙ МЕТОД: Хук, вызываемый после перезагрузки настроек из файла.
    // Наследники могут переопределить, чтобы перечитать свои кэшированные
    // настройки (например, конфигурацию датчиков).
    virtual void onSettingsReloaded() {
        // Базовая реализация — пустая.
    }
    // ✅ НОВЫЙ МЕТОД: Хук, вызываемый в конце loadFromFilePreserveSections.
    // Наследники могут переопределить, чтобы загрузить кэшированные настройки
    // после чтения файла документа (независимо от того, кто вызвал загрузку).
    virtual void onFileLoaded() {
        // Базовая реализация — пустая.
    }
    // ✅ НОВОЕ: Автоматическое создание таблицы в БД при загрузке документа
    void ensureDatabaseTable();

    // ✅ НОВОЕ: Возвращает числовой код типа документа для БД
    virtual int databaseDocTypeCode() const;
    // ✅ Накопленные ошибки по каждому устройству: deviceId → коды
    // Обновляется в processPollResult(), читается при записи в БД.
    QMap<int, QList<int>> m_deviceErrors;
    // Подтверждённые пользователем аварии.
    //    Пока авария в этом множестве, она не попадает в alarmsUpdated
    //    и не озвучивается. Как только авария исчезает из устройства,
    //    она удаляется из множества — и при повторном появлении снова видна.
    QSet<int> m_acknowledgedAlarms;
    // ========================================================================
    // ✅ НОВОЕ: Накопление времени до записи в БД.
    //
    // Каждый цикл опроса к счётчику добавляется polling_interval.
    // Когда счётчик >= db_write_interval — устанавливается флаг записи
    // (в общей структуре документа), счётчик обнуляется.
    // Флаг уходит клиентам по сети вместе с данными — клиент по нему
    // тоже пишет в свою локальную БД.
    // ========================================================================
    int m_msSinceLastDbWrite = 0;

    // ✅ Счётчик времени с момента появления аварии.
    // Накопление интервалов опроса (polling_interval) на каждом цикле.
    //   0 — окно аварийной записи закрыто (аварии нет или оно истекло);
    //   >0 — идёт отсчёт.
    // Сброс — только при пропадании аварии (по ErrorsAll).
    int m_msSinceAlarmWindowStart = 0;

    // Вызывается каждый цикл опроса в updateUIFromBuffer() наследника.
    // errorsAll — строка "N;code1;code2;...".
    // Возвращает true, если в этом цикле нужно писать в БД.
    bool shouldWriteToDb(const QString &errorsAll);
    // ✅ НОВЫЙ МЕТОД: Финализация цикла опроса
    // Вызывается из updateUIFromBuffer() (или processDocClientData() на клиенте)
    // ПОСЛЕ того, как агрегированная структура данных полностью сформирована.
    // Делает:
    //   1. Обновляет цвет заголовка (updateAlarmState)
    //   2. Эмитит сигнал alarmsUpdated для панели аварий
    void finalizePollCycle();
    // Проигрыватель звуков аварий.
    bool m_alarmSoundEnabled = true;

private:
    // ОБЪЯВЛЕНИЯ МЕТОДОВ ПРОВЕРКИ
    void loadAndValidateEquipmentConnections();
    void writeEquipmentErrorLog();

protected slots:
    // Доступно наследникам (например, ModulatorWidget)
    void onViewDataGraph();

public slots:
    // Пользователь подтвердил аварию.
    void onAlarmAcknowledged(int code);

private slots:
    void onDocNewConnection();
    void onDocClientReadyRead();
    void onDocClientDisconnected();
    void onDocHeartbeatTimeout();

    static QByteArray serializePacket(const QJsonObject &obj);
    static bool parsePacket(QByteArray &buffer, QJsonObject &outObj);
};

#endif // BASEDOCWIDGET_H
