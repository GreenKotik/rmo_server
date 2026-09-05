#ifndef TELETEHDRIVER_H
#define TELETEHDRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include <QDebug>

class TeletehDriver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit TeletehDriver(int deviceId, QObject *parent = nullptr);
    ~TeletehDriver() override;

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

    // TODO: Заменить на реальные OID из MIB-файла Teleteh
    static const QString OID_OUTPUT_POWER;
    static const QString OID_FREQUENCY;
    static const QString OID_STATUS;
};

#endif // TELETEHDRIVER_H
