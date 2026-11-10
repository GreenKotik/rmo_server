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
    //----------------------------------------------------
    static const QString OID_SERIANUMBER;
    static const QString OID_PRODUCTTYPE;
    static const QString OID_PRODUCTNAME;
    static const QString OID_TEMPERATURE;
    static const QString OID_ALARM_INPUT_ASI1_TSSYNCHROSTATUS;
    static const QString OID_ALARM_INPUT_TSOIP1_TSSYNCHROSTATUS;
    static const QString OID_ALARM_INPUT_ACTIVE_INFIFOOVERFLOWSTATUS;
    static const QString OID_CONFIG_PROCESSBAUDRATE;
    static const QString OID_MONITOR_INPUT_ASI1_TSSYNCHRO;
    static const QString OID_MONITOR_INPUT_ASI1_USERFULBITRATE;
    static const QString OID_MONITOR_INPUT_ASI1_TOTOLBITRATE;
    static const QString OID_MONITOR_INPUT_TSOIP1_TSSYNCHRO;
    static const QString OID_MONITOR_INPUT_TSOIP1_USERFULBITRATE;
    static const QString OID_MONITOR_INPUT_TSOIP1_TOTOLBITRATE;
    static const QString OID_MONITOR_INPUTACTIVE_ASI1_TSSYNCHRO;
    static const QString OID_MONITOR_INPUTACTIVE_ASI1_USERFULBITRATE;
    static const QString OID_MONITOR_INPUTACTIVE_ASI1_TOTOLBITRATE;
    static const QString OID_MONITOR_INPUTACTIVE_TSOIP1_USERFULBITRATE;
    static const QString OID_MONITOR_INPUTACTIVE_TSOIP1_TOTOLBITRATE;
    static const QString OID_MONITOR_INPUTACTIVE_TSOIP1_TSSYNCHRO;
    static const QString OID_MONITOR_PROCEESS_OUTPUT_SIGNALBANDWIDTH;
    static const QString OID_MONITOR_PROCESS_OUTPUT_CHANNELBITRATE;
    static const QString OID_OUTPUT_RF_IFBANDPOWER_LEVEL;
    static const QString OID_OUTPUT_RF_LBBANDPOWER_LEVEL;
    static const QString OID_OUTPUT_CONFIG_RF_LBBANDCARRIERFREQUENCY;
    static const QString OID_OUTPUT_CONFIG_RF_IFBANDCARRIERFREQUENCY;
    static const QString OID_OUTPUT_CONFIG_RF_MUTE;
    static const QString OID_OUTPUT_CONFIG_RF_UNMUTE;
    static const QString OID_OUTPUT_CONFIG_RF_MUTEATSTARTUP;
    static const QString OID_OUTPUT_CONFIG_RF_CARRIERMODULATED;
    static const QString OID_OUTPUT_CONFIG_RF_CUSTOMROLLOFFVALUE;
    static const QString OID_MONITOR_IP1_EHTERNETLOCK;
    static const QString OID_MONITOR_IP1_MPEGTSBITRATE;
    static const QString OID_MONITOR_RELAY_WARNINSTATE;
    static const QString OID_MONITOR_RELE_CRITSTATE;
    static const QString OID_MONITOR_REDANDANCY_STATUSGPIORELAY2;
    static const QString OID_MONITOR_REDANDANCY_STATUSGPIORELAY3;
    static const QString OID_MONITOR_TIME_CURRENTUTC;
    static const QString OID_MONITOR_POWERSUPPLY1_STATUS;
    static const QString OID_MONITOR_POWERSUPPLY2_STATUS;
    static const QString OID_MONITOR_FAN1_SPEED;
    static const QString OID_MONITOR_FAN2_SPEED;
    static const QString OID_MONITOR_FAN3_SPEED;
    static const QString OID_MONITOR_FAN4_SPEED;
    static const QString OID_MONITOR_FAN5_SPEED;
};

#endif // TELETEHDRIVER_H
