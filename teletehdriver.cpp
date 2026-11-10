#include "teletehdriver.h"
#include "basedocwidget.h"

// TODO: Впишите сюда реальные OID
const QString TeletehDriver::OID_OUTPUT_POWER = ".1.3.6.1.4.1.34186.2.1.3.0"; // Пример
const QString TeletehDriver::OID_FREQUENCY    = ".1.3.6.1.4.1.34186.26.1.3.1.2.1.0"; // Пример
const QString TeletehDriver::OID_STATUS       = ".1.3.6.1.4.1.34186.2.1.3.0"; // Пример
//-----------------------------------------------------------------------------------
const QString TeletehDriver::OID_SERIANUMBER = ".1.3.6.1.4.1.34186.2.1.1.0"; // Serianumber
const QString TeletehDriver::OID_PRODUCTTYPE = ".1.3.6.1.4.1.34186.2.1.2.0"; // PRODUCTTYPE
const QString TeletehDriver::OID_PRODUCTNAME = ".1.3.6.1.4.1.34186.2.1.3.0"; // PRODUCTNAME
const QString TeletehDriver::OID_TEMPERATURE = ".1.3.6.1.4.1.34186.2.1.3.0"; // TEMPERATURE
const QString TeletehDriver::OID_ALARM_INPUT_ASI1_TSSYNCHROSTATUS = ".1.3.6.1.4.1.34186"; //
const QString TeletehDriver::OID_ALARM_INPUT_TSOIP1_TSSYNCHROSTATUS = ".1.3.6.1.4.1.34186"; //
const QString TeletehDriver::OID_ALARM_INPUT_ACTIVE_INFIFOOVERFLOWSTATUS = ".1.3.6.1.4.1.34186"; //
const QString TeletehDriver::OID_CONFIG_PROCESSBAUDRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUT_ASI1_TSSYNCHRO = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUT_ASI1_USERFULBITRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUT_ASI1_TOTOLBITRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUT_TSOIP1_TSSYNCHRO = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUT_TSOIP1_USERFULBITRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUT_TSOIP1_TOTOLBITRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_INPUTACTIVE_ASI1_TSSYNCHRO = ".1.3.6.1.4.1.34186."; //проверить актуальностль OID
const QString TeletehDriver::OID_MONITOR_INPUTACTIVE_ASI1_USERFULBITRATE = ".1.3.6.1.4.1.34186."; //проверить актуальностль OID
const QString TeletehDriver::OID_MONITOR_INPUTACTIVE_ASI1_TOTOLBITRATE = ".1.3.6.1.4.1.34186."; //проверить актуальностль OID
const QString TeletehDriver::OID_MONITOR_INPUTACTIVE_TSOIP1_USERFULBITRATE = ".1.3.6.1.4.1.34186."; //проверить актуальностль OID
const QString TeletehDriver::OID_MONITOR_INPUTACTIVE_TSOIP1_TOTOLBITRATE = ".1.3.6.1.4.1.34186."; //проверить актуальностль OID
const QString TeletehDriver::OID_MONITOR_INPUTACTIVE_TSOIP1_TSSYNCHRO = ".1.3.6.1.4.1.34186."; //проверить актуальностль OID
const QString TeletehDriver::OID_MONITOR_PROCEESS_OUTPUT_SIGNALBANDWIDTH = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_PROCESS_OUTPUT_CHANNELBITRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_RF_IFBANDPOWER_LEVEL = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_RF_LBBANDPOWER_LEVEL = ".1.3.6.1.4.1.34186.26.1.3.1.2.1.0"; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_LBBANDCARRIERFREQUENCY = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_IFBANDCARRIERFREQUENCY = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_MUTE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_UNMUTE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_MUTEATSTARTUP = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_CARRIERMODULATED = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_OUTPUT_CONFIG_RF_CUSTOMROLLOFFVALUE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_IP1_EHTERNETLOCK = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_IP1_MPEGTSBITRATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_RELAY_WARNINSTATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_RELE_CRITSTATE = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_REDANDANCY_STATUSGPIORELAY2 = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_REDANDANCY_STATUSGPIORELAY3 = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_TIME_CURRENTUTC = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_POWERSUPPLY1_STATUS = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_POWERSUPPLY2_STATUS = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_FAN1_SPEED = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_FAN2_SPEED = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_FAN3_SPEED = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_FAN4_SPEED = ".1.3.6.1.4.1.34186."; //
const QString TeletehDriver::OID_MONITOR_FAN5_SPEED = ".1.3.6.1.4.1.34186."; //

TeletehDriver::TeletehDriver(int deviceId, QObject *parent)
    : IDeviceDriver(deviceId, parent), m_port(161), m_community("public")
{
    qDebug() << "[TeletehDriver] Создан драйвер для deviceId:" << deviceId;
}

TeletehDriver::~TeletehDriver() { disconnect(); }

