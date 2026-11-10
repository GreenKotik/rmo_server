#include "modulatordriver.h"
#include "basedocwidget.h"   // ← ДОБАВИТЬ: полное определение EquipmentConnection
#include <cmath>

// ============================================================================
// ✅ КОНСТРУКТОР / ДЕСТРУКТОР
// ============================================================================
ModulatorDriver::ModulatorDriver(int deviceId, QObject *parent)
    : IDeviceDriver(deviceId, parent)
{
    // 1. Грузим конфиг по deviceId (из applicationDirPath()/drivers/modulators/)
    if (!m_config.loadForDevice(deviceId)) {
        qWarning() << "[ModulatorDriver] Конфиг не найден для deviceId:" << deviceId;
        // Драйвер всё равно создаётся; poll() вернёт аварию 1600
    }

    // 2. Строим таблицы сеттеров/геттеров
    buildSetters();
    buildGetters();

    qDebug() << "[ModulatorDriver] Создан для deviceId:" << deviceId
             << "модель:" << m_config.model()
             << "параметров:" << m_config.params().size();
}

ModulatorDriver::~ModulatorDriver()
{
    disconnect();
}

// ============================================================================
// ✅ ПАРАМЕТРЫ ПОДКЛЮЧЕНИЯ
// ============================================================================
void ModulatorDriver::setConnectionParams(const EquipmentConnection &conn)
{
    m_ip        = conn.ipAddress;
    m_port      = conn.port > 0 ? conn.port : 161;
    m_community = conn.snmpV1CommunityRead.isEmpty() ? "public"
                                                     : conn.snmpV1CommunityRead;

    qDebug() << "[ModulatorDriver] Параметры подключения:"
             << "IP=" << m_ip
             << "Port=" << m_port
             << "Community=" << m_community;
}

void ModulatorDriver::setDeviceSettings(const QJsonObject &settings)
{
    // ✅ ModulatorDriver НЕ использует deviceSettings: конфиг модели
    //    грузится из JSON-файла по deviceId, а не из документа.
    //    Метод переопределён, чтобы явно зафиксировать это.
    Q_UNUSED(settings);
}

// ============================================================================
// ✅ ПОДКЛЮЧЕНИЕ
// ============================================================================
bool ModulatorDriver::connect()
{
    if (!m_config.isValid()) {
        qWarning() << "[ModulatorDriver] Нет конфига, подключение невозможно";
        return false;
    }
    if (m_ip.isEmpty()) {
        qWarning() << "[ModulatorDriver] IP-адрес не указан!";
        return false;
    }

    // Проверяем доступность через GET первого параметра из конфига
    const QVector<ModulatorParam> &params = m_config.params();
    if (params.isEmpty()) {
        qWarning() << "[ModulatorDriver] В конфиге нет параметров";
        return false;
    }

    QVariant probe;
    const ModulatorParam &first = params.first();
    if (m_snmp.get(m_ip, m_port, m_community, first.oid, probe, 2000)) {
        m_connected = true;
        emit connected(m_deviceId);
        qDebug() << "[ModulatorDriver] Подключено к" << m_ip
                 << "первый параметр" << first.key << "=" << probe;
        return true;
    }

    m_connected = false;
    qWarning() << "[ModulatorDriver] Ошибка подключения к" << m_ip
               << "причина:" << m_snmp.lastError();
    return false;
}

void ModulatorDriver::disconnect()
{
    m_connected = false;
    emit disconnected(m_deviceId);
}

