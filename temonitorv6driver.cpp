#include "temonitorv6driver.h"
#include <cmath>

// ============================================================================
// ✅ OID-ы для TE Monitor V6
// ============================================================================
const QString TEMonitorV6Driver::OID_BASE = ".1.3.6.1.4.1.17095";

// ✅ Только 4 датчика: 1, 2 (канал 1) и 5, 6 (канал 2)
const QString TEMonitorV6Driver::OID_TEMP[4] = {
    ".1.3.6.1.4.1.17095.1.2.1.0",    // датчик 1 (канал 1, слот 1)
    ".1.3.6.1.4.1.17095.1.2.3.0",    // датчик 2 (канал 1, слот 2)
    ".1.3.6.1.4.1.17095.1.2.9.0",    // датчик 5 (канал 2, слот 1)
    ".1.3.6.1.4.1.17095.1.2.11.0"    // датчик 6 (канал 2, слот 2)
};

// ✅ Один OID для битовой маски аварий
const QString TEMonitorV6Driver::OID_ALARM_STATUS = ".1.3.6.1.4.1.17095.1.2.21.0";

// ============================================================================
// КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================

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

// ============================================================================
// ПАРАМЕТРЫ ПОДКЛЮЧЕНИЯ И НАСТРОЙКИ
// ============================================================================

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

