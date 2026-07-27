#include "temonitorv6driver.h"

// Инициализация статических констант
const QString TEMonitorV6Driver::OID_BASE = ".1.3.6.1.4.1.17095";

const QString TEMonitorV6Driver::OID_TEMP[8] = {
    ".1.3.6.1.4.1.17095.1.2.1.0",
    ".1.3.6.1.4.1.17095.1.2.3.0",
    ".1.3.6.1.4.1.17095.1.2.5.0",
    ".1.3.6.1.4.1.17095.1.2.7.0",
    ".1.3.6.1.4.1.17095.1.2.9.0",
    ".1.3.6.1.4.1.17095.1.2.11.0",
    ".1.3.6.1.4.1.17095.1.2.13.0",
    ".1.3.6.1.4.1.17095.1.2.15.0"
};

const QString TEMonitorV6Driver::OID_ALARM_STATUS = ".1.3.6.1.4.1.17095.1.2.21.0";

TEMonitorV6Driver::TEMonitorV6Driver(int deviceId, QObject *parent)
    : IDeviceDriver(deviceId, parent)
    , m_port(161)
    , m_community("public")
{
    qDebug() << "[TEMonitorV6Driver] Создан драйвер для deviceId:" << deviceId;
}

TEMonitorV6Driver::~TEMonitorV6Driver()
{
    disconnect();
}

void TEMonitorV6Driver::setConnectionParams(const EquipmentConnection &conn)
{
    m_ip = conn.ipAddress;
    m_port = conn.port > 0 ? conn.port : 161;
    m_community = conn.snmpV1CommunityRead.isEmpty() ? "public" : conn.snmpV1CommunityRead;
    qDebug() << "[TEMonitorV6Driver] Параметры подключения:"
             << "IP=" << m_ip
             << "Port=" << m_port
             << "Community=" << m_community;
}

bool TEMonitorV6Driver::connect()
{
    if (m_ip.isEmpty()) {
        qWarning() << "[TEMonitorV6Driver] IP-адрес не указан!";
        return false;
    }

    // Проверяем доступность через GET первого OID температуры
    QVariant result;
    bool ok = m_snmp.get(m_ip, m_port, m_community, OID_TEMP[0], result, 2000);

    if (ok) {
        m_connected = true;
        emit connected(m_deviceId);
        qDebug() << "[TEMonitorV6Driver] Успешное подключение к" << m_ip
                 << "первое значение температуры:" << result;
    } else {
        m_connected = false;
        qWarning() << "[TEMonitorV6Driver] Ошибка подключения к" << m_ip
                   << "причина:" << m_snmp.lastError();
    }

    return m_connected;
}

void TEMonitorV6Driver::disconnect()
{
    m_connected = false;
    emit disconnected(m_deviceId);
    qDebug() << "[TEMonitorV6Driver] Отключено";
}

std::shared_ptr<IDeviceData> TEMonitorV6Driver::poll()
{
    if (!m_connected) {
        qWarning() << "[TEMonitorV6Driver] Устройство не подключено!";
        auto errorData = std::make_shared<TEMonitorV6Data>(m_deviceId);
        errorData->setConnectionError();
        return errorData;
    }

    auto data = std::make_shared<TEMonitorV6Data>(m_deviceId);
    bool allSuccess = true;

    // Опрос 8 температур
    for (int i = 0; i < 8; ++i) {
        QVariant tempResult;
        bool ok = m_snmp.get(m_ip, m_port, m_community, OID_TEMP[i], tempResult, 2000);
        if (ok) {
            // В MFC-коде: DataStr.Replace('.',','); atof((LPCTSTR)DataStr);
            // QVariant уже корректно парсит double, но на всякий случай
            // обрабатываем оба формата (с точкой и запятой)
            QString strValue = tempResult.toString();
            strValue.replace(',', '.');  // Нормализуем разделитель
            bool parseOk;
            double tempValue = strValue.toDouble(&parseOk);
            if (parseOk) {
                data->temperatures[i] = tempValue;
                qDebug() << "[TEMonitorV6Driver] Температура" << (i + 1)
                         << "=" << data->temperatures[i] << "°C";
            } else {
                qWarning() << "[TEMonitorV6Driver] Не удалось распарсить температуру"
                           << (i + 1) << "значение:" << strValue;
                allSuccess = false;
            }
        } else {
            qWarning() << "[TEMonitorV6Driver] Ошибка опроса температуры" << (i + 1)
                       << "причина:" << m_snmp.lastError();
            allSuccess = false;
        }
    }

    // Опрос битовой маски аварий
    QVariant alarmResult;
    bool ok = m_snmp.get(m_ip, m_port, m_community, OID_ALARM_STATUS, alarmResult, 2000);
    if (ok) {
        data->alarmStatus = alarmResult.toInt();
        qDebug() << "[TEMonitorV6Driver] Статус аварий (битовая маска):"
                 << data->alarmStatus
                 << "бинарное:" << QString::number(data->alarmStatus, 2);

        // Обработка битовой маски: бит i = датчик i+1
        // Если бит установлен (== 1), значит датчик неисправен
        for (int i = 0; i < 8; ++i) {
            if ((data->alarmStatus >> i) & 0x01) {
                data->errors.addCode(ALARM_CODE_BASE + i);
                qWarning() << "[TEMonitorV6Driver] Датчик температуры №" << (i + 1)
                           << "- неисправен (бит" << i << "установлен)";
            }
        }
    } else {
        qWarning() << "[TEMonitorV6Driver] Ошибка опроса статуса аварий"
                   << "причина:" << m_snmp.lastError();
        allSuccess = false;
    }

    if (!allSuccess) {
        data->errors.addCode(250);  // "Нет связи"
    }

    emit pollCompleted(m_deviceId, data);
    return data;
}

bool TEMonitorV6Driver::executeCommand(const ControlCommand &cmd)
{
    Q_UNUSED(cmd);
    qWarning() << "[TEMonitorV6Driver] Устройство не поддерживает команды управления";
    emit commandExecuted(m_deviceId, false, "Команды не поддерживаются");
    return false;
}
