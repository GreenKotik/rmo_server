#ifndef IDEVICEDRIVER_H
#define IDEVICEDRIVER_H

#include <QObject>
#include <QString>
#include <QJsonObject>
#include <memory>
#include "devicedata.h"
#include "deviceids.h"

// ✅ Используем forward declaration вместо include, чтобы избежать циклических зависимостей
struct EquipmentConnection;

// ============================================================================
// ✅ СТРУКТУРА ДЛЯ КОМАНД УПРАВЛЕНИЯ
// ============================================================================
struct ControlCommand {
    QString commandId;           // Уникальный ID команды (QUuid)
    int deviceIndex;             // Индекс устройства в цепочке опроса
    int slotIndex = -1;          // стабильный адрес в m_equipmentConnections
    int commandCode;             // Код команды (из AlarmRegistry)
    QMap<QString, QVariant> params;  // Параметры команды
    int priority;                // 0=нормальный, 1=высокий
    QString issuedBy;            // Кто отправил команду
};

// ============================================================================
// ✅ БАЗОВЫЙ ИНТЕРФЕЙС ДЛЯ ВСЕХ ДРАЙВЕРОВ УСТРОЙСТВ
// ============================================================================
class IDeviceDriver : public QObject
{
    Q_OBJECT
public:
    explicit IDeviceDriver(int deviceId, QObject *parent = nullptr)
        : QObject(parent), m_deviceId(deviceId), m_connected(false) {}

    virtual ~IDeviceDriver() = default;

    // ✅ НОВОЕ: Виртуальный метод для передачи параметров подключения
    // По умолчанию ничего не делает — переопределяется в конкретных драйверах
    virtual void setConnectionParams(const EquipmentConnection &conn) {
        Q_UNUSED(conn);
    }
    // ✅ НОВОЕ: Виртуальный метод для передачи специфичных настроек устройства
    // (тип датчика, поправка, пределы и т.п.), полученных из файла документа.
    // По умолчанию ничего не делает — переопределяется в конкретных драйверах.
    virtual void setDeviceSettings(const QJsonObject &settings) {
        Q_UNUSED(settings);
    }
    // ✅ Методы подключения
    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const { return m_connected; }

    // ✅ Методы опроса
    virtual std::shared_ptr<IDeviceData> poll() = 0;

    // ✅ Методы управления
    virtual bool executeCommand(const ControlCommand &cmd) = 0;

    // ✅ Получить deviceId
    int deviceId() const { return m_deviceId; }

signals:
    // ✅ Сигналы для уведомления о событиях
    void connected(int deviceId);
    void disconnected(int deviceId);
    void pollCompleted(int deviceId, std::shared_ptr<IDeviceData> data);
    void commandExecuted(int deviceId, bool success, const QString &message);
    void errorOccurred(int deviceId, const QString &error);

protected:
    int m_deviceId;
    bool m_connected;
};

#endif // IDEVICEDRIVER_H
