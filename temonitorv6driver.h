#ifndef TEMONITORV6DRIVER_H
#define TEMONITORV6DRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include "basedocwidget.h"
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>

// ============================================================================
// ✅ SNMP-ДРАЙВЕР ДЛЯ ДАТЧИКА ТЕМПЕРАТУРЫ TE MONITOR V6
// Протокол: SNMP v1
// 4 датчика температуры + битовая маска аварий (бит i = датчик i+1)
// Настройки (тип датчика, поправка, пределы) хранятся в m_settings["sensors"]
// (массив из 4 объектов, полученный из файла документа).
// ============================================================================
class TEMonitorV6Driver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit TEMonitorV6Driver(int deviceId, QObject *parent = nullptr);
    ~TEMonitorV6Driver() override;

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

    // OID для битовой маски аварий
    static const QString OID_ALARM_STATUS;

    // ✅ Коды аварий для проверки пределов
    static constexpr int ALARM_TOP_BASE    = 4500;  // 4500..4503
    static constexpr int ALARM_BOTTOM_BASE = 4510;  // 4510..4513

    // ✅ Коды аварий для неисправности датчиков (бит i = код 633+i)
    static constexpr int ALARM_SENSOR_BASE = 633;

    // ✅ Вспомогательный метод: применить настройки датчиков к TemperaturaData
    void applySettingsAndCheckLimits(std::shared_ptr<TemperaturaData> &data);
};

#endif // TEMONITORV6DRIVER_H