// ============================================================================
// ✅ ОПРОС
//
// Семантика oidTodo:
//   • oidTodo: true  — заглушка, ещё не выверен по MIB. Провал ожидаем,
//                       авария 1601 НЕ ставится, лог не пишется.
//   • oidTodo: false — реальный OID. Провал = авария 1601 + одна строка
//                       qWarning за цикл со списком непрочитанных ключей.
// ============================================================================
std::shared_ptr<IDeviceData> ModulatorDriver::poll()
{
    auto data = std::make_shared<ModulatorData>(m_deviceId);

    // 0. Конфиг не загружен — авария 1600
    if (!m_config.isValid()) {
        data->errors.addCode(1600);   // "Конфигурация модулятора не найдена"
        emit pollCompleted(m_deviceId, data);
        return data;
    }

    // 1. Нет подключения — авария 250
    if (!m_connected) {
        data->setConnectionError();
        emit pollCompleted(m_deviceId, data);
        return data;
    }

    // 2. Читаем все параметры из конфига, собирая статистику
    int  failedRealCount = 0;       // реальные OID, не прочитались
    int  failedTodoCount = 0;       // заглушки, не прочитались (не авария)
    QStringList failedRealKeys;     // ключи реальных, для одной строки лога

    for (const ModulatorParam &p : m_config.params()) {
        if (readParam(p, *data)) {
            continue;   // прочитался — идём дальше
        }

        if (p.oidTodo) {
            ++failedTodoCount;      // тихо, без аварии
        } else {
            ++failedRealCount;
            failedRealKeys << p.key;
        }
    }

    // 3. Авария 1601 — только за реальные OID
    if (failedRealCount > 0) {
        data->errors.addCode(1601);   // "Модулятор: ошибка чтения параметра"

        // Одна строка за цикл вместо N
        qWarning() << "[ModulatorDriver] Не прочитано параметров:"
                   << failedRealCount
                   << "(модель:" << m_config.model() << ", deviceId:" << m_deviceId << ")"
                   << ":" << failedRealKeys.join(", ");
    }

    // 4. Заглушки — один qDebug за цикл, чтобы было видно, что они ещё в TODO
    if (failedTodoCount > 0) {
        qDebug() << "[ModulatorDriver] Заглушек не прочитано:"
                 << failedTodoCount
                 << "(модель:" << m_config.model() << ", deviceId:" << m_deviceId << ")";
    }

    // 5. Применяем правила аварий из конфига
    applyAlarmRules(*data);

    emit pollCompleted(m_deviceId, data);
    return data;
}

// ============================================================================
// ✅ ЧТЕНИЕ ОДНОГО ПАРАМЕТРА
//
// Логирование ошибок — НЕ здесь, а в poll() одной строкой за цикл.
// Для oidTodo-заглушек — тихо, без лога и без аварии.
// ============================================================================
bool ModulatorDriver::readParam(const ModulatorParam &p, ModulatorData &data)
{
    auto it = m_setters.constFind(p.target);
    if (it == m_setters.constEnd()) {
        qWarning() << "[ModulatorDriver] Нет сеттера для target:" << p.target
                   << "(key:" << p.key << ")";
        return false;
    }

    QVariant v;
    if (!m_snmp.get(m_ip, m_port, m_community, p.oid, v, 2000)) {
        // Никакого qWarning: poll() соберёт статистику и напишет одну строку.
        // Для oidTodo это вообще норма (OID ещё не выверен по MIB).
        return false;
    }

    (*it)(data, v, p.scale);
    return true;
}

// ============================================================================
// ✅ ПРАВИЛА АВАРИЙ
// ============================================================================
void ModulatorDriver::applyAlarmRules(ModulatorData &data)
{
    for (const AlarmRule &rule : m_config.alarmRules()) {
        auto it = m_getters.constFind(rule.field);
        if (it == m_getters.constEnd()) {
            qWarning() << "[ModulatorDriver] Правило аварии ссылается на неизвестное поле:"
                       << rule.field << "(код" << rule.code << "), пропущено";
            continue;
        }

        const double actual = (*it)(data);
        if (evalAlarmRule(rule, actual)) {
            data.errors.addCode(rule.code);
        }
    }
}

bool ModulatorDriver::evalAlarmRule(const AlarmRule &rule, double actual)
{
    switch (rule.op) {
        case AlarmOp::Eq: return actual == rule.value;
        case AlarmOp::Ne: return actual != rule.value;
        case AlarmOp::Gt: return actual >  rule.value;
        case AlarmOp::Lt: return actual <  rule.value;
        case AlarmOp::Ge: return actual >= rule.value;
        case AlarmOp::Le: return actual <= rule.value;
    }
    return false;
}

// ============================================================================
// ✅ КОМАНДЫ УПРАВЛЕНИЯ (SNMP SET)
// ============================================================================
bool ModulatorDriver::executeCommand(const ControlCommand &cmd)
{
    // 1. Конфиг
    if (!m_config.isValid()) {
        emit commandExecuted(m_deviceId, false, "Нет конфига модулятора");
        return false;
    }

    // 2. Ищем параметр по commandCode
    const ModulatorParam *param = m_config.findByCommandCode(cmd.commandCode);
    if (!param) {
        emit commandExecuted(m_deviceId, false,
            QString("Код команды %1 не найден в конфиге").arg(cmd.commandCode));
        return false;
    }

    // 3. Значение из params["value"]
    if (!cmd.params.contains("value")) {
        emit commandExecuted(m_deviceId, false, "Не задан params[\"value\"]");
        return false;
    }

    // 4. Проверка диапазона
    const double v = cmd.params.value("value").toDouble();
    if (v < param->min || v > param->max) {
        emit commandExecuted(m_deviceId, false,
            QString("Значение %1 вне диапазона [%2..%3]")
                .arg(v).arg(param->min).arg(param->max));
        return false;
    }

    // 5. Формируем значение нужного типа
    QVariant newValue;
    if (param->type == ParamType::String) {
        newValue = cmd.params.value("value").toString();
    } else {
        newValue = static_cast<int>(v);  // INTEGER для SNMP SET
    }

    // 6. SET
    if (m_snmp.set(m_ip, m_port, m_community, param->oid,
                   newValue, param->snmpType, 2000)) {
        qDebug() << "[ModulatorDriver] SET OK:" << param->key
                 << "=" << newValue << "OID:" << param->oid;
        emit commandExecuted(m_deviceId, true, "OK");
        return true;
    }

    qWarning() << "[ModulatorDriver] SET FAILED:" << param->key
               << "причина:" << m_snmp.lastError();
    emit commandExecuted(m_deviceId, false, m_snmp.lastError());
    return false;
}

