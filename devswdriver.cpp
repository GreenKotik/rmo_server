#include "devswdriver.h"
#include "basedocwidget.h"

// TODO: Впишите сюда реальные OID
const QString DevSwDriver::OID_ACTIVE_PATH = ".1.3.6.1.4.1.88888.1.1.0"; // Пример
const QString DevSwDriver::OID_STATUS      = ".1.3.6.1.4.1.88888.1.2.0"; // Пример
const QString DevSwDriver::OID_SET_PATH    = ".1.3.6.1.4.1.88888.1.3.0"; // Пример для SET

DevSwDriver::DevSwDriver(int deviceId, QObject *parent)
    : IDeviceDriver(deviceId, parent), m_port(161), m_community("public")
{
    qDebug() << "[DevSwDriver] Создан драйвер для deviceId:" << deviceId;
}

DevSwDriver::~DevSwDriver() { disconnect(); }

void DevSwDriver::setConnectionParams(const EquipmentConnection &conn)
{
    m_ip = conn.ipAddress;
    m_port = conn.port > 0 ? conn.port : 161;
    m_community = conn.snmpV1CommunityRead.isEmpty() ? "public" : conn.snmpV1CommunityRead;
}

bool DevSwDriver::connect()
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

void DevSwDriver::disconnect()
{
    m_connected = false;
    emit disconnected(m_deviceId);
}

std::shared_ptr<IDeviceData> DevSwDriver::poll()
{
    if (!m_connected) {
        auto errorData = std::make_shared<DevSwData>(m_deviceId);
        errorData->setConnectionError();
        return errorData;
    }

    auto data = std::make_shared<DevSwData>(m_deviceId);
    bool allSuccess = true;

    QVariant path, stat;
    if (!m_snmp.get(m_ip, m_port, m_community, OID_ACTIVE_PATH, path, 2000)) allSuccess = false;
    if (!m_snmp.get(m_ip, m_port, m_community, OID_STATUS, stat, 2000)) allSuccess = false;

    if (allSuccess) {
        data->activePath = path.toInt();
        data->status = stat.toInt();

        if (data->status != 0) {
            data->errors.addCode(1702); // TODO: Заменить на реальный код аварии Dev-SW
        }
    } else {
        data->errors.addCode(250); // Нет связи
    }

    emit pollCompleted(m_deviceId, data);
    return data;
}

bool DevSwDriver::executeCommand(const ControlCommand &cmd)
{
    // Пример: переключение пути (1 или 2)
/*    if (cmd.commandName == "switchPath") {
        int targetPath = cmd.value.toInt();
        if (targetPath == 1 || targetPath == 2) {
            QVariant newValue = targetPath;
            if (m_snmp.set(m_ip, m_port, m_community, OID_SET_PATH, newValue, 0x02, 2000)) { // 0x02 = INTEGER
                emit commandExecuted(m_deviceId, true, "Путь переключен на " + QString::number(targetPath));
                return true;
            }
        }
    }*/
    emit commandExecuted(m_deviceId, false, "Неверная команда или значение");
    return false;
}
