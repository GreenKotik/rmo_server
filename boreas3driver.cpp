#include "boreas3driver.h"
#include <cmath>

// ============================================================================
// ✅ OID-ы для Boreas 3
// ============================================================================
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

// ============================================================================
// КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================

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
}

// ============================================================================
// ПАРАМЕТРЫ ПОДКЛЮЧЕНИЯ И НАСТРОЙКИ
// ============================================================================

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

void Boreas3Driver::setDeviceSettings(const QJsonObject &settings)
{
    m_settings = settings;

    QJsonArray sensors = m_settings.value("sensors").toArray();
    qDebug() << "[Boreas3Driver] setDeviceSettings: датчиков =" << sensors.size();
}

// ============================================================================
// ПОДКЛЮЧЕНИЕ
// ============================================================================

bool Boreas3Driver::connect()
{
    if (m_ip.isEmpty()) {
        qWarning() << "[Boreas3Driver] IP-адрес не указан!";
        return false;
    }

    // Проверяем доступность через GET первого OID температуры
    QVariant result;
    bool ok = m_snmp.get(m_ip, m_port, m_community, OID_TEMP[0], result, 2000);

    if (ok) {
        m_connected = true;
        emit connected(m_deviceId);
        qDebug() << "[Boreas3Driver] Успешное подключение к" << m_ip
                 << "первое значение:" << result;
    } else {
        m_connected = false;
        qWarning() << "[Boreas3Driver] Ошибка подключения к" << m_ip
                   << "последняя ошибка:" << m_snmp.lastError();
    }

    return m_connected;
}

void Boreas3Driver::disconnect()
{
    m_connected = false;
    emit disconnected(m_deviceId);
    qDebug() << "[Boreas3Driver] Отключено";
}

// ============================================================================
// ОПРОС
// ============================================================================

std::shared_ptr<IDeviceData> Boreas3Driver::poll()
{
    // ✅ Проверка состояния перед опросом
    if (!m_connected || m_ip.isEmpty()) {
        qWarning() << "[Boreas3Driver] Устройство не подключено или IP не указан!";
        auto errorData = std::make_shared<TemperaturaData>(m_deviceId);
        errorData->setConnectionError();
        return errorData;
    }

    auto data = std::make_shared<TemperaturaData>(m_deviceId);

    // ✅ Настройки датчиков из файла документа
    QJsonArray sensors = m_settings.value("sensors").toArray();

    bool anyRequestFailed = false;

    // ------------------------------------------------------------------------
    // Опрос 4 датчиков — только активных
    // ------------------------------------------------------------------------
    for (int i = 0; i < 4; ++i) {
        // Проверка на потерю связи во время опроса
        if (!m_connected) {
            qWarning() << "[Boreas3Driver] Подключение потеряно во время опроса";
            data->errors.addCode(250);
            return data;
        }

        // -------------------------------------------------------------------
        // Определяем тип датчика
        // -------------------------------------------------------------------
        QString sensorType = "Нет";
        if (i < sensors.size()) {
            QJsonObject sensorObj = sensors[i].toObject();
            sensorType = sensorObj.value("type").toString("Нет");
        }

        // ✅ Если тип == "Нет" — пропускаем датчик полностью
        if (sensorType == "Нет") {
            data->temperatures[i] = qQNaN();
            data->alarms[i] = 0;
            continue;
        }

        // -------------------------------------------------------------------
        // Читаем температуру
        // -------------------------------------------------------------------
        QVariant tempResult;
        bool ok = m_snmp.get(m_ip, m_port, m_community,
                             OID_TEMP[i], tempResult, 2000);
        if (!ok) {
            qWarning() << "[Boreas3Driver] Ошибка опроса температуры" << (i + 1)
                       << "причина:" << m_snmp.lastError();
            anyRequestFailed = true;
            data->temperatures[i] = qQNaN();
            continue;
        }

        double rawTemp = tempResult.toDouble() * TEMP_MULTIPLIER;
        data->temperatures[i] = rawTemp;

        qDebug() << "[Boreas3Driver] Датчик" << (i + 1)
                 << "тип:" << sensorType
                 << "сырая температура:" << rawTemp;

        // -------------------------------------------------------------------
        // Читаем статус датчика
        // -------------------------------------------------------------------
        QVariant alarmResult;
        bool alarmOk = m_snmp.get(m_ip, m_port, m_community,
                                  OID_ALARM[i], alarmResult, 2000);
        if (alarmOk) {
            int status = alarmResult.toInt();
            data->alarms[i] = status;
            if (status != 1) {
                data->errors.addCode(633 + i);  // Неисправность датчика
                qWarning() << "[Boreas3Driver] Датчик" << (i + 1)
                           << "неисправен (статус:" << status << ")";
            }
        } else {
            qWarning() << "[Boreas3Driver] Ошибка опроса статуса" << (i + 1);
            anyRequestFailed = true;
        }
    }

    // ✅ Применяем поправки и проверяем пределы
    // (метод модифицирует data->temperatures и добавляет коды в data->errors)
    applySettingsAndCheckLimits(data);

    // ✅ Общая ошибка "нет связи", если что-то не ответило
    if (anyRequestFailed) {
        data->errors.addCode(250);
    }

    emit pollCompleted(m_deviceId, data);
    return data;
}

// ============================================================================
// ✅ ПРИМЕНЕНИЕ ПОПРАВОК И ПРОВЕРКА ПРЕДЕЛОВ
// ============================================================================
void Boreas3Driver::applySettingsAndCheckLimits(std::shared_ptr<TemperaturaData> &data)
{
    QJsonArray sensors = m_settings.value("sensors").toArray();

    for (int i = 0; i < 4; ++i) {
        // Пропускаем датчики, у которых нет данных
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
                qWarning() << "[Boreas3Driver] Датчик" << (i + 1)
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
                qWarning() << "[Boreas3Driver] Датчик" << (i + 1)
                           << "ниже нижнего предела:"
                           << corrected << "<" << bottomPredel;
            }
        }
    }
}

// ============================================================================
// КОМАНДЫ
// ============================================================================

bool Boreas3Driver::executeCommand(const ControlCommand &cmd)
{
    Q_UNUSED(cmd);
    qWarning() << "[Boreas3Driver] Устройство не поддерживает команды управления";
    emit commandExecuted(m_deviceId, false, "Команды не поддерживаются");
    return false;
}
