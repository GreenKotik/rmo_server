#ifndef BOREAS3DRIVER_H
#define BOREAS3DRIVER_H

#include "idevicedriver.h"
#include "devicedata.h"
#include "snmpclient.h"
#include "basedocwidget.h"
#include <QDebug>

class Boreas3Driver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit Boreas3Driver(int deviceId, QObject *parent = nullptr);
    ~Boreas3Driver() override;

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

    // Базовый OID для Boreas 3
    static const QString OID_BASE;

    // OID для температур (используются в getNext)
    static const QString OID_TEMP[4];

    // OID для статусов аварий
    static const QString OID_ALARM[4];

    // Множитель для температур
    static constexpr double TEMP_MULTIPLIER = 0.0001;
};

#endif // BOREAS3DRIVER_H
