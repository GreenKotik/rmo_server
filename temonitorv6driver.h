#ifndef TEMONITORV6DRIVER_H
#define TEMONITORV6DRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include "basedocwidget.h"
#include <QDebug>

// ============================================================================
// ✅ SNMP-ДРАЙВЕР ДЛЯ ДАТЧИКА ТЕМПЕРАТУРЫ TE MONITOR V6
// Протокол: SNMP v1
// 8 датчиков температуры + битовая маска аварий (бит i = датчик i+1)
// Коды аварий: 633-640 (Неисправен датчик температуры №1-8)
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
    std::shared_ptr<IDeviceData> poll() override;
    bool executeCommand(const ControlCommand &cmd) override;

private:
    SnmpClient m_snmp;
    QString m_ip;
    int m_port;
    QString m_community;

    // Базовый OID
    static const QString OID_BASE;

    // OID для 8 температур
    static const QString OID_TEMP[8];

    // OID для битовой маски аварий
    static const QString OID_ALARM_STATUS;

    // Коды аварий для каждого датчика (бит i → код 633+i)
    static constexpr int ALARM_CODE_BASE = 633;
};

#endif // TEMONITORV6DRIVER_H
