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
#include <QReadWriteLock>  // ✅ НОВОЕ: для потокобезопасного доступа к данным
#include <QMap>              // ✅ Для QMap
#include <memory> // ✅ Для std::shared_ptr

#include "ledindicator.h"
#include "networkcontroller.h"
#include "deviceids.h"  // ✅ Коды типов оборудования
#include "idevicedriver.h"  // ✅ НОВОЕ: для ControlCommand и IDeviceData
#include "devicedata.h"      // ✅ Для IDeviceData
#include "alarmregistry.h"   // ✅ Для AlarmRegistry

class PollScheduler;  // ✅ Forward declaration вместо #include

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
    QString interface;            // Тип интерфейса: "TCP/IP", "SNMP", "Modbus TCP", "Modbus RTU", "RS-232", "RS-422", "RS-485"
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

    int errorCode = 4;

    EquipmentConnection()
        : connected(false),
          deviceId(ID_UNKNOWN),
          useStream(false),
          slowPool(false),
          interface("TCP/IP"),
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
    // ✅ НОВОЕ: Получить унифицированные данные температуры для указанного слота
    // slotIndex — индекс строки в m_equipmentConnections (например, 11 для "Температура")
    std::shared_ptr<TemperaturaData> getTemperatureDataForSlot(int slotIndex) const;

signals:
    void modifiedChanged(bool);
    // ✅ НОВЫЙ СИГНАЛ: Уведомление об изменении аварий в документе
    void alarmsUpdated(const QString &docPath, const QList<int> &alarmCodes, const QList<int> &alarmStatuses);

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

private:
    // ✅ ОБЪЯВЛЕНИЯ МЕТОДОВ ПРОВЕРКИ
    void loadAndValidateEquipmentConnections();
    void writeEquipmentErrorLog();
    // ✅ НОВОЕ: Конвертер "сырых" данных драйвера в унифицированную TemperaturaData
    std::shared_ptr<TemperaturaData> convertToTemperatureData(
        int deviceId,
        const std::shared_ptr<IDeviceData>& rawData) const;

private slots:
    void onDocNewConnection();
    void onDocClientReadyRead();
    void onDocClientDisconnected();
    void onDocHeartbeatTimeout();

    static QByteArray serializePacket(const QJsonObject &obj);
    static bool parsePacket(QByteArray &buffer, QJsonObject &outObj);
};

#endif // BASEDOCWIDGET_H
