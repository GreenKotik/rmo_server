#include "teletehdriver.h"
#include "basedocwidget.h"

// TODO: Впишите сюда реальные OID
const QString TeletehDriver::OID_OUTPUT_POWER = ".1.3.6.1.4.1.99999.1.1.0"; // Пример
const QString TeletehDriver::OID_FREQUENCY    = ".1.3.6.1.4.1.99999.1.2.0"; // Пример
const QString TeletehDriver::OID_STATUS       = ".1.3.6.1.4.1.99999.1.3.0"; // Пример

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
    if (m_snmp.get(m_ip, m_port, m_community, OID_STATUS, result, 2000)) {
        m_connected = true;
        emit connected(m_deviceId);
        return true;
    }
    m_connected = false;
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
        auto errorData = std::make_shared<TeletehData>(m_deviceId);
        errorData->setConnectionError();
        return errorData;
    }

    auto data = std::make_shared<TeletehData>(m_deviceId);
    bool allSuccess = true;

    QVariant pwr, freq, stat;
    if (!m_snmp.get(m_ip, m_port, m_community, OID_OUTPUT_POWER, pwr, 2000)) allSuccess = false;
    if (!m_snmp.get(m_ip, m_port, m_community, OID_FREQUENCY, freq, 2000)) allSuccess = false;
    if (!m_snmp.get(m_ip, m_port, m_community, OID_STATUS, stat, 2000)) allSuccess = false;

    if (allSuccess) {
        data->outputPower = pwr.toDouble();
        data->frequency = freq.toDouble();
        data->status = stat.toInt();

        if (data->status != 0) {
            data->errors.addCode(1602); // TODO: Заменить на реальный код аварии Teleteh
        }
    } else {
        data->errors.addCode(250); // Нет связи
    }

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
