#ifndef BOREAS3DRIVER_H
#define BOREAS3DRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include "basedocwidget.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>

// ============================================================================
// ✅ SNMP-ДРАЙВЕР ДЛЯ ДАТЧИКА ТЕМПЕРАТУРЫ BOREAS 3
// Протокол: SNMP v1
// 4 датчика температуры + 4 статуса
// Настройки (тип датчика, поправка, пределы) хранятся в m_settings["sensors"]
// (массив из 4 объектов, полученный из файла документа).
// ============================================================================
class Boreas3Driver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit Boreas3Driver(int deviceId, QObject *parent = nullptr);
    ~Boreas3Driver() override;

    bool connect() override;
    void disconnect() override;
    void setConnectionParams(const EquipmentConnection &conn) override;
    void setDeviceSettings(const QJsonObject &settings) override;
    std::shared_ptr<IDeviceData> poll() override;
    bool executeCommand(const ControlCommand &cmd) override;

private:
    SnmpClient m_snmp;
    QString m_ip;
    int m_port;
    QString m_community;

    // ✅ Настройки датчиков (из файла документа)
    QJsonObject m_settings;

    // Базовый OID
    static const QString OID_BASE;

    // OID для 4 температур
    static const QString OID_TEMP[4];

    // OID для 4 статусов аварий
    static const QString OID_ALARM[4];

    // Множитель для температур
    static constexpr double TEMP_MULTIPLIER = 0.0001;

    // ✅ Коды аварий для проверки пределов
    // (регистрируются в AlarmRegistry)
    static constexpr int ALARM_TOP_BASE    = 4500;  // 4500..4503
    static constexpr int ALARM_BOTTOM_BASE = 4510;  // 4510..4513

    // ✅ Вспомогательный метод: применить настройки датчиков к TemperaturaData
    void applySettingsAndCheckLimits(std::shared_ptr<TemperaturaData> &data);
};

#endif // BOREAS3DRIVER_H
