#include "boreas3driver.h"

// Инициализация статических констант
const QString Boreas3Driver::OID_BASE = ".1.3.6.1.4.1.61858";

const QString Boreas3Driver::OID_TEMP[4] = {
    ".1.3.6.1.4.1.61858.5.1.1.2.0",
    ".1.3.6.1.4.1.61858.5.1.2.2.0",
    ".1.3.6.1.4.1.61858.5.1.3.2.0",
    ".1.3.6.1.4.1.61858.5.1.4.2.0"
};

const QString Boreas3Driver::OID_ALARM[4] = {
    ".1.3.6.1.4.1.61858.5.1.1.1.0",
    ".1.3.6.1.4.1.61858.5.1.2.1.0",
    ".1.3.6.1.4.1.61858.5.1.3.1.0",
    ".1.3.6.1.4.1.61858.5.1.4.1.0"
};

Boreas3Driver::Boreas3Driver(int deviceId, QObject *parent)
    : IDeviceDriver(deviceId, parent)
    , m_port(161)
    , m_community("public")
{
    qDebug() << "[Boreas3Driver] Создан драйвер для deviceId:" << deviceId;
}

Boreas3Driver::~Boreas3Driver()
{
    qDebug() << "[Boreas3Driver] Деструктор вызван для deviceId:" << m_deviceId;
    disconnect();
    qDebug() << "[Boreas3Driver] Деструктор завершён";
}

void Boreas3Driver::setConnectionParams(const EquipmentConnection &conn)
{
    m_ip = conn.ipAddress;
    m_port = conn.port > 0 ? conn.port : 161;
    m_community = conn.snmpV1CommunityRead.isEmpty() ? "public" : conn.snmpV1CommunityRead;
    qDebug() << "[Boreas3Driver] Параметры подключения:"
             << "IP=" << m_ip
             << "Port=" << m_port
             << "Community=" << m_community;
}

bool Boreas3Driver::connect()
{
    if (m_ip.isEmpty()) {
        qWarning() << "[Boreas3Driver] IP-адрес не указан!";
        return false;
    }

    // ✅ Используем GET по конкретному OID (как в MFC-коде)
    QVariant result;
    bool ok = m_snmp.get(m_ip, m_port, m_community, OID_TEMP[0], result, 2000);

    if (ok) {
        m_connected = true;
        emit connected(m_deviceId);
        qDebug() << "[Boreas3Driver] Успешное подключение к" << m_ip
                 << "первое значение температуры:" << result;
    } else {
        m_connected = false;
        qWarning() << "[Boreas3Driver] Ошибка подключения к" << m_ip
                   << "последняя ошибка SnmpClient:" << m_snmp.lastError();
    }

    return m_connected;
}

void Boreas3Driver::disconnect()
{
    m_connected = false;
    emit disconnected(m_deviceId);
    qDebug() << "[Boreas3Driver] Отключено";
}

std::shared_ptr<IDeviceData> Boreas3Driver::poll()
{
    // ✅ Проверка состояния перед опросом
    if (!m_connected || m_ip.isEmpty()) {
        qWarning() << "[Boreas3Driver] Устройство не подключено или IP не указан!";
        auto errorData = std::make_shared<Boreas3Data>(m_deviceId);
        errorData->errors.addCode(250);
        return errorData;
    }

    auto data = std::make_shared<Boreas3Data>(m_deviceId);
    bool allSuccess = true;

    // ✅ Опрос 4 температур через GET по конкретному OID
    for (int i = 0; i < 4; ++i) {
        // ✅ Проверка состояния перед каждым запросом
        if (!m_connected) {
            qWarning() << "[Boreas3Driver] Подключение потеряно во время опроса";
            data->errors.addCode(250);
            return data;
        }

        QVariant tempResult;
        bool ok = m_snmp.get(m_ip, m_port, m_community, OID_TEMP[i], tempResult, 2000);
        if (ok) {
            data->temperatures[i] = tempResult.toDouble() * TEMP_MULTIPLIER;
            qDebug() << "[Boreas3Driver] Температура" << (i + 1)
                     << "=" << data->temperatures[i] << "°C"
                     << "(сырое:" << tempResult.toDouble() << ")";
        } else {
            qWarning() << "[Boreas3Driver] Ошибка опроса температуры" << (i + 1);
            allSuccess = false;
        }
    }

    // ✅ Опрос 4 статусов аварий через GET по конкретному OID
    for (int i = 0; i < 4; ++i) {
        // ✅ Проверка состояния перед каждым запросом
        if (!m_connected) {
            qWarning() << "[Boreas3Driver] Подключение потеряно во время опроса";
            data->errors.addCode(250);
            return data;
        }

        QVariant alarmResult;
        bool ok = m_snmp.get(m_ip, m_port, m_community, OID_ALARM[i], alarmResult, 2000);
        if (ok) {
            int status = alarmResult.toInt();
            data->alarms[i] = status;
            qDebug() << "[Boreas3Driver] Статус датчика" << (i + 1) << "=" << status;

            if (status != 1) {
                data->errors.addCode(633 + i);
                qWarning() << "[Boreas3Driver] Датчик" << (i + 1)
                           << "- неисправен (статус:" << status << ")";
            }
        } else {
            qWarning() << "[Boreas3Driver] Ошибка опроса статуса" << (i + 1);
            allSuccess = false;
        }
    }

    if (!allSuccess) {
        data->errors.addCode(250);
    }

    emit pollCompleted(m_deviceId, data);
    return data;
}

bool Boreas3Driver::executeCommand(const ControlCommand &cmd)
{
    Q_UNUSED(cmd);
    qWarning() << "[Boreas3Driver] Устройство не поддерживает команды управления";
    emit commandExecuted(m_deviceId, false, "Команды не поддерживаются");
    return false;
}