// ============================================================================
// ✅ ТАБЛИЦА СЕТТЕРОВ (target → запись в ModulatorData)
// Полный охват полей ModulatorData из devicedata.h.
// При добавлении новых полей в ModulatorData — добавить строку сюда.
// ============================================================================
void ModulatorDriver::buildSetters()
{
    m_setters.clear();

    // --- Общие ---
    m_setters["serianumber"] = [](ModulatorData &d, const QVariant &v, double){ d.serianumber = v.toString(); };
    m_setters["productType"] = [](ModulatorData &d, const QVariant &v, double){ d.productType = v.toString(); };
    m_setters["productName"] = [](ModulatorData &d, const QVariant &v, double){ d.productName = v.toString(); };
    m_setters["temperature"] = [](ModulatorData &d, const QVariant &v, double s){ d.temperature = v.toDouble() * s; };

    // --- Alarm ---
    m_setters["alarmInputAsi1TsSynchroStatus"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.alarmInputAsi1TsSynchroStatus = static_cast<int>(v.toDouble() * s); };
    m_setters["alarmInputTSoIp1TsSynchroStatus"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.alarmInputTSoIp1TsSynchroStatus = static_cast<int>(v.toDouble() * s); };
    m_setters["alarmInputActiveInFifoOverflowStatus"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.alarmInputActiveInFifoOverflowStatus = static_cast<int>(v.toDouble() * s); };

    // --- Config ---
    m_setters["configProcessBaudrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.configProcessBaudrate = static_cast<int>(v.toDouble() * s); };

    // --- Monitor текущее ---
    m_setters["monitorInputAsi1TsSynchro"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputAsi1TsSynchro = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorInputAsi1UserfulBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputAsi1UserfulBitrate = v.toDouble() * s; };
    m_setters["monitorInputAsi1TotalBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputAsi1TotalBitrate = v.toDouble() * s; };
    m_setters["monitorInputTSoIp1TsSynchro"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputTSoIp1TsSynchro = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorInputTSoIp1UserfulBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputTSoIp1UserfulBitrate = v.toDouble() * s; };
    m_setters["monitorInputTSoIp1TotalBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputTSoIp1TotalBitrate = v.toDouble() * s; };

    // --- Monitor активный ---
    m_setters["monitorInputActiveAsi1TsSynchro"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputActiveAsi1TsSynchro = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorInputActiveAsi1UserfulBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputActiveAsi1UserfulBitrate = v.toDouble() * s; };
    m_setters["monitorInputActiveAsi1TotalBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputActiveAsi1TotalBitrate = v.toDouble() * s; };
    m_setters["monitorInputActiveTSoIp1UserfulBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputActiveTSoIp1UserfulBitrate = v.toDouble() * s; };
    m_setters["monitorInputActiveTSoIp1TotalBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputActiveTSoIp1TotalBitrate = v.toDouble() * s; };
    m_setters["monitorInputActiveTSoIp1TsSynchro"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorInputActiveTSoIp1TsSynchro = static_cast<int>(v.toDouble() * s); };

    // --- Monitor выход ---
    m_setters["monitorProcessOutputSignalBandwidth"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorProcessOutputSignalBandwidth = v.toDouble() * s; };
    m_setters["monitorProcessOutputChannelBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorProcessOutputChannelBitrate = v.toDouble() * s; };

    // --- Output RF ---
    m_setters["outputRfIfBandPowerLevel"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputRfIfBandPowerLevel = v.toDouble() * s; };
    m_setters["outputRfLbBandPowerLevel"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputRfLbBandPowerLevel = v.toDouble() * s; };
    m_setters["outputConfigRfLbBandCarrierFrequency"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfLbBandCarrierFrequency = v.toDouble() * s; };
    m_setters["outputConfigRfIfBandCarrierFrequency"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfIfBandCarrierFrequency = v.toDouble() * s; };
    m_setters["outputConfigRfMute"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfMute = static_cast<int>(v.toDouble() * s); };
    m_setters["outputConfigRfUnmute"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfUnmute = static_cast<int>(v.toDouble() * s); };
    m_setters["outputConfigRfMuteAtStartup"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfMuteAtStartup = static_cast<int>(v.toDouble() * s); };
    m_setters["outputConfigRfCarrierModulated"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfCarrierModulated = static_cast<int>(v.toDouble() * s); };
    m_setters["outputConfigRfCustomRolloffValue"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.outputConfigRfCustomRolloffValue = v.toDouble() * s; };

    // --- Monitor IP ---
    m_setters["monitorIp1EthernetLock"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorIp1EthernetLock = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorIp1MpegTsBitrate"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorIp1MpegTsBitrate = v.toDouble() * s; };

    // --- Relay ---
    m_setters["monitorRelayWarnInState"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorRelayWarnInState = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorRelayCritState"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorRelayCritState = static_cast<int>(v.toDouble() * s); };

    // --- Redundancy ---
    m_setters["monitorRedundancyStatusGpioRelay2"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorRedundancyStatusGpioRelay2 = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorRedundancyStatusGpioRelay3"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorRedundancyStatusGpioRelay3 = static_cast<int>(v.toDouble() * s); };

    // --- Time ---
    m_setters["monitorTimeCurrentUtc"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorTimeCurrentUtc = v.toDouble() * s; };

    // --- Power supply ---
    m_setters["monitorPowerSupply1Status"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorPowerSupply1Status = static_cast<int>(v.toDouble() * s); };
    m_setters["monitorPowerSupply2Status"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorPowerSupply2Status = static_cast<int>(v.toDouble() * s); };

    // --- Fans ---
    m_setters["monitorFan1Speed"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorFan1Speed = v.toDouble() * s; };
    m_setters["monitorFan2Speed"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorFan2Speed = v.toDouble() * s; };
    m_setters["monitorFan3Speed"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorFan3Speed = v.toDouble() * s; };
    m_setters["monitorFan4Speed"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorFan4Speed = v.toDouble() * s; };
    m_setters["monitorFan5Speed"] =
        [](ModulatorData &d, const QVariant &v, double s){ d.monitorFan5Speed = v.toDouble() * s; };
}

// ============================================================================
// ✅ ТАБЛИЦА ГЕТТЕРОВ (target → чтение из ModulatorData для alarmRules)
// Только числовые поля (int/double). Строки в alarmRules не поддерживаются.
// ============================================================================
void ModulatorDriver::buildGetters()
{
    m_getters.clear();

    m_getters["temperature"] =
        [](const ModulatorData &d){ return d.temperature; };
    m_getters["alarmInputAsi1TsSynchroStatus"] =
        [](const ModulatorData &d){ return static_cast<double>(d.alarmInputAsi1TsSynchroStatus); };
    m_getters["alarmInputTSoIp1TsSynchroStatus"] =
        [](const ModulatorData &d){ return static_cast<double>(d.alarmInputTSoIp1TsSynchroStatus); };
    m_getters["alarmInputActiveInFifoOverflowStatus"] =
        [](const ModulatorData &d){ return static_cast<double>(d.alarmInputActiveInFifoOverflowStatus); };
    m_getters["configProcessBaudrate"] =
        [](const ModulatorData &d){ return static_cast<double>(d.configProcessBaudrate); };
    m_getters["monitorInputAsi1TsSynchro"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorInputAsi1TsSynchro); };
    m_getters["monitorInputAsi1UserfulBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputAsi1UserfulBitrate; };
    m_getters["monitorInputAsi1TotalBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputAsi1TotalBitrate; };
    m_getters["monitorInputTSoIp1TsSynchro"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorInputTSoIp1TsSynchro); };
    m_getters["monitorInputTSoIp1UserfulBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputTSoIp1UserfulBitrate; };
    m_getters["monitorInputTSoIp1TotalBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputTSoIp1TotalBitrate; };
    m_getters["monitorInputActiveAsi1TsSynchro"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorInputActiveAsi1TsSynchro); };
    m_getters["monitorInputActiveAsi1UserfulBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputActiveAsi1UserfulBitrate; };
    m_getters["monitorInputActiveAsi1TotalBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputActiveAsi1TotalBitrate; };
    m_getters["monitorInputActiveTSoIp1UserfulBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputActiveTSoIp1UserfulBitrate; };
    m_getters["monitorInputActiveTSoIp1TotalBitrate"] =
        [](const ModulatorData &d){ return d.monitorInputActiveTSoIp1TotalBitrate; };
    m_getters["monitorInputActiveTSoIp1TsSynchro"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorInputActiveTSoIp1TsSynchro); };
    m_getters["monitorProcessOutputSignalBandwidth"] =
        [](const ModulatorData &d){ return d.monitorProcessOutputSignalBandwidth; };
    m_getters["monitorProcessOutputChannelBitrate"] =
        [](const ModulatorData &d){ return d.monitorProcessOutputChannelBitrate; };
    m_getters["outputRfIfBandPowerLevel"] =
        [](const ModulatorData &d){ return d.outputRfIfBandPowerLevel; };
    m_getters["outputRfLbBandPowerLevel"] =
        [](const ModulatorData &d){ return d.outputRfLbBandPowerLevel; };
    m_getters["outputConfigRfLbBandCarrierFrequency"] =
        [](const ModulatorData &d){ return d.outputConfigRfLbBandCarrierFrequency; };
    m_getters["outputConfigRfIfBandCarrierFrequency"] =
        [](const ModulatorData &d){ return d.outputConfigRfIfBandCarrierFrequency; };
    m_getters["outputConfigRfMute"] =
        [](const ModulatorData &d){ return static_cast<double>(d.outputConfigRfMute); };
    m_getters["outputConfigRfUnmute"] =
        [](const ModulatorData &d){ return static_cast<double>(d.outputConfigRfUnmute); };
    m_getters["outputConfigRfMuteAtStartup"] =
        [](const ModulatorData &d){ return static_cast<double>(d.outputConfigRfMuteAtStartup); };
    m_getters["outputConfigRfCarrierModulated"] =
        [](const ModulatorData &d){ return static_cast<double>(d.outputConfigRfCarrierModulated); };
    m_getters["outputConfigRfCustomRolloffValue"] =
        [](const ModulatorData &d){ return d.outputConfigRfCustomRolloffValue; };
    m_getters["monitorIp1EthernetLock"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorIp1EthernetLock); };
    m_getters["monitorIp1MpegTsBitrate"] =
        [](const ModulatorData &d){ return d.monitorIp1MpegTsBitrate; };
    m_getters["monitorRelayWarnInState"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorRelayWarnInState); };
    m_getters["monitorRelayCritState"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorRelayCritState); };
    m_getters["monitorRedundancyStatusGpioRelay2"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorRedundancyStatusGpioRelay2); };
    m_getters["monitorRedundancyStatusGpioRelay3"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorRedundancyStatusGpioRelay3); };
    m_getters["monitorTimeCurrentUtc"] =
        [](const ModulatorData &d){ return d.monitorTimeCurrentUtc; };
    m_getters["monitorPowerSupply1Status"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorPowerSupply1Status); };
    m_getters["monitorPowerSupply2Status"] =
        [](const ModulatorData &d){ return static_cast<double>(d.monitorPowerSupply2Status); };
    m_getters["monitorFan1Speed"] =
        [](const ModulatorData &d){ return d.monitorFan1Speed; };
    m_getters["monitorFan2Speed"] =
        [](const ModulatorData &d){ return d.monitorFan2Speed; };
    m_getters["monitorFan3Speed"] =
        [](const ModulatorData &d){ return d.monitorFan3Speed; };
    m_getters["monitorFan4Speed"] =
        [](const ModulatorData &d){ return d.monitorFan4Speed; };
    m_getters["monitorFan5Speed"] =
        [](const ModulatorData &d){ return d.monitorFan5Speed; };
}

QStringList ModulatorDriver::knownTargets()
{
    // Единственный источник правды — ключи m_setters.
    // Чтобы не дублировать список, создаём временный экземпляр таблицы.
    ModulatorDriver tmp(0);   // deviceId=0 — конфиг не нужен, только таблица
    return tmp.m_setters.keys();
}

QVariant ModulatorDriver::valueByTarget(const ModulatorData &data, const QString &target)
{
    // Строковые поля — вручную (в m_getters их нет, геттеры возвращают double)
    if (target == "serianumber") return data.serianumber;
    if (target == "productType") return data.productType;
    if (target == "productName") return data.productName;

    ModulatorDriver tmp(0);
    auto it = tmp.m_getters.constFind(target);
    if (it == tmp.m_getters.constEnd()) return QVariant();
    return (*it)(data);
}
