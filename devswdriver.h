#ifndef DEVSWSWITCHDRIVER_H
#define DEVSWSWITCHDRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include <QDebug>

class DevSwDriver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit DevSwDriver(int deviceId, QObject *parent = nullptr);
    ~DevSwDriver() override;

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

    // TODO: Заменить на реальные OID из MIB-файла переключателя
    static const QString OID_ACTIVE_PATH;
    static const QString OID_STATUS;
    static const QString OID_SET_PATH; // OID для SNMP SET
};

#endif // DEVSWSWITCHDRIVER_H