void TEMonitorV6Driver::setDeviceSettings(const QJsonObject &settings)
{
    m_settings = settings;

    QJsonArray sensors = m_settings.value("sensors").toArray();
    qDebug() << "[TEMonitorV6Driver] setDeviceSettings: датчиков =" << sensors.size();
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ
// ============================================================================

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
                 << "первое значение:" << result;
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

// ============================================================================
// ОПРОС
// ============================================================================

std::shared_ptr<IDeviceData> TEMonitorV6Driver::poll()
{
    if (!m_connected) {
        qWarning() << "[TEMonitorV6Driver] Устройство не подключено!";
        auto errorData = std::make_shared<TemperaturaData>(m_deviceId);
        errorData->setConnectionError();
        return errorData;
    }

    auto data = std::make_shared<TemperaturaData>(m_deviceId);

    QJsonArray sensors = m_settings.value("sensors").toArray();

    bool anyRequestFailed = false;

    // ------------------------------------------------------------------------
    // Опрос 4 датчиков — только активных
    // ------------------------------------------------------------------------
    for (int i = 0; i < 4; ++i) {
        if (!m_connected) {
            qWarning() << "[TEMonitorV6Driver] Подключение потеряно во время опроса";
            data->errors.addCode(250);
            return data;
        }

        // Определяем тип датчика
        QString sensorType = "Нет";
        if (i < sensors.size()) {
            QJsonObject sensorObj = sensors[i].toObject();
            sensorType = sensorObj.value("type").toString("Нет");
        }

        // ✅ Если тип == "Нет" — пропускаем датчик
        if (sensorType == "Нет") {
            data->temperatures[i] = qQNaN();
            data->alarms[i] = 0;
            continue;
        }

        // Читаем температуру
        QVariant tempResult;
        bool ok = m_snmp.get(m_ip, m_port, m_community,
                             OID_TEMP[i], tempResult, 2000);
        if (!ok) {
            qWarning() << "[TEMonitorV6Driver] Ошибка опроса температуры" << (i + 1)
                       << "причина:" << m_snmp.lastError();
            anyRequestFailed = true;
            data->temperatures[i] = qQNaN();
            continue;
        }

        // ✅ Обработка температуры (может прийти с запятой вместо точки)
        QString strValue = tempResult.toString();
        strValue.replace(',', '.');
        bool parseOk;
        double tempValue = strValue.toDouble(&parseOk);
        if (parseOk) {
            data->temperatures[i] = tempValue;
            qDebug() << "[TEMonitorV6Driver] Датчик" << (i + 1)
                     << "тип:" << sensorType
                     << "температура:" << tempValue;
        } else {
            qWarning() << "[TEMonitorV6Driver] Не удалось распарсить температуру"
                       << (i + 1) << "значение:" << strValue;
            anyRequestFailed = true;
            data->temperatures[i] = qQNaN();
        }
    }

    // ------------------------------------------------------------------------
    // Опрос битовой маски аварий (один запрос на все датчики)
    // ------------------------------------------------------------------------
    QVariant alarmResult;
    bool alarmOk = m_snmp.get(m_ip, m_port, m_community,
                              OID_ALARM_STATUS, alarmResult, 2000);
    if (alarmOk) {
        int alarmStatus = alarmResult.toInt();
        qDebug() << "[TEMonitorV6Driver] Битовая маска аварий:"
                 << alarmStatus
                 << "бинарное:" << QString::number(alarmStatus, 2);

        // ✅ Соответствие между слотами в нашей структуре и битами маски:
        //    слот 0 (наш датчик 1)  → бит 0
        //    слот 1 (наш датчик 2)  → бит 1
        //    слот 2 (наш датчик 5)  → бит 4
        //    слот 3 (наш датчик 6)  → бит 5
        static const int sensorBitIndex[4] = {0, 1, 4, 5};

        for (int i = 0; i < 4; ++i) {
            const int bitIndex = sensorBitIndex[i];
            bool sensorFaulted = (alarmStatus >> bitIndex) & 0x01;
            data->alarms[i] = sensorFaulted ? 0 : 1;

            if (sensorFaulted) {
                data->errors.addCode(ALARM_SENSOR_BASE + i);
                qWarning() << "[TEMonitorV6Driver] Датчик" << (i + 1)
                           << "(бит" << bitIndex << ") неисправен";
            }
        }
    } else {
        qWarning() << "[TEMonitorV6Driver] Ошибка опроса статуса аварий"
                   << "причина:" << m_snmp.lastError();
        anyRequestFailed = true;
    }

    // ✅ Применяем поправки и проверяем пределы
    applySettingsAndCheckLimits(data);

    if (anyRequestFailed) {
        data->errors.addCode(250);
    }

    emit pollCompleted(m_deviceId, data);
    return data;
}

// ============================================================================
// ✅ ПРИМЕНЕНИЕ ПОПРАВОК И ПРОВЕРКА ПРЕДЕЛОВ
// ============================================================================
void TEMonitorV6Driver::applySettingsAndCheckLimits(std::shared_ptr<TemperaturaData> &data)
{
    QJsonArray sensors = m_settings.value("sensors").toArray();

    for (int i = 0; i < 4; ++i) {
        if (std::isnan(data->temperatures[i])) {
            continue;
        }

        if (i >= sensors.size()) continue;

        QJsonObject sensorObj = sensors[i].toObject();

        QString sensorType = sensorObj.value("type").toString("Нет");
        if (sensorType == "Нет") continue;

        // --- Применяем поправку ---
        double popravka = sensorObj.value("popravka").toDouble(0.0);
        double corrected = data->temperatures[i] + popravka;
        data->temperatures[i] = corrected;

        // --- Проверка верхнего предела ---
        bool topEnabled = sensorObj.value("topPredelEnabled").toBool(false);
        if (topEnabled) {
            double topPredel = sensorObj.value("topPredel").toDouble(0.0);
            if (corrected > topPredel) {
                data->errors.addCode(ALARM_TOP_BASE + i);
                qWarning() << "[TEMonitorV6Driver] Датчик" << (i + 1)
                           << "превысил верхний предел:"
                           << corrected << ">" << topPredel;
            }
        }

        // --- Проверка нижнего предела ---
        bool bottomEnabled = sensorObj.value("bottomPredelEnabled").toBool(false);
        if (bottomEnabled) {
            double bottomPredel = sensorObj.value("bottomPredel").toDouble(0.0);
            if (corrected < bottomPredel) {
                data->errors.addCode(ALARM_BOTTOM_BASE + i);
                qWarning() << "[TEMonitorV6Driver] Датчик" << (i + 1)
                           << "ниже нижнего предела:"
                           << corrected << "<" << bottomPredel;
            }
        }
    }
}

// ============================================================================
// КОМАНДЫ
// ============================================================================

bool TEMonitorV6Driver::executeCommand(const ControlCommand &cmd)
{
    Q_UNUSED(cmd);
    qWarning() << "[TEMonitorV6Driver] Устройство не поддерживает команды управления";
    emit commandExecuted(m_deviceId, false, "Команды не поддерживаются");
    return false;
}