void TeletehDriver::setConnectionParams(const EquipmentConnection &conn)
{
    m_ip = conn.ipAddress;
    m_port = conn.port > 0 ? conn.port : 161;
    m_community = conn.snmpV1CommunityRead.isEmpty() ? "public" : conn.snmpV1CommunityRead;
}

bool TeletehDriver::connect()
{
    if (m_ip.isEmpty()) return false;
    QVariant result;
    if (m_snmp.get(m_ip, m_port, m_community, OID_PRODUCTNAME, result, 2000)) {
        m_connected = true;
        emit connected(m_deviceId);
        qDebug() << "[TeletehDriver] Подключено к" << m_ip
                 << "ProductName:" << result.toString();
        return true;
    }
    m_connected = false;
    qWarning() << "[TeletehDriver] Ошибка подключения к" << m_ip
               << "причина:" << m_snmp.lastError();
    return false;
}

void TeletehDriver::disconnect()
{
    m_connected = false;
    emit disconnected(m_deviceId);
}

std::shared_ptr<IDeviceData> TeletehDriver::poll()
{
    if (!m_connected) {
        auto errorData = std::make_shared<ModulatorData>(m_deviceId);
        errorData->setConnectionError();
        return errorData;
    }

    auto data = std::make_shared<ModulatorData>(m_deviceId);
    bool allSuccess = true;

    // Лямбда для чтения одного OID
    auto readString = [&](const QString &oid, QString &out) {
        QVariant v;
        if (m_snmp.get(m_ip, m_port, m_community, oid, v, 2000)) {
            out = v.toString();
        } else {
            allSuccess = false;
        }
    };

    auto readInt = [&](const QString &oid, int &out) {
        QVariant v;
        if (m_snmp.get(m_ip, m_port, m_community, oid, v, 2000)) {
            out = v.toInt();
        } else {
            allSuccess = false;
        }
    };

    auto readDouble = [&](const QString &oid, double &out) {
        QVariant v;
        if (m_snmp.get(m_ip, m_port, m_community, oid, v, 2000)) {
            out = v.toDouble();
        } else {
            allSuccess = false;
        }
    };

    // --- Общие ---
    readString(OID_SERIANUMBER, data->serianumber);
    readString(OID_PRODUCTTYPE, data->productType);
    readString(OID_PRODUCTNAME, data->productName);
    readDouble(OID_TEMPERATURE, data->temperature);
    // --- Alarm ---
    readInt(OID_ALARM_INPUT_ASI1_TSSYNCHROSTATUS, data->alarmInputAsi1TsSynchroStatus);
    readInt(OID_ALARM_INPUT_TSOIP1_TSSYNCHROSTATUS, data->alarmInputTSoIp1TsSynchroStatus);
    readInt(OID_ALARM_INPUT_ACTIVE_INFIFOOVERFLOWSTATUS, data->alarmInputActiveInFifoOverflowStatus);
    // --- Config ---
    readInt(OID_CONFIG_PROCESSBAUDRATE, data->configProcessBaudrate);
    // --- Monitor текущее ---
    readInt(OID_MONITOR_INPUT_ASI1_TSSYNCHRO, data->monitorInputAsi1TsSynchro);
    readDouble(OID_MONITOR_INPUT_ASI1_USERFULBITRATE, data->monitorInputAsi1UserfulBitrate);
    readDouble(OID_MONITOR_INPUT_ASI1_TOTOLBITRATE, data->monitorInputAsi1TotalBitrate);
    readInt(OID_MONITOR_INPUT_TSOIP1_TSSYNCHRO, data->monitorInputTSoIp1TsSynchro);
    readDouble(OID_MONITOR_INPUT_TSOIP1_USERFULBITRATE, data->monitorInputTSoIp1UserfulBitrate);
    readDouble(OID_MONITOR_INPUT_TSOIP1_TOTOLBITRATE, data->monitorInputTSoIp1TotalBitrate);
    // --- Monitor активный ---
    readInt(OID_MONITOR_INPUTACTIVE_ASI1_TSSYNCHRO, data->monitorInputActiveAsi1TsSynchro);
    readDouble(OID_MONITOR_INPUTACTIVE_ASI1_USERFULBITRATE, data->monitorInputActiveAsi1UserfulBitrate);
    readDouble(OID_MONITOR_INPUTACTIVE_ASI1_TOTOLBITRATE, data->monitorInputActiveAsi1TotalBitrate);
    readDouble(OID_MONITOR_INPUTACTIVE_TSOIP1_USERFULBITRATE, data->monitorInputActiveTSoIp1UserfulBitrate);
    readDouble(OID_MONITOR_INPUTACTIVE_TSOIP1_TOTOLBITRATE, data->monitorInputActiveTSoIp1TotalBitrate);
    readInt(OID_MONITOR_INPUTACTIVE_TSOIP1_TSSYNCHRO, data->monitorInputActiveTSoIp1TsSynchro);
    // --- Monitor выход ---
    readDouble(OID_MONITOR_PROCEESS_OUTPUT_SIGNALBANDWIDTH, data->monitorProcessOutputSignalBandwidth);
    readDouble(OID_MONITOR_PROCESS_OUTPUT_CHANNELBITRATE, data->monitorProcessOutputChannelBitrate);
    // --- Output RF ---
    readDouble(OID_OUTPUT_RF_IFBANDPOWER_LEVEL, data->outputRfIfBandPowerLevel);
    readDouble(OID_OUTPUT_RF_LBBANDPOWER_LEVEL, data->outputRfLbBandPowerLevel);
    data->outputRfLbBandPowerLevel /= 10;
    readDouble(OID_OUTPUT_CONFIG_RF_LBBANDCARRIERFREQUENCY, data->outputConfigRfLbBandCarrierFrequency);
    readDouble(OID_OUTPUT_CONFIG_RF_IFBANDCARRIERFREQUENCY, data->outputConfigRfIfBandCarrierFrequency);
    readInt(OID_OUTPUT_CONFIG_RF_MUTE, data->outputConfigRfMute);
    readInt(OID_OUTPUT_CONFIG_RF_UNMUTE, data->outputConfigRfUnmute);
    readInt(OID_OUTPUT_CONFIG_RF_MUTEATSTARTUP, data->outputConfigRfMuteAtStartup);
    readInt(OID_OUTPUT_CONFIG_RF_CARRIERMODULATED, data->outputConfigRfCarrierModulated);
    readDouble(OID_OUTPUT_CONFIG_RF_CUSTOMROLLOFFVALUE, data->outputConfigRfCustomRolloffValue);
    // --- Monitor IP ---
    readInt(OID_MONITOR_IP1_EHTERNETLOCK, data->monitorIp1EthernetLock);
    readDouble(OID_MONITOR_IP1_MPEGTSBITRATE, data->monitorIp1MpegTsBitrate);
    // --- Relay ---
    readInt(OID_MONITOR_RELAY_WARNINSTATE, data->monitorRelayWarnInState);
    readInt(OID_MONITOR_RELE_CRITSTATE, data->monitorRelayCritState);
    // --- Redundancy ---
    readInt(OID_MONITOR_REDANDANCY_STATUSGPIORELAY2, data->monitorRedundancyStatusGpioRelay2);
    readInt(OID_MONITOR_REDANDANCY_STATUSGPIORELAY3, data->monitorRedundancyStatusGpioRelay3);
    // --- Time ---
    readDouble(OID_MONITOR_TIME_CURRENTUTC, data->monitorTimeCurrentUtc);
    // --- Power supply ---
    readInt(OID_MONITOR_POWERSUPPLY1_STATUS, data->monitorPowerSupply1Status);
    readInt(OID_MONITOR_POWERSUPPLY2_STATUS, data->monitorPowerSupply2Status);
    // --- Fans ---
    readDouble(OID_MONITOR_FAN1_SPEED, data->monitorFan1Speed);
    readDouble(OID_MONITOR_FAN2_SPEED, data->monitorFan2Speed);
    readDouble(OID_MONITOR_FAN3_SPEED, data->monitorFan3Speed);
    readDouble(OID_MONITOR_FAN4_SPEED, data->monitorFan4Speed);
    readDouble(OID_MONITOR_FAN5_SPEED, data->monitorFan5Speed);

    // --- Общая ошибка связи ---
    if (!allSuccess) {
        data->errors.addCode(250);
    }

    // --- Проверка аварий по Relay ---
    // (если WarnInState / CritState не 0 — добавляем код аварии)
    if (data->monitorRelayWarnInState != 0 || data->monitorRelayCritState != 0) {
        data->errors.addCode(1602);  // TODO: заменить на реальный код аварии Teleteh
    }

    qDebug() << "[TeletehDriver] Опрос завершён:"
             << "productName=" << data->productName
             << "temperature=" << data->temperature
             << "outputRfLbBandPowerLevel=" << data->outputRfLbBandPowerLevel
             << "monitorRelayWarnInState=" << data->monitorRelayWarnInState
             << "monitorRelayCritState=" << data->monitorRelayCritState;

    emit pollCompleted(m_deviceId, data);
    return data;
}

bool TeletehDriver::executeCommand(const ControlCommand &cmd)
{
    // Пример: изменение частоты (если поддерживается)
/*    if (cmd.commandName == "setFrequency") {
        QVariant newValue = QString::number(cmd.value.toDouble());
        if (m_snmp.set(m_ip, m_port, m_community, OID_FREQUENCY, newValue, 0x04, 2000)) { // 0x04 = OCTET STRING
            emit commandExecuted(m_deviceId, true, "Частота изменена");
            return true;
        }
    }*/
    emit commandExecuted(m_deviceId, false, "Команда не поддерживается");
    return false;
}
