#ifndef DEVICEDATA_H
#define DEVICEDATA_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>   // для QJsonValue() в toJson
#include <QVariant>
#include <QMap>
#include <vector>
#include <memory>
#include <QDebug>              // ✅ Для qWarning() в createDeviceData()
#include <cmath>
#include "deviceids.h"
#include "alarmregistry.h"     // ✅ НОВОЕ: для AlarmRegistry::instance()
#include "databuffer.h" // Подключаем наш буфер

// ============================================================================
// ✅ СТРУКТУРА ДЛЯ ХРАНЕНИЯ КОДОВ ОШИБОК (Динамический размер)
// ============================================================================
struct DeviceErrors {
    int count = 0;
    std::vector<int> codes;

    void addCode(int code) {
        codes.push_back(code);
        count = static_cast<int>(codes.size());
    }

    void clear() {
        codes.clear();
        count = 0;
    }

    bool hasCriticalErrors() const {
        for (int code : codes) {
            if (AlarmRegistry::instance().getStatus(code) == 1) {
                return true;
            }
        }
        return false;
    }

    // ✅ Сериализация в формат MFC: "N;code1;code2;...;"
    // Совместимо с MeasurementsModel::rowErrorCount / rowErrorMessages.
    // Пустой список → "0;"
    QString toString() const {
        if (codes.empty()) {
            return QStringLiteral("0;");
        }
        QStringList parts;
        parts.reserve(static_cast<int>(codes.size()) + 1);
        parts << QString::number(count);
        for (int code : codes) {
            parts << QString::number(code);
        }
        return parts.join(';') + ';';
    }
};

// ============================================================================
// ✅ БАЗОВЫЙ ИНТЕРФЕЙС ДЛЯ ВСЕХ ДАННЫХ УСТРОЙСТВ
// ============================================================================
struct IDeviceData {
    virtual ~IDeviceData() = default;

    virtual int deviceId() const = 0;
    virtual bool hasErrors() const = 0;
    virtual DeviceErrors getErrors() const = 0;

    // ✅ Сериализация в JSON (для записи в БД и отправки клиентам)
    virtual QJsonObject toJson() const = 0;
    // ✅ НОВЫЙ ВИРТУАЛЬНЫЙ МЕТОД: Установить ошибку "Нет связи" (код 250)
    // Базовая реализация — пустая. Наследники могут переопределить.
    virtual void setConnectionError() {}
};

// ============================================================================
// ✅ КОНКРЕТНЫЕ СТРУКТУРЫ ДАННЫХ (Замена MFC-структурам)
// ============================================================================

// --- 1. Данные передатчика (аналог StructDataPeredatchik) ---
struct PeredatchikData : public IDeviceData {
    int m_deviceId;

    // Основные параметры (пример ключевых полей)
    int shRegim = 0;
    double dPower = 0.0;
    double dPower_dBm = 0.0;
    double dReflectPorog = 0.0;
    double dTemperatura = 0.0;
    double dVpitanie1 = 0.0;
    double dIpitanie1 = 0.0;
    int iOnline = 0;
    int iFault = 0;
    int iTransmit = 0;

    QString sRegim;          // ✅ Замена CString
    QString sErrorBin;       // ✅ Замена CString

    DeviceErrors errors;     // ✅ Динамические ошибки вместо int DecodeErrorsPer[40]

    PeredatchikData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }
    void setConnectionError() override {
        errors.addCode(250);
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;
        obj["power"] = dPower;
        obj["power_dBm"] = dPower_dBm;
        obj["temperature"] = dTemperatura;
        obj["online"] = (iOnline != 0);
        obj["fault"] = (iFault != 0);
        obj["regim"] = sRegim;

        // Сериализация ошибок
        QJsonArray errArray;
        for (int code : errors.codes) {
            errArray.append(code);
        }
        obj["errors"] = errArray;

        return obj;
    }
};

// --- 2. Унифицированные данные датчика температуры (для Boreas 3 и TE Monitor V6) ---
struct TemperaturaData : public IDeviceData {
    int m_deviceId;

    // ✅ 4 датчика (согласно настройкам документа)
    double temperatures[4] = {qQNaN(), qQNaN(), qQNaN(), qQNaN()};
    int alarms[4] = {0, 0, 0, 0};  // Статус каждого датчика (1 = OK, иначе — неисправность)

    DeviceErrors errors;

    TemperaturaData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }

    void setConnectionError() override {
        errors.addCode(250);
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;

        QJsonArray tempArray;
        QJsonArray alarmArray;
        for (int i = 0; i < 4; ++i) {
            // NaN → null в JSON (чтобы не терять информацию о «нет данных»)
            if (std::isnan(temperatures[i])) {
                tempArray.append(QJsonValue());  // null
            } else {
                tempArray.append(temperatures[i]);
            }
            alarmArray.append(alarms[i]);
        }
        obj["temperatures"] = tempArray;
        obj["alarms"] = alarmArray;

        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;

        return obj;
    }
};

// --- 3. Данные дегидратора (аналог StructDataDegidrator) ---
struct DegidratorData : public IDeviceData {
    int m_deviceId;
    double dPressure = 0.0;
    double dDutyCycle = 0.0;
    int iCanister1 = 0;
    int iCanister2 = 0;
    DeviceErrors errors;

    DegidratorData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }
    void setConnectionError() override {
        errors.addCode(250);
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;
        obj["pressure"] = dPressure;
        obj["duty_cycle"] = dDutyCycle;
        obj["canister1"] = iCanister1;
        obj["canister2"] = iCanister2;

        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;

        return obj;
    }
};

// --- 4. Данные UP-конвертора (аналог StructDataUP, сокращённый пример) ---
struct UPData : public IDeviceData {
    int m_deviceId;
    double dFrequency = 0.0;
    double dTemperatura = 0.0;
    double dAttenuator = 0.0;
    double dPowerIn = 0.0;
    double dPowerOut = 0.0;
    int iTransmit = 0;
    int iOnline = 0;
    int iFault = 0;
    DeviceErrors errors;

    UPData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }
    void setConnectionError() override {
        errors.addCode(250);
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;
        obj["frequency"] = dFrequency;
        obj["temperature"] = dTemperatura;
        obj["attenuator"] = dAttenuator;
        obj["power_in"] = dPowerIn;
        obj["power_out"] = dPowerOut;
        obj["online"] = (iOnline != 0);
        obj["fault"] = (iFault != 0);

        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;

        return obj;
    }
};


// --- Данные модулятора Teleteh TT-MC-72 ---
struct TeletehData : public IDeviceData {
    int m_deviceId;
    double outputPower = 0.0;   // Выходная мощность (dBm)
    double frequency = 0.0;     // Частота (MHz)
    int status = 0;             // 0 = OK, 1 = Alarm
    DeviceErrors errors;

    TeletehData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }

    void setConnectionError() override { errors.addCode(250); }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;
        obj["outputPower"] = outputPower;
        obj["frequency"] = frequency;
        obj["status"] = status;
        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;
        return obj;
    }
};

// --- Данные переключателя Dev-SW ---
struct DevSwData : public IDeviceData {
    int m_deviceId;
    int activePath = 0;         // 1 = Путь А, 2 = Путь Б
    int status = 0;             // 0 = OK, 1 = Alarm
    DeviceErrors errors;

    DevSwData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }

    void setConnectionError() override { errors.addCode(250); }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;
        obj["activePath"] = activePath;
        obj["status"] = status;
        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;
        return obj;
    }
};


// --- Данные модулятора (Enensess/Teleteh/Newtec/Sencore) ---
struct ModulatorData : public IDeviceData {
    int m_deviceId;

    // --- Общие ---
    QString serianumber;                // OID: Serianumber
    QString productType;                // OID: ProductType
    QString productName;                // OID: ProductName
    double  temperature = 0.0;          // OID: Temperature
    // --- Alarm (аварии входов) ---
    int alarmInputAsi1TsSynchroStatus = 0;          // OID: AlarmInputAsi1TsSynchroStatus
    int alarmInputTSoIp1TsSynchroStatus = 0;        // OID: AlarmInputTSoIp1TsSynchroStatus
    int alarmInputActiveInFifoOverflowStatus = 0;   // OID: AlarmInputActiveInFifoOverflowStatus
    // --- Config ---
    int configProcessBaudrate = 0;                  // OID: ConfigProcessBaudrate
    // --- Monitor (текущее состояние входов) ---
    int    monitorInputAsi1TsSynchro = 0;           // OID: MonitorInputAsi1TsSynchro
    double monitorInputAsi1UserfulBitrate = 0.0;    // OID: MonitorInputAsi1UserfulBitrate
    double monitorInputAsi1TotalBitrate = 0.0;      // OID: MonitorInputAsi1TotalBitrate
    int    monitorInputTSoIp1TsSynchro = 0;         // OID: MonitorInputTSoIp1TsSynchro
    double monitorInputTSoIp1UserfulBitrate = 0.0;  // OID: MonitorInputTSoIp1UserfulBitrate
    double monitorInputTSoIp1TotalBitrate = 0.0;    // OID: MonitorInputTSoIp1TotalBitrate
    // --- Monitor (активный вход) ---
    int    monitorInputActiveAsi1TsSynchro = 0;          // OID: MonitorInputActiveAsi1TsSynchro
    double monitorInputActiveAsi1UserfulBitrate = 0.0;   // OID: MonitorInputActiveAsi1UserfulBitrate
    double monitorInputActiveAsi1TotalBitrate = 0.0;     // OID: MonitorInputActiveAsi1TotalBitrate
    double monitorInputActiveTSoIp1UserfulBitrate = 0.0; // OID: MonitorInputActiveTSoIp1UserfulBitrate
    double monitorInputActiveTSoIp1TotalBitrate = 0.0;   // OID: MonitorInputActiveTSoIp1TotalBitrate
    int    monitorInputActiveTSoIp1TsSynchro = 0;        // OID: MonitorInputActiveTSoIp1TsSynchro
    // --- Monitor (выход) ---
    double monitorProcessOutputSignalBandwidth = 0.0; // OID: MonitorProcessOutputSignalBandwidth
    double monitorProcessOutputChannelBitrate = 0.0;  // OID: MonitorProcessOutputChannelBitrate
    // --- Output RF ---
    double outputRfIfBandPowerLevel = 0.0;              // OID: OutputRfIfBandPowerLevel
    double outputRfLbBandPowerLevel = 0.0;              // OID: OutputRfLbBandPowerLevel
    double outputConfigRfLbBandCarrierFrequency = 0.0;  // OID: OutputConfigRfLbBandCarrierFrequency
    double outputConfigRfIfBandCarrierFrequency = 0.0;  // OID: OutputConfigRfIfBandCarrierFrequency
    int    outputConfigRfMute = 0;                      // OID: OutputConfigRfMute
    int    outputConfigRfUnmute = 0;                    // OID: OutputConfigRfUnmute
    int    outputConfigRfMuteAtStartup = 0;             // OID: OutputConfigRfMuteAtStartup
    int    outputConfigRfCarrierModulated = 0;          // OID: OutputConfigRfCarrierModulated
    double outputConfigRfCustomRolloffValue = 0.0;      // OID: OutputConfigRfCustomRolloffValue
    // --- Monitor IP ---
    int    monitorIp1EthernetLock = 0;         // OID: MonitorIp1EthernetLock
    double monitorIp1MpegTsBitrate = 0.0;      // OID: MonitorIp1MpegTsBitrate
    // --- Relay ---
    int monitorRelayWarnInState = 0;           // OID: MonitorRelayWarnInState
    int monitorRelayCritState = 0;             // OID: MonitorRelayCritState
    // --- Redundancy ---
    int monitorRedundancyStatusGpioRelay2 = 0; // OID: MonitorRedundancyStatusGpioRelay2
    int monitorRedundancyStatusGpioRelay3 = 0; // OID: MonitorRedundancyStatusGpioRelay3
    // --- Time ---
    double monitorTimeCurrentUtc = 0.0;        // OID: MonitorTimeCurrentUtc
    // --- Power supply ---
    int monitorPowerSupply1Status = 0;         // OID: MonitorPowerSupply1Status
    int monitorPowerSupply2Status = 0;         // OID: MonitorPowerSupply2Status
    // --- Fans ---
    double monitorFan1Speed = 0.0;             // OID: MonitorFan1Speed
    double monitorFan2Speed = 0.0;             // OID: MonitorFan2Speed
    double monitorFan3Speed = 0.0;             // OID: MonitorFan3Speed
    double monitorFan4Speed = 0.0;             // OID: MonitorFan4Speed
    double monitorFan5Speed = 0.0;             // OID: MonitorFan5Speed

    DeviceErrors errors;

    ModulatorData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }

    void setConnectionError() override {
        errors.addCode(250);
    }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;

        // Общие
        obj["serianumber"] = serianumber;
        obj["productType"] = productType;
        obj["productName"] = productName;
        obj["temperature"] = temperature;
        // Alarm
        obj["alarmInputAsi1TsSynchroStatus"] = alarmInputAsi1TsSynchroStatus;
        obj["alarmInputTSoIp1TsSynchroStatus"] = alarmInputTSoIp1TsSynchroStatus;
        obj["alarmInputActiveInFifoOverflowStatus"] = alarmInputActiveInFifoOverflowStatus;
        // Config
        obj["configProcessBaudrate"] = configProcessBaudrate;
        // Monitor текущее
        obj["monitorInputAsi1TsSynchro"] = monitorInputAsi1TsSynchro;
        obj["monitorInputAsi1UserfulBitrate"] = monitorInputAsi1UserfulBitrate;
        obj["monitorInputAsi1TotalBitrate"] = monitorInputAsi1TotalBitrate;
        obj["monitorInputTSoIp1TsSynchro"] = monitorInputTSoIp1TsSynchro;
        obj["monitorInputTSoIp1UserfulBitrate"] = monitorInputTSoIp1UserfulBitrate;
        obj["monitorInputTSoIp1TotalBitrate"] = monitorInputTSoIp1TotalBitrate;
        // Monitor активный
        obj["monitorInputActiveAsi1TsSynchro"] = monitorInputActiveAsi1TsSynchro;
        obj["monitorInputActiveAsi1UserfulBitrate"] = monitorInputActiveAsi1UserfulBitrate;
        obj["monitorInputActiveAsi1TotalBitrate"] = monitorInputActiveAsi1TotalBitrate;
        obj["monitorInputActiveTSoIp1UserfulBitrate"] = monitorInputActiveTSoIp1UserfulBitrate;
        obj["monitorInputActiveTSoIp1TotalBitrate"] = monitorInputActiveTSoIp1TotalBitrate;
        obj["monitorInputActiveTSoIp1TsSynchro"] = monitorInputActiveTSoIp1TsSynchro;
        // Monitor выход
        obj["monitorProcessOutputSignalBandwidth"] = monitorProcessOutputSignalBandwidth;
        obj["monitorProcessOutputChannelBitrate"] = monitorProcessOutputChannelBitrate;
        // Output RF
        obj["outputRfIfBandPowerLevel"] = outputRfIfBandPowerLevel;
        obj["outputRfLbBandPowerLevel"] = outputRfLbBandPowerLevel;
        obj["outputConfigRfLbBandCarrierFrequency"] = outputConfigRfLbBandCarrierFrequency;
        obj["outputConfigRfIfBandCarrierFrequency"] = outputConfigRfIfBandCarrierFrequency;
        obj["outputConfigRfMute"] = outputConfigRfMute;
        obj["outputConfigRfUnmute"] = outputConfigRfUnmute;
        obj["outputConfigRfMuteAtStartup"] = outputConfigRfMuteAtStartup;
        obj["outputConfigRfCarrierModulated"] = outputConfigRfCarrierModulated;
        obj["outputConfigRfCustomRolloffValue"] = outputConfigRfCustomRolloffValue;
        // Monitor IP
        obj["monitorIp1EthernetLock"] = monitorIp1EthernetLock;
        obj["monitorIp1MpegTsBitrate"] = monitorIp1MpegTsBitrate;
        // Relay
        obj["monitorRelayWarnInState"] = monitorRelayWarnInState;
        obj["monitorRelayCritState"] = monitorRelayCritState;
        // Redundancy
        obj["monitorRedundancyStatusGpioRelay2"] = monitorRedundancyStatusGpioRelay2;
        obj["monitorRedundancyStatusGpioRelay3"] = monitorRedundancyStatusGpioRelay3;
        // Time
        obj["monitorTimeCurrentUtc"] = monitorTimeCurrentUtc;
        // Power supply
        obj["monitorPowerSupply1Status"] = monitorPowerSupply1Status;
        obj["monitorPowerSupply2Status"] = monitorPowerSupply2Status;
        // Fans
        obj["monitorFan1Speed"] = monitorFan1Speed;
        obj["monitorFan2Speed"] = monitorFan2Speed;
        obj["monitorFan3Speed"] = monitorFan3Speed;
        obj["monitorFan4Speed"] = monitorFan4Speed;
        obj["monitorFan5Speed"] = monitorFan5Speed;

        // Ошибки
        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;

        return obj;
    }
};

// ============================================================================
// ✅ ВСПОМОГАТЕЛЬНАЯ ФАБРИКА ДЛЯ СОЗДАНИЯ ПУСТЫХ ОБЪЕКТОВ ДАННЫХ
// ============================================================================
inline std::shared_ptr<IDeviceData> createDeviceData(int deviceId)
{
    // Используем макросы из deviceids.h для определения категории
/*    if (IS_TRANSMITTER(deviceId) || IS_TX_SWITCH(deviceId)) {
        return std::make_shared<PeredatchikData>(deviceId);
    }
    if (IS_TEMP_SENSOR(deviceId) || IS_TEMP_RECEIVER(deviceId)) {
        return std::make_shared<TemperaturaData>(deviceId);
    }
    if (IS_DEHYDRATOR(deviceId)) {
        return std::make_shared<DegidratorData>(deviceId);
    }
    if (IS_UP_CONVERTER(deviceId) || IS_UPC_SWITCH(deviceId)) {
        return std::make_shared<UPData>(deviceId);
    }
*/

    // ✅ Температурные датчики (Boreas 3, TE Monitor V6 и др.) возвращают TemperaturaData
    if (deviceId == ID_TEMP_SNS_BOREAS_3 ||
        deviceId == ID_TEMP_SNS_TE_MONITOR_V6) {
        return std::make_shared<TemperaturaData>(deviceId);
    }

    // Если тип неизвестен или специфичен, можно вернуть заглушку или расширить фабрику
    qWarning() << "[DeviceData] Неизвестный тип устройства для deviceId:" << deviceId;
    return nullptr;
}


// ============================================================================
// ✅ Структура данных для Передатчика (Transmitter) - ОБЪЕДИНЕННАЯ
// Включает данные передатчиков, температур, дегидратора и UP-конвертеров
// ============================================================================

struct TransmitterData {
    QDateTime timestamp;
    // 1. ДАННЫЕ ПЕРЕДАТЧИКОВ (StructDataPeredatchik)
    int    shRegim[3] = {0};
    double dPower[3] = {0.0};
    double dPowerPorogLowAlarm[3] = {0.0};
    double dPowerPorogLowFault[3] = {0.0};
    int    iPowerPorogLowCheck[3] = {0};
    double dPowerPorogHighAlarm[3] = {0.0};
    double dPowerPorogHighFault[3] = {0.0};
    int    iPowerPorogHighCheck[3] = {0};
    double dReflectPorogAlarm[3] = {0.0};
    double dReflectPorogFault[3] = {0.0};
    int    iReflectPorogCheck[3] = {0};
    double dPower_dBm[3] = {0.0};
    double dPower_dBW[3] = {0.0};
    double dInPower[3] = {0.0};
    double dRefPower[3] = {0.0};
    double dAttenuator[3] = {0.0};
    double dTemperLBV[3] = {0.0};
    double dTemperatura[3] = {0.0};
    double dIspirali[3] = {0.0};
    double dVspirali[3] = {0.0};
    double dVpitanie1[3] = {0.0};
    double dVpitanie2[3] = {0.0};
    double dVpitanie3[3] = {0.0};
    double dVpitanie4[3] = {0.0};
    double dIpitanie1[3] = {0.0};
    double dIpitanie2[3] = {0.0};
    double dIpitanie3[3] = {0.0};
    double dIpitanie4[3] = {0.0};
    int    iTransmit[3] = {0};
    int    iPowerZnakIn[3] = {0};
    int    iPowerZnakOut[3] = {0};
    int    iPowerZnakRef[3] = {0};
    int    iOnline[3] = {0};
    int    iFault[3] = {0};
    int    iRegimAM[3] = {0};
    int    iKanal[3] = {0};
    int    iPeredatchikID[3] = {0};
    QString sRegim[3];
    QString sErrorBin[3];
    DeviceErrors errorsPer[3];              // 3 УМ

    // --- Данные переключателя передатчиков ---
    double dSwitchPower = 0.0;
    int    iSwitchPowerZnak = 0;
    QString sRegimSwitch = "";
    int    iRegimSwitch = 0;
    int    iSwitchAM = 0;
    int    iSwitchKanal = 0;
    DeviceErrors errorsSwitch;              // переключатель УМ
//    int    iNumErrorsAll = 0;

    // 2. ДАННЫЕ ДАТЧИКОВ ТЕМПЕРАТУРЫ (StructDataTemperatura)
    double temperatures[4] = {0.0};
    DeviceErrors errorsTemperatura;         // датчик температуры

    // 3. ДАННЫЕ ДЕГИДРАТОРА (StructDataDegidrator)
    double dPressure = 0.0;
    double dDutyCycle = 0.0;
    int    iCanister1 = 0;
    int    iCanister2 = 0;
    int    iDegidratorLine[6] = {0}; // 0..5
    DeviceErrors errorsDegidrator;          // дегидратор

    // 4. ДАННЫЕ UP-КОНВЕРТЕРОВ (StructDataUP)
    // 4 основных + 1 резервный
    double dFrequencyUP[5] = {0.0};
    double dTemperaturaUP[5] = {0.0};
    double dAttenuatorUP[5] = {0.0};
    double dPowerInUP[5] = {0.0};
    double dPowerOutUP[5] = {0.0};
    int    iTransmitUP[5] = {0};
    int    iOnlineUP[5] = {0};
    int    iRegimAMUP[5] = {0};
    int    iRedundancyUP[5] = {0};
    int    iFaultUP[5] = {0};
    int    iConverterUP[5] = {0};
    int    iConverterExtUP[5] = {0};
    int    iRegimUP[5] = {0};
    DeviceErrors errorsUP[5];               // 4 основных + 1 резервный UP-конвертор

    // --- Переключатель UP-конвертеров ---
    double dFrequencyUPSwitch = 0.0;
    double dTemperaturaUPSwitch = 0.0;
    double dAttenuatorUPSwitch = 0.0;
    double dPowerInUPSwitch = 0.0;
    double dPowerOutUPSwitch = 0.0;
    int    iTransmitUPSwitch = 0;
    int    iOnlineUPSwitch = 0;
    int    iRegimAMUPSwitch = 0;
    int    iRedundancyUPSwitch = 0;
    int    iFaultUPSwitch = 0;
    int    iConverterUPSwitch = 0;
    int    iConverterExtUPSwitch = 0;
    int    iRegimUPSwitch = 0;
    int    iBackupRegimUP = 0;
    int    iBackupNumUP = 0;
    DeviceErrors errorsUPSwitch;            // переключатель UP-конверторов

    // 5. ОБЩИЕ ПОЛЯ ДЛЯ БАЗЫ ДАННЫХ
    QString Sputnik = "";
    // ✅ Агрегированные ошибки по всем устройствам документа
    // Заполняется в BaseDocWidget после завершения цикла опроса
    DeviceErrors errorsAll;
    QString ErrorsAll = "";
    bool writeToDb = false;
    QString ProtokolPer1 = "", ProtokolPer2 = "", ProtokolPer3 = "", ProtokolPerSW = "";

    // ✅ Сериализация в JSON для базы данных
    QJsonObject toJsonObject() const {
        QJsonObject obj;

        // ✅ Передаём время опроса в БД (если установлено).
        //    Если пустое — DatabaseManager подставит CURRENT_TIMESTAMP.
        if (timestamp.isValid()) {
            obj["Дата_и_время"] = timestamp.toString(Qt::ISODate);
        }
        // --- 1. Основные поля, которые УЖЕ есть в текущей схеме БД (docType = 2) ---
        obj["Спутник"] = Sputnik;

        // Маппинг первых 3-х передатчиков на имена полей в БД
        for (int i = 0; i < 3; ++i) {
            QString idx = QString::number(i + 1);
            obj["PerPowerIn" + idx] = dInPower[i];
            obj["PerPower" + idx] = dPower[i];
            obj["PerPowerOtr" + idx] = dRefPower[i];
            obj["Attenuator" + idx] = dAttenuator[i];
            obj["Spiral" + idx] = dIspirali[i];
            obj["PerStatus" + idx] = iOnline[i];
        }

        obj["SwitchPower"] = dSwitchPower;
        obj["SwitchStatus"] = iRegimSwitch;
        obj["SwitchReg"] = iSwitchAM;
        obj["SwitchKanal"] = iSwitchKanal;
        obj["Temper1"] = temperatures[0];
        obj["Temper2"] = temperatures[1];
        obj["ErrorsAll"] = ErrorsAll;
        obj["write_to_db"] = writeToDb;
        obj["ProtokolPer1"] = ProtokolPer1;
        obj["ProtokolPer2"] = ProtokolPer2;
        obj["ProtokolPer3"] = ProtokolPer3;
        obj["ProtokolPerSW"] = ProtokolPerSW;

        // --- 2. ДОПОЛНИТЕЛЬНЫЕ ПОЛЯ (Готовы для добавления в БД через миграцию) ---
        // Если вы захотите сохранять эти данные, просто добавьте соответствующие
        // столбцы в таблицу через migrateTableIfNeeded и раскомментируйте строки ниже:

        /*
        // Дегидратор
        obj["Degidrator_Pressure"] = dPressure;
        obj["Degidrator_DutyCycle"] = dDutyCycle;
        obj["Degidrator_Canister1"] = iCanister1;
        obj["Degidrator_Canister2"] = iCanister2;

        // UP-конвертеры (пример для 1-го основного и переключателя)
        obj["UP1_Frequency"] = dFrequencyUP[0];
        obj["UP1_Temperature"] = dTemperaturaUP[0];
        obj["UP1_PowerIn"] = dPowerInUP[0];
        obj["UP1_PowerOut"] = dPowerOutUP[0];
        obj["UP1_Attenuator"] = dAttenuatorUP[0];
        obj["UP1_Online"] = iOnlineUP[0];

        obj["UPSwitch_Frequency"] = dFrequencyUPSwitch;
        obj["UPSwitch_Temperature"] = dTemperaturaUPSwitch;
        obj["UPSwitch_Online"] = iOnlineUPSwitch;
        */

        return obj;
    }
};


// ============================================================================
// ✅ Структура данных для документа "Модулятор" (KanalModulatorData)
// Хранит данные от двух модуляторов + (в будущем) переключателя и анализатора.
// Располагается после TransmitterData.
// ============================================================================
struct KanalModulatorData {
    QDateTime timestamp;
    // ------------------------------------------------------------------------
    // МОДУЛЯТОР 1 (данные от ModulatorData для первого слота)
    // ------------------------------------------------------------------------
    QString mod1_serianumber;
    QString mod1_productType;
    QString mod1_productName;
    double  mod1_temperature = 0.0;

    int mod1_alarmInputAsi1TsSynchroStatus = 0;
    int mod1_alarmInputTSoIp1TsSynchroStatus = 0;
    int mod1_alarmInputActiveInFifoOverflowStatus = 0;

    int mod1_configProcessBaudrate = 0;

    int    mod1_monitorInputAsi1TsSynchro = 0;
    double mod1_monitorInputAsi1UserfulBitrate = 0.0;
    double mod1_monitorInputAsi1TotalBitrate = 0.0;
    int    mod1_monitorInputTSoIp1TsSynchro = 0;
    double mod1_monitorInputTSoIp1UserfulBitrate = 0.0;
    double mod1_monitorInputTSoIp1TotalBitrate = 0.0;

    int    mod1_monitorInputActiveAsi1TsSynchro = 0;
    double mod1_monitorInputActiveAsi1UserfulBitrate = 0.0;
    double mod1_monitorInputActiveAsi1TotalBitrate = 0.0;
    double mod1_monitorInputActiveTSoIp1UserfulBitrate = 0.0;
    double mod1_monitorInputActiveTSoIp1TotalBitrate = 0.0;
    int    mod1_monitorInputActiveTSoIp1TsSynchro = 0;

    double mod1_monitorProcessOutputSignalBandwidth = 0.0;
    double mod1_monitorProcessOutputChannelBitrate = 0.0;

    double mod1_outputRfIfBandPowerLevel = 0.0;
    double mod1_outputRfLbBandPowerLevel = 0.0;
    double mod1_outputConfigRfLbBandCarrierFrequency = 0.0;
    double mod1_outputConfigRfIfBandCarrierFrequency = 0.0;
    int    mod1_outputConfigRfMute = 0;
    int    mod1_outputConfigRfUnmute = 0;
    int    mod1_outputConfigRfMuteAtStartup = 0;
    int    mod1_outputConfigRfCarrierModulated = 0;
    double mod1_outputConfigRfCustomRolloffValue = 0.0;

    int    mod1_monitorIp1EthernetLock = 0;
    double mod1_monitorIp1MpegTsBitrate = 0.0;

    int mod1_monitorRelayWarnInState = 0;
    int mod1_monitorRelayCritState = 0;

    int mod1_monitorRedundancyStatusGpioRelay2 = 0;
    int mod1_monitorRedundancyStatusGpioRelay3 = 0;

    double mod1_monitorTimeCurrentUtc = 0.0;

    int mod1_monitorPowerSupply1Status = 0;
    int mod1_monitorPowerSupply2Status = 0;

    double mod1_monitorFan1Speed = 0.0;
    double mod1_monitorFan2Speed = 0.0;
    double mod1_monitorFan3Speed = 0.0;
    double mod1_monitorFan4Speed = 0.0;
    double mod1_monitorFan5Speed = 0.0;

    DeviceErrors errorsMod1;

    // ------------------------------------------------------------------------
    // МОДУЛЯТОР 2 (данные от ModulatorData для второго слота)
    // ------------------------------------------------------------------------
    QString mod2_serianumber;
    QString mod2_productType;
    QString mod2_productName;
    double  mod2_temperature = 0.0;

    int mod2_alarmInputAsi1TsSynchroStatus = 0;
    int mod2_alarmInputTSoIp1TsSynchroStatus = 0;
    int mod2_alarmInputActiveInFifoOverflowStatus = 0;

    int mod2_configProcessBaudrate = 0;

    int    mod2_monitorInputAsi1TsSynchro = 0;
    double mod2_monitorInputAsi1UserfulBitrate = 0.0;
    double mod2_monitorInputAsi1TotalBitrate = 0.0;
    int    mod2_monitorInputTSoIp1TsSynchro = 0;
    double mod2_monitorInputTSoIp1UserfulBitrate = 0.0;
    double mod2_monitorInputTSoIp1TotalBitrate = 0.0;

    int    mod2_monitorInputActiveAsi1TsSynchro = 0;
    double mod2_monitorInputActiveAsi1UserfulBitrate = 0.0;
    double mod2_monitorInputActiveAsi1TotalBitrate = 0.0;
    double mod2_monitorInputActiveTSoIp1UserfulBitrate = 0.0;
    double mod2_monitorInputActiveTSoIp1TotalBitrate = 0.0;
    int    mod2_monitorInputActiveTSoIp1TsSynchro = 0;

    double mod2_monitorProcessOutputSignalBandwidth = 0.0;
    double mod2_monitorProcessOutputChannelBitrate = 0.0;

    double mod2_outputRfIfBandPowerLevel = 0.0;
    double mod2_outputRfLbBandPowerLevel = 0.0;
    double mod2_outputConfigRfLbBandCarrierFrequency = 0.0;
    double mod2_outputConfigRfIfBandCarrierFrequency = 0.0;
    int    mod2_outputConfigRfMute = 0;
    int    mod2_outputConfigRfUnmute = 0;
    int    mod2_outputConfigRfMuteAtStartup = 0;
    int    mod2_outputConfigRfCarrierModulated = 0;
    double mod2_outputConfigRfCustomRolloffValue = 0.0;

    int    mod2_monitorIp1EthernetLock = 0;
    double mod2_monitorIp1MpegTsBitrate = 0.0;

    int mod2_monitorRelayWarnInState = 0;
    int mod2_monitorRelayCritState = 0;

    int mod2_monitorRedundancyStatusGpioRelay2 = 0;
    int mod2_monitorRedundancyStatusGpioRelay3 = 0;

    double mod2_monitorTimeCurrentUtc = 0.0;

    int mod2_monitorPowerSupply1Status = 0;
    int mod2_monitorPowerSupply2Status = 0;

    double mod2_monitorFan1Speed = 0.0;
    double mod2_monitorFan2Speed = 0.0;
    double mod2_monitorFan3Speed = 0.0;
    double mod2_monitorFan4Speed = 0.0;
    double mod2_monitorFan5Speed = 0.0;

    DeviceErrors errorsMod2;

    // ------------------------------------------------------------------------
    // ПЕРЕКЛЮЧАТЕЛЬ РЕЗЕРВИРОВАНИЯ (задел, сейчас — заглушка)
    // ------------------------------------------------------------------------
    int switchActivePath = 0;   // 1 = Mod1, 2 = Mod2
    int switchStatus = 0;
    DeviceErrors errorsSwitch;

    // ------------------------------------------------------------------------
    // ОБЩИЕ ПОЛЯ
    // ------------------------------------------------------------------------
    DeviceErrors errorsAll;
    QString ErrorsAll = "";
    bool writeToDb = false;

    // ✅ Сериализация в JSON для БД и рассылки клиентам
    QJsonObject toJsonObject() const {
        QJsonObject obj;

        if (timestamp.isValid()) {
            obj["Дата_и_время"] = timestamp.toString(Qt::ISODate);
        }

        // --- Модулятор 1 ---
        obj["Mod1_Serianumber"] = mod1_serianumber;
        obj["Mod1_ProductType"] = mod1_productType;
        obj["Mod1_ProductName"] = mod1_productName;
        obj["Mod1_Temperature"] = mod1_temperature;

        obj["Mod1_AlarmInputAsi1TsSynchroStatus"] = mod1_alarmInputAsi1TsSynchroStatus;
        obj["Mod1_AlarmInputTSoIp1TsSynchroStatus"] = mod1_alarmInputTSoIp1TsSynchroStatus;
        obj["Mod1_AlarmInputActiveInFifoOverflowStatus"] = mod1_alarmInputActiveInFifoOverflowStatus;

        obj["Mod1_ConfigProcessBaudrate"] = mod1_configProcessBaudrate;

        obj["Mod1_MonitorInputAsi1TsSynchro"] = mod1_monitorInputAsi1TsSynchro;
        obj["Mod1_MonitorInputAsi1UserfulBitrate"] = mod1_monitorInputAsi1UserfulBitrate;
        obj["Mod1_MonitorInputAsi1TotalBitrate"] = mod1_monitorInputAsi1TotalBitrate;
        obj["Mod1_MonitorInputTSoIp1TsSynchro"] = mod1_monitorInputTSoIp1TsSynchro;
        obj["Mod1_MonitorInputTSoIp1UserfulBitrate"] = mod1_monitorInputTSoIp1UserfulBitrate;
        obj["Mod1_MonitorInputTSoIp1TotalBitrate"] = mod1_monitorInputTSoIp1TotalBitrate;

        obj["Mod1_MonitorInputActiveAsi1TsSynchro"] = mod1_monitorInputActiveAsi1TsSynchro;
        obj["Mod1_MonitorInputActiveAsi1UserfulBitrate"] = mod1_monitorInputActiveAsi1UserfulBitrate;
        obj["Mod1_MonitorInputActiveAsi1TotalBitrate"] = mod1_monitorInputActiveAsi1TotalBitrate;
        obj["Mod1_MonitorInputActiveTSoIp1UserfulBitrate"] = mod1_monitorInputActiveTSoIp1UserfulBitrate;
        obj["Mod1_MonitorInputActiveTSoIp1TotalBitrate"] = mod1_monitorInputActiveTSoIp1TotalBitrate;
        obj["Mod1_MonitorInputActiveTSoIp1TsSynchro"] = mod1_monitorInputActiveTSoIp1TsSynchro;

        obj["Mod1_MonitorProcessOutputSignalBandwidth"] = mod1_monitorProcessOutputSignalBandwidth;
        obj["Mod1_MonitorProcessOutputChannelBitrate"] = mod1_monitorProcessOutputChannelBitrate;

        obj["Mod1_OutputRfIfBandPowerLevel"] = mod1_outputRfIfBandPowerLevel;
        obj["Mod1_OutputRfLbBandPowerLevel"] = mod1_outputRfLbBandPowerLevel;
        obj["Mod1_OutputConfigRfLbBandCarrierFrequency"] = mod1_outputConfigRfLbBandCarrierFrequency;
        obj["Mod1_OutputConfigRfIfBandCarrierFrequency"] = mod1_outputConfigRfIfBandCarrierFrequency;
        obj["Mod1_OutputConfigRfMute"] = mod1_outputConfigRfMute;
        obj["Mod1_OutputConfigRfUnmute"] = mod1_outputConfigRfUnmute;
        obj["Mod1_OutputConfigRfMuteAtStartup"] = mod1_outputConfigRfMuteAtStartup;
        obj["Mod1_OutputConfigRfCarrierModulated"] = mod1_outputConfigRfCarrierModulated;
        obj["Mod1_OutputConfigRfCustomRolloffValue"] = mod1_outputConfigRfCustomRolloffValue;

        obj["Mod1_MonitorIp1EthernetLock"] = mod1_monitorIp1EthernetLock;
        obj["Mod1_MonitorIp1MpegTsBitrate"] = mod1_monitorIp1MpegTsBitrate;

        obj["Mod1_MonitorRelayWarnInState"] = mod1_monitorRelayWarnInState;
        obj["Mod1_MonitorRelayCritState"] = mod1_monitorRelayCritState;

        obj["Mod1_MonitorRedundancyStatusGpioRelay2"] = mod1_monitorRedundancyStatusGpioRelay2;
        obj["Mod1_MonitorRedundancyStatusGpioRelay3"] = mod1_monitorRedundancyStatusGpioRelay3;

        obj["Mod1_MonitorTimeCurrentUtc"] = mod1_monitorTimeCurrentUtc;

        obj["Mod1_MonitorPowerSupply1Status"] = mod1_monitorPowerSupply1Status;
        obj["Mod1_MonitorPowerSupply2Status"] = mod1_monitorPowerSupply2Status;

        obj["Mod1_MonitorFan1Speed"] = mod1_monitorFan1Speed;
        obj["Mod1_MonitorFan2Speed"] = mod1_monitorFan2Speed;
        obj["Mod1_MonitorFan3Speed"] = mod1_monitorFan3Speed;
        obj["Mod1_MonitorFan4Speed"] = mod1_monitorFan4Speed;
        obj["Mod1_MonitorFan5Speed"] = mod1_monitorFan5Speed;

        // --- Модулятор 2 ---
        obj["Mod2_Serianumber"] = mod2_serianumber;
        obj["Mod2_ProductType"] = mod2_productType;
        obj["Mod2_ProductName"] = mod2_productName;
        obj["Mod2_Temperature"] = mod2_temperature;

        obj["Mod2_AlarmInputAsi1TsSynchroStatus"] = mod2_alarmInputAsi1TsSynchroStatus;
        obj["Mod2_AlarmInputTSoIp1TsSynchroStatus"] = mod2_alarmInputTSoIp1TsSynchroStatus;
        obj["Mod2_AlarmInputActiveInFifoOverflowStatus"] = mod2_alarmInputActiveInFifoOverflowStatus;

        obj["Mod2_ConfigProcessBaudrate"] = mod2_configProcessBaudrate;

        obj["Mod2_MonitorInputAsi1TsSynchro"] = mod2_monitorInputAsi1TsSynchro;
        obj["Mod2_MonitorInputAsi1UserfulBitrate"] = mod2_monitorInputAsi1UserfulBitrate;
        obj["Mod2_MonitorInputAsi1TotalBitrate"] = mod2_monitorInputAsi1TotalBitrate;
        obj["Mod2_MonitorInputTSoIp1TsSynchro"] = mod2_monitorInputTSoIp1TsSynchro;
        obj["Mod2_MonitorInputTSoIp1UserfulBitrate"] = mod2_monitorInputTSoIp1UserfulBitrate;
        obj["Mod2_MonitorInputTSoIp1TotalBitrate"] = mod2_monitorInputTSoIp1TotalBitrate;

        obj["Mod2_MonitorInputActiveAsi1TsSynchro"] = mod2_monitorInputActiveAsi1TsSynchro;
        obj["Mod2_MonitorInputActiveAsi1UserfulBitrate"] = mod2_monitorInputActiveAsi1UserfulBitrate;
        obj["Mod2_MonitorInputActiveAsi1TotalBitrate"] = mod2_monitorInputActiveAsi1TotalBitrate;
        obj["Mod2_MonitorInputActiveTSoIp1UserfulBitrate"] = mod2_monitorInputActiveTSoIp1UserfulBitrate;
        obj["Mod2_MonitorInputActiveTSoIp1TotalBitrate"] = mod2_monitorInputActiveTSoIp1TotalBitrate;
        obj["Mod2_MonitorInputActiveTSoIp1TsSynchro"] = mod2_monitorInputActiveTSoIp1TsSynchro;

        obj["Mod2_MonitorProcessOutputSignalBandwidth"] = mod2_monitorProcessOutputSignalBandwidth;
        obj["Mod2_MonitorProcessOutputChannelBitrate"] = mod2_monitorProcessOutputChannelBitrate;

        obj["Mod2_OutputRfIfBandPowerLevel"] = mod2_outputRfIfBandPowerLevel;
        obj["Mod2_OutputRfLbBandPowerLevel"] = mod2_outputRfLbBandPowerLevel;
        obj["Mod2_OutputConfigRfLbBandCarrierFrequency"] = mod2_outputConfigRfLbBandCarrierFrequency;
        obj["Mod2_OutputConfigRfIfBandCarrierFrequency"] = mod2_outputConfigRfIfBandCarrierFrequency;
        obj["Mod2_OutputConfigRfMute"] = mod2_outputConfigRfMute;
        obj["Mod2_OutputConfigRfUnmute"] = mod2_outputConfigRfUnmute;
        obj["Mod2_OutputConfigRfMuteAtStartup"] = mod2_outputConfigRfMuteAtStartup;
        obj["Mod2_OutputConfigRfCarrierModulated"] = mod2_outputConfigRfCarrierModulated;
        obj["Mod2_OutputConfigRfCustomRolloffValue"] = mod2_outputConfigRfCustomRolloffValue;

        obj["Mod2_MonitorIp1EthernetLock"] = mod2_monitorIp1EthernetLock;
        obj["Mod2_MonitorIp1MpegTsBitrate"] = mod2_monitorIp1MpegTsBitrate;

        obj["Mod2_MonitorRelayWarnInState"] = mod2_monitorRelayWarnInState;
        obj["Mod2_MonitorRelayCritState"] = mod2_monitorRelayCritState;

        obj["Mod2_MonitorRedundancyStatusGpioRelay2"] = mod2_monitorRedundancyStatusGpioRelay2;
        obj["Mod2_MonitorRedundancyStatusGpioRelay3"] = mod2_monitorRedundancyStatusGpioRelay3;

        obj["Mod2_MonitorTimeCurrentUtc"] = mod2_monitorTimeCurrentUtc;

        obj["Mod2_MonitorPowerSupply1Status"] = mod2_monitorPowerSupply1Status;
        obj["Mod2_MonitorPowerSupply2Status"] = mod2_monitorPowerSupply2Status;

        obj["Mod2_MonitorFan1Speed"] = mod2_monitorFan1Speed;
        obj["Mod2_MonitorFan2Speed"] = mod2_monitorFan2Speed;
        obj["Mod2_MonitorFan3Speed"] = mod2_monitorFan3Speed;
        obj["Mod2_MonitorFan4Speed"] = mod2_monitorFan4Speed;
        obj["Mod2_MonitorFan5Speed"] = mod2_monitorFan5Speed;

        // --- Переключатель ---
        obj["SwitchActivePath"] = switchActivePath;
        obj["SwitchStatus"] = switchStatus;

        // --- Общие ---
        obj["ErrorsAll"] = ErrorsAll;
        obj["write_to_db"] = writeToDb;

        return obj;
    }
};


// ============================================================================
// ✅ Структура данных для Энерго (Energo) - ПОЛНЫЙ СПИСОК ПОЛЕЙ
// ============================================================================
struct EnergoData {
    QDateTime timestamp;
    // --- VV1 (Ввод/Вывод 1) ---
    double VV1_In_V1 = 0.0, VV1_In_V2 = 0.0, VV1_In_V3 = 0.0;
    double VV1_In_A1 = 0.0, VV1_In_A2 = 0.0, VV1_In_A3 = 0.0;
    double VV1_In_W1 = 0.0, VV1_In_W2 = 0.0, VV1_In_W3 = 0.0;
    double VV1_In_H = 0.0, VV1_In_AAll = 0.0;

    double VV1_Out_V1 = 0.0, VV1_Out_V2 = 0.0, VV1_Out_V3 = 0.0;
    double VV1_Out_A1 = 0.0, VV1_Out_A2 = 0.0, VV1_Out_A3 = 0.0;
    double VV1_Out_W1 = 0.0, VV1_Out_W2 = 0.0, VV1_Out_W3 = 0.0;
    double VV1_Out_H = 0.0, VV1_Out_AAll = 0.0;

    // --- VV2 (Ввод/Вывод 2) ---
    double VV2_In_V1 = 0.0, VV2_In_V2 = 0.0, VV2_In_V3 = 0.0;
    double VV2_In_A1 = 0.0, VV2_In_A2 = 0.0, VV2_In_A3 = 0.0;
    double VV2_In_W1 = 0.0, VV2_In_W2 = 0.0, VV2_In_W3 = 0.0;
    double VV2_In_H = 0.0, VV2_In_AAll = 0.0;

    double VV2_Out_V1 = 0.0, VV2_Out_V2 = 0.0, VV2_Out_V3 = 0.0;
    double VV2_Out_A1 = 0.0, VV2_Out_A2 = 0.0, VV2_Out_A3 = 0.0;
    double VV2_Out_W1 = 0.0, VV2_Out_W2 = 0.0, VV2_Out_W3 = 0.0;
    double VV2_Out_H = 0.0, VV2_Out_AAll = 0.0;

    // --- VV3 (Ввод/Вывод 3) ---
    double VV3_In_V1 = 0.0, VV3_In_V2 = 0.0, VV3_In_V3 = 0.0;
    double VV3_In_A1 = 0.0, VV3_In_A2 = 0.0, VV3_In_A3 = 0.0;
    double VV3_In_W1 = 0.0, VV3_In_W2 = 0.0, VV3_In_W3 = 0.0;
    double VV3_In_H = 0.0, VV3_In_AAll = 0.0;

    double VV3_Out_V1 = 0.0, VV3_Out_V2 = 0.0, VV3_Out_V3 = 0.0;
    double VV3_Out_A1 = 0.0, VV3_Out_A2 = 0.0, VV3_Out_A3 = 0.0;
    double VV3_Out_W1 = 0.0, VV3_Out_W2 = 0.0, VV3_Out_W3 = 0.0;
    double VV3_Out_H = 0.0, VV3_Out_AAll = 0.0;

    // --- VV4 (Ввод/Вывод 4) ---
    double VV4_In_V1 = 0.0, VV4_In_V2 = 0.0, VV4_In_V3 = 0.0;
    double VV4_In_A1 = 0.0, VV4_In_A2 = 0.0, VV4_In_A3 = 0.0;
    double VV4_In_W1 = 0.0, VV4_In_W2 = 0.0, VV4_In_W3 = 0.0;
    double VV4_In_H = 0.0, VV4_In_AAll = 0.0;

    double VV4_Out_V1 = 0.0, VV4_Out_V2 = 0.0, VV4_Out_V3 = 0.0;
    double VV4_Out_A1 = 0.0, VV4_Out_A2 = 0.0, VV4_Out_A3 = 0.0;
    double VV4_Out_W1 = 0.0, VV4_Out_W2 = 0.0, VV4_Out_W3 = 0.0;
    double VV4_Out_H = 0.0, VV4_Out_AAll = 0.0;

    // --- ИБП 1 ---
    double IBP1_In_H = 0.0, IBP1_In_V1 = 0.0, IBP1_In_V2 = 0.0, IBP1_In_V3 = 0.0;
    double IBP1_In_A1 = 0.0, IBP1_In_A2 = 0.0, IBP1_In_A3 = 0.0;
    double IBP1_In_W1 = 0.0, IBP1_In_W2 = 0.0, IBP1_In_W3 = 0.0;
    double IBP1_Bypass_H = 0.0, IBP1_Bypass_V1 = 0.0, IBP1_Bypass_V2 = 0.0, IBP1_Bypass_V3 = 0.0;
    double IBP1_Out_H = 0.0, IBP1_Out_V1 = 0.0, IBP1_Out_V2 = 0.0, IBP1_Out_V3 = 0.0;
    double IBP1_Out_A1 = 0.0, IBP1_Out_A2 = 0.0, IBP1_Out_A3 = 0.0;
    double IBP1_Out_W1 = 0.0, IBP1_Out_W2 = 0.0, IBP1_Out_W3 = 0.0;
    double IBP1_Bat_V = 0.0, IBP1_Bat_A = 0.0, IBP1_Bat_Zarad = 0.0;

    // --- ИБП 2 ---
    double IBP2_In_H = 0.0, IBP2_In_V1 = 0.0, IBP2_In_V2 = 0.0, IBP2_In_V3 = 0.0;
    double IBP2_In_A1 = 0.0, IBP2_In_A2 = 0.0, IBP2_In_A3 = 0.0;
    double IBP2_In_W1 = 0.0, IBP2_In_W2 = 0.0, IBP2_In_W3 = 0.0;
    double IBP2_Bypass_H = 0.0, IBP2_Bypass_V1 = 0.0, IBP2_Bypass_V2 = 0.0, IBP2_Bypass_V3 = 0.0;
    double IBP2_Out_H = 0.0, IBP2_Out_V1 = 0.0, IBP2_Out_V2 = 0.0, IBP2_Out_V3 = 0.0;
    double IBP2_Out_A1 = 0.0, IBP2_Out_A2 = 0.0, IBP2_Out_A3 = 0.0;
    double IBP2_Out_W1 = 0.0, IBP2_Out_W2 = 0.0, IBP2_Out_W3 = 0.0;
    double IBP2_Bat_V = 0.0, IBP2_Bat_A = 0.0, IBP2_Bat_Zarad = 0.0;

    // --- ИБП 3 ---
    double IBP3_In_H = 0.0, IBP3_In_V1 = 0.0, IBP3_In_V2 = 0.0, IBP3_In_V3 = 0.0;
    double IBP3_In_A1 = 0.0, IBP3_In_A2 = 0.0, IBP3_In_A3 = 0.0;
    double IBP3_In_W1 = 0.0, IBP3_In_W2 = 0.0, IBP3_In_W3 = 0.0;
    double IBP3_Bypass_H = 0.0, IBP3_Bypass_V1 = 0.0, IBP3_Bypass_V2 = 0.0, IBP3_Bypass_V3 = 0.0;
    double IBP3_Out_H = 0.0, IBP3_Out_V1 = 0.0, IBP3_Out_V2 = 0.0, IBP3_Out_V3 = 0.0;
    double IBP3_Out_A1 = 0.0, IBP3_Out_A2 = 0.0, IBP3_Out_A3 = 0.0;
    double IBP3_Out_W1 = 0.0, IBP3_Out_W2 = 0.0, IBP3_Out_W3 = 0.0;
    double IBP3_Bat_V = 0.0, IBP3_Bat_A = 0.0, IBP3_Bat_Zarad = 0.0;

    // --- Температуры ---
    double Temperatura1 = 0.0, Temperatura2 = 0.0;

    // ------------------------------------------------------------------------
    // ОБЩИЕ ПОЛЯ
    // ------------------------------------------------------------------------
    DeviceErrors errorsAll;
    QString ErrorsAll = "";
    bool writeToDb = false;
    QString ProtokolIBP1 = "", ProtokolIBP2 = "", ProtokolIBP3 = "";

    // ✅ Сериализация в JSON для базы данных (ключи точно совпадают с SQL-запросом)
    QJsonObject toJsonObject() const {
        QJsonObject obj;

        // ✅ Передаём время опроса в БД (если установлено).
        //    Если пустое — DatabaseManager подставит CURRENT_TIMESTAMP.
        if (timestamp.isValid()) {
            obj["Дата_и_время"] = timestamp.toString(Qt::ISODate);
        }
        // VV1
        obj["VV1_In_V1"] = VV1_In_V1; obj["VV1_In_V2"] = VV1_In_V2; obj["VV1_In_V3"] = VV1_In_V3;
        obj["VV1_In_A1"] = VV1_In_A1; obj["VV1_In_A2"] = VV1_In_A2; obj["VV1_In_A3"] = VV1_In_A3;
        obj["VV1_In_W1"] = VV1_In_W1; obj["VV1_In_W2"] = VV1_In_W2; obj["VV1_In_W3"] = VV1_In_W3;
        obj["VV1_In_H"] = VV1_In_H; obj["VV1_In_AAll"] = VV1_In_AAll;
        obj["VV1_Out_V1"] = VV1_Out_V1; obj["VV1_Out_V2"] = VV1_Out_V2; obj["VV1_Out_V3"] = VV1_Out_V3;
        obj["VV1_Out_A1"] = VV1_Out_A1; obj["VV1_Out_A2"] = VV1_Out_A2; obj["VV1_Out_A3"] = VV1_Out_A3;
        obj["VV1_Out_W1"] = VV1_Out_W1; obj["VV1_Out_W2"] = VV1_Out_W2; obj["VV1_Out_W3"] = VV1_Out_W3;
        obj["VV1_Out_H"] = VV1_Out_H; obj["VV1_Out_AAll"] = VV1_Out_AAll;

        // VV2
        obj["VV2_In_V1"] = VV2_In_V1; obj["VV2_In_V2"] = VV2_In_V2; obj["VV2_In_V3"] = VV2_In_V3;
        obj["VV2_In_A1"] = VV2_In_A1; obj["VV2_In_A2"] = VV2_In_A2; obj["VV2_In_A3"] = VV2_In_A3;
        obj["VV2_In_W1"] = VV2_In_W1; obj["VV2_In_W2"] = VV2_In_W2; obj["VV2_In_W3"] = VV2_In_W3;
        obj["VV2_In_H"] = VV2_In_H; obj["VV2_In_AAll"] = VV2_In_AAll;
        obj["VV2_Out_V1"] = VV2_Out_V1; obj["VV2_Out_V2"] = VV2_Out_V2; obj["VV2_Out_V3"] = VV2_Out_V3;
        obj["VV2_Out_A1"] = VV2_Out_A1; obj["VV2_Out_A2"] = VV2_Out_A2; obj["VV2_Out_A3"] = VV2_Out_A3;
        obj["VV2_Out_W1"] = VV2_Out_W1; obj["VV2_Out_W2"] = VV2_Out_W2; obj["VV2_Out_W3"] = VV2_Out_W3;
        obj["VV2_Out_H"] = VV2_Out_H; obj["VV2_Out_AAll"] = VV2_Out_AAll;

        // VV3
        obj["VV3_In_V1"] = VV3_In_V1; obj["VV3_In_V2"] = VV3_In_V2; obj["VV3_In_V3"] = VV3_In_V3;
        obj["VV3_In_A1"] = VV3_In_A1; obj["VV3_In_A2"] = VV3_In_A2; obj["VV3_In_A3"] = VV3_In_A3;
        obj["VV3_In_W1"] = VV3_In_W1; obj["VV3_In_W2"] = VV3_In_W2; obj["VV3_In_W3"] = VV3_In_W3;
        obj["VV3_In_H"] = VV3_In_H; obj["VV3_In_AAll"] = VV3_In_AAll;
        obj["VV3_Out_V1"] = VV3_Out_V1; obj["VV3_Out_V2"] = VV3_Out_V2; obj["VV3_Out_V3"] = VV3_Out_V3;
        obj["VV3_Out_A1"] = VV3_Out_A1; obj["VV3_Out_A2"] = VV3_Out_A2; obj["VV3_Out_A3"] = VV3_Out_A3;
        obj["VV3_Out_W1"] = VV3_Out_W1; obj["VV3_Out_W2"] = VV3_Out_W2; obj["VV3_Out_W3"] = VV3_Out_W3;
        obj["VV3_Out_H"] = VV3_Out_H; obj["VV3_Out_AAll"] = VV3_Out_AAll;

        // VV4
        obj["VV4_In_V1"] = VV4_In_V1; obj["VV4_In_V2"] = VV4_In_V2; obj["VV4_In_V3"] = VV4_In_V3;
        obj["VV4_In_A1"] = VV4_In_A1; obj["VV4_In_A2"] = VV4_In_A2; obj["VV4_In_A3"] = VV4_In_A3;
        obj["VV4_In_W1"] = VV4_In_W1; obj["VV4_In_W2"] = VV4_In_W2; obj["VV4_In_W3"] = VV4_In_W3;
        obj["VV4_In_H"] = VV4_In_H; obj["VV4_In_AAll"] = VV4_In_AAll;
        obj["VV4_Out_V1"] = VV4_Out_V1; obj["VV4_Out_V2"] = VV4_Out_V2; obj["VV4_Out_V3"] = VV4_Out_V3;
        obj["VV4_Out_A1"] = VV4_Out_A1; obj["VV4_Out_A2"] = VV4_Out_A2; obj["VV4_Out_A3"] = VV4_Out_A3;
        obj["VV4_Out_W1"] = VV4_Out_W1; obj["VV4_Out_W2"] = VV4_Out_W2; obj["VV4_Out_W3"] = VV4_Out_W3;
        obj["VV4_Out_H"] = VV4_Out_H; obj["VV4_Out_AAll"] = VV4_Out_AAll;

        // ИБП 1
        obj["IBP1_In_H"] = IBP1_In_H; obj["IBP1_In_V1"] = IBP1_In_V1; obj["IBP1_In_V2"] = IBP1_In_V2; obj["IBP1_In_V3"] = IBP1_In_V3;
        obj["IBP1_In_A1"] = IBP1_In_A1; obj["IBP1_In_A2"] = IBP1_In_A2; obj["IBP1_In_A3"] = IBP1_In_A3;
        obj["IBP1_In_W1"] = IBP1_In_W1; obj["IBP1_In_W2"] = IBP1_In_W2; obj["IBP1_In_W3"] = IBP1_In_W3;
        obj["IBP1_Bypass_H"] = IBP1_Bypass_H; obj["IBP1_Bypass_V1"] = IBP1_Bypass_V1; obj["IBP1_Bypass_V2"] = IBP1_Bypass_V2; obj["IBP1_Bypass_V3"] = IBP1_Bypass_V3;
        obj["IBP1_Out_H"] = IBP1_Out_H; obj["IBP1_Out_V1"] = IBP1_Out_V1; obj["IBP1_Out_V2"] = IBP1_Out_V2; obj["IBP1_Out_V3"] = IBP1_Out_V3;
        obj["IBP1_Out_A1"] = IBP1_Out_A1; obj["IBP1_Out_A2"] = IBP1_Out_A2; obj["IBP1_Out_A3"] = IBP1_Out_A3;
        obj["IBP1_Out_W1"] = IBP1_Out_W1; obj["IBP1_Out_W2"] = IBP1_Out_W2; obj["IBP1_Out_W3"] = IBP1_Out_W3;
        obj["IBP1_Bat_V"] = IBP1_Bat_V; obj["IBP1_Bat_A"] = IBP1_Bat_A; obj["IBP1_Bat_Zarad"] = IBP1_Bat_Zarad;

        // ИБП 2
        obj["IBP2_In_H"] = IBP2_In_H; obj["IBP2_In_V1"] = IBP2_In_V1; obj["IBP2_In_V2"] = IBP2_In_V2; obj["IBP2_In_V3"] = IBP2_In_V3;
        obj["IBP2_In_A1"] = IBP2_In_A1; obj["IBP2_In_A2"] = IBP2_In_A2; obj["IBP2_In_A3"] = IBP2_In_A3;
        obj["IBP2_In_W1"] = IBP2_In_W1; obj["IBP2_In_W2"] = IBP2_In_W2; obj["IBP2_In_W3"] = IBP2_In_W3;
        obj["IBP2_Bypass_H"] = IBP2_Bypass_H; obj["IBP2_Bypass_V1"] = IBP2_Bypass_V1; obj["IBP2_Bypass_V2"] = IBP2_Bypass_V2; obj["IBP2_Bypass_V3"] = IBP2_Bypass_V3;
        obj["IBP2_Out_H"] = IBP2_Out_H; obj["IBP2_Out_V1"] = IBP2_Out_V1; obj["IBP2_Out_V2"] = IBP2_Out_V2; obj["IBP2_Out_V3"] = IBP2_Out_V3;
        obj["IBP2_Out_A1"] = IBP2_Out_A1; obj["IBP2_Out_A2"] = IBP2_Out_A2; obj["IBP2_Out_A3"] = IBP2_Out_A3;
        obj["IBP2_Out_W1"] = IBP2_Out_W1; obj["IBP2_Out_W2"] = IBP2_Out_W2; obj["IBP2_Out_W3"] = IBP2_Out_W3;
        obj["IBP2_Bat_V"] = IBP2_Bat_V; obj["IBP2_Bat_A"] = IBP2_Bat_A; obj["IBP2_Bat_Zarad"] = IBP2_Bat_Zarad;

        // ИБП 3
        obj["IBP3_In_H"] = IBP3_In_H; obj["IBP3_In_V1"] = IBP3_In_V1; obj["IBP3_In_V2"] = IBP3_In_V2; obj["IBP3_In_V3"] = IBP3_In_V3;
        obj["IBP3_In_A1"] = IBP3_In_A1; obj["IBP3_In_A2"] = IBP3_In_A2; obj["IBP3_In_A3"] = IBP3_In_A3;
        obj["IBP3_In_W1"] = IBP3_In_W1; obj["IBP3_In_W2"] = IBP3_In_W2; obj["IBP3_In_W3"] = IBP3_In_W3;
        obj["IBP3_Bypass_H"] = IBP3_Bypass_H; obj["IBP3_Bypass_V1"] = IBP3_Bypass_V1; obj["IBP3_Bypass_V2"] = IBP3_Bypass_V2; obj["IBP3_Bypass_V3"] = IBP3_Bypass_V3;
        obj["IBP3_Out_H"] = IBP3_Out_H; obj["IBP3_Out_V1"] = IBP3_Out_V1; obj["IBP3_Out_V2"] = IBP3_Out_V2; obj["IBP3_Out_V3"] = IBP3_Out_V3;
        obj["IBP3_Out_A1"] = IBP3_Out_A1; obj["IBP3_Out_A2"] = IBP3_Out_A2; obj["IBP3_Out_A3"] = IBP3_Out_A3;
        obj["IBP3_Out_W1"] = IBP3_Out_W1; obj["IBP3_Out_W2"] = IBP3_Out_W2; obj["IBP3_Out_W3"] = IBP3_Out_W3;
        obj["IBP3_Bat_V"] = IBP3_Bat_V; obj["IBP3_Bat_A"] = IBP3_Bat_A; obj["IBP3_Bat_Zarad"] = IBP3_Bat_Zarad;

        // Общие поля
        obj["Temperatura1"] = Temperatura1;
        obj["Temperatura2"] = Temperatura2;
        obj["ErrorsAll"] = ErrorsAll;
        obj["write_to_db"] = writeToDb;
        obj["ProtokolIBP1"] = ProtokolIBP1;
        obj["ProtokolIBP2"] = ProtokolIBP2;
        obj["ProtokolIBP3"] = ProtokolIBP3;

        return obj;
    }
};

// ============================================================================
// ✅ Структура данных для Антенны (Antenna) - ПОЛНЫЙ СПИСОК ПОЛЕЙ
// ============================================================================
struct AntennaData {
    QDateTime timestamp;
    // --- Базовые параметры антенны ---
    double Azimuth = 0.0;            // Азимут
    double Elevation = 0.0;          // Угол места
    double Polarization = 0.0;       // Угол поляризации
    double SignalLevel = 0.0;        // Уровень сигнала
    QString Sputnik = "";            // Спутник
    QString Status = "";             // Статус
    int ObogrevTormozov = 0;         // Обогрев тормозов
    int PitaniePrivodov = 0;         // Питание приводов
    int AntErrors = 0;               // Коды ошибок антенны
    QString Komanda = "";            // Последняя команда
    QString ProtokolKontroller = ""; // Протокол контроллера
    QString ProtokolPilot = "";      // Протокол пилот-сигнала
    QString ProtokolDatAzimut = "";  // Протокол датчика азимута
    QString ProtokolDatUgol = "";    // Протокол датчика угла
    QString ProtokolPosAzimut = "";  // Протокол позиционера азимута
    QString ProtokolPosUgol = "";    // Протокол позиционера угла

    // --- ИБП 1 ---
    double IBP1_V = 0.0, IBP1_A1 = 0.0, IBP1_W1 = 0.0;
    double IBP1_A2 = 0.0, IBP1_W2 = 0.0, IBP1_A3 = 0.0, IBP1_W3 = 0.0;
    double IBP1_A4 = 0.0, IBP1_W4 = 0.0, IBP1_A5 = 0.0, IBP1_W5 = 0.0;
    double IBP1_A6 = 0.0, IBP1_W6 = 0.0;

    // --- ИБП 2 ---
    double IBP2_V = 0.0, IBP2_A1 = 0.0, IBP2_W1 = 0.0;
    double IBP2_A2 = 0.0, IBP2_W2 = 0.0, IBP2_A3 = 0.0, IBP2_W3 = 0.0;
    double IBP2_A4 = 0.0, IBP2_W4 = 0.0, IBP2_A5 = 0.0, IBP2_W5 = 0.0;
    double IBP2_A6 = 0.0, IBP2_W6 = 0.0;

    // --- АВР (Автоматический Ввод Резерва) ---
    double AV_V1 = 0.0, AV_V2 = 0.0, AV_V3 = 0.0, AV_V4 = 0.0;
    double AV_V5 = 0.0, AV_V6 = 0.0, AV_V7 = 0.0, AV_V8 = 0.0;

    // --- Дискретные входы ---
    int DV_D1 = -1, DV_D2 = -1, DV_D3 = -1, DV_D4 = -1;
    int DV_D5 = -1, DV_D6 = -1, DV_D7 = -1, DV_D8 = -1;
    int DV_D9 = -1, DV_D10 = -1, DV_D11 = -1, DV_D12 = -1;
    int DV_D13 = -1, DV_D14 = -1, DV_D15 = -1, DV_D16 = -1;

    // --- Температуры ---
    double Temperatura1 = 0.0, Temperatura2 = 0.0, Temperatura3 = 0.0, Temperatura4 = 0.0;
    double Temperatura5 = 0.0, Temperatura6 = 0.0, Temperatura7 = 0.0, Temperatura8 = 0.0;

    // --- Обогрев (12 групп по 3 параметра: V, A, W) ---
    double Obogrev_V1 = 0.0, Obogrev_A1 = 0.0, Obogrev_W1 = 0.0;
    double Obogrev_V2 = 0.0, Obogrev_A2 = 0.0, Obogrev_W2 = 0.0;
    double Obogrev_V3 = 0.0, Obogrev_A3 = 0.0, Obogrev_W3 = 0.0;
    double Obogrev_V4 = 0.0, Obogrev_A4 = 0.0, Obogrev_W4 = 0.0;
    double Obogrev_V5 = 0.0, Obogrev_A5 = 0.0, Obogrev_W5 = 0.0;
    double Obogrev_V6 = 0.0, Obogrev_A6 = 0.0, Obogrev_W6 = 0.0;
    double Obogrev_V7 = 0.0, Obogrev_A7 = 0.0, Obogrev_W7 = 0.0;
    double Obogrev_V8 = 0.0, Obogrev_A8 = 0.0, Obogrev_W8 = 0.0;
    double Obogrev_V9 = 0.0, Obogrev_A9 = 0.0, Obogrev_W9 = 0.0;
    double Obogrev_V10 = 0.0, Obogrev_A10 = 0.0, Obogrev_W10 = 0.0;
    double Obogrev_V11 = 0.0, Obogrev_A11 = 0.0, Obogrev_W11 = 0.0;
    double Obogrev_V12 = 0.0, Obogrev_A12 = 0.0, Obogrev_W12 = 0.0;

    // ------------------------------------------------------------------------
    // ОБЩИЕ ПОЛЯ
    // ------------------------------------------------------------------------
    DeviceErrors errorsAll;
    QString ErrorsAll = "";
    bool writeToDb = false;

    // ✅ Сериализация в JSON для базы данных
    // Ключи точно совпадают с именами столбцов в таблице и плейсхолдерами в SQL
    QJsonObject toJsonObject() const {
        QJsonObject obj;

        // ✅ Передаём время опроса в БД (если установлено).
        //    Если пустое — DatabaseManager подставит CURRENT_TIMESTAMP.
        if (timestamp.isValid()) {
            obj["Дата_и_время"] = timestamp.toString(Qt::ISODate);
        }
        // Базовые параметры
        obj["Азимут"] = Azimuth;
        obj["Угол_места"] = Elevation;
        obj["Угол_поляризации"] = Polarization;
        obj["Уровень_сигнала"] = SignalLevel;
        obj["Спутник"] = Sputnik;
        obj["Статус"] = Status;
        obj["ОбогревТормозов"] = ObogrevTormozov;
        obj["ПитаниеПриводов"] = PitaniePrivodov;
        obj["AntErrors"] = AntErrors;
        obj["Komanda"] = Komanda;
        obj["ErrorsAll"] = ErrorsAll;
        obj["write_to_db"] = writeToDb;
        obj["ProtokolKontroller"] = ProtokolKontroller;
        obj["ProtokolPilot"] = ProtokolPilot;
        obj["ProtokolDatAzimut"] = ProtokolDatAzimut;
        obj["ProtokolDatUgol"] = ProtokolDatUgol;
        obj["ProtokolPosAzimut"] = ProtokolPosAzimut;
        obj["ProtokolPosUgol"] = ProtokolPosUgol;

        // ИБП 1
        obj["IBP1_V"] = IBP1_V; obj["IBP1_A1"] = IBP1_A1; obj["IBP1_W1"] = IBP1_W1;
        obj["IBP1_A2"] = IBP1_A2; obj["IBP1_W2"] = IBP1_W2; obj["IBP1_A3"] = IBP1_A3; obj["IBP1_W3"] = IBP1_W3;
        obj["IBP1_A4"] = IBP1_A4; obj["IBP1_W4"] = IBP1_W4; obj["IBP1_A5"] = IBP1_A5; obj["IBP1_W5"] = IBP1_W5;
        obj["IBP1_A6"] = IBP1_A6; obj["IBP1_W6"] = IBP1_W6;

        // ИБП 2
        obj["IBP2_V"] = IBP2_V; obj["IBP2_A1"] = IBP2_A1; obj["IBP2_W1"] = IBP2_W1;
        obj["IBP2_A2"] = IBP2_A2; obj["IBP2_W2"] = IBP2_W2; obj["IBP2_A3"] = IBP2_A3; obj["IBP2_W3"] = IBP2_W3;
        obj["IBP2_A4"] = IBP2_A4; obj["IBP2_W4"] = IBP2_W4; obj["IBP2_A5"] = IBP2_A5; obj["IBP2_W5"] = IBP2_W5;
        obj["IBP2_A6"] = IBP2_A6; obj["IBP2_W6"] = IBP2_W6;

        // АВР
        obj["AV_V1"] = AV_V1; obj["AV_V2"] = AV_V2; obj["AV_V3"] = AV_V3; obj["AV_V4"] = AV_V4;
        obj["AV_V5"] = AV_V5; obj["AV_V6"] = AV_V6; obj["AV_V7"] = AV_V7; obj["AV_V8"] = AV_V8;

        // Дискретные входы
        obj["DV_D1"] = DV_D1; obj["DV_D2"] = DV_D2; obj["DV_D3"] = DV_D3; obj["DV_D4"] = DV_D4;
        obj["DV_D5"] = DV_D5; obj["DV_D6"] = DV_D6; obj["DV_D7"] = DV_D7; obj["DV_D8"] = DV_D8;
        obj["DV_D9"] = DV_D9; obj["DV_D10"] = DV_D10; obj["DV_D11"] = DV_D11; obj["DV_D12"] = DV_D12;
        obj["DV_D13"] = DV_D13; obj["DV_D14"] = DV_D14; obj["DV_D15"] = DV_D15; obj["DV_D16"] = DV_D16;

        // Температуры
        obj["Temperatura1"] = Temperatura1; obj["Temperatura2"] = Temperatura2;
        obj["Temperatura3"] = Temperatura3; obj["Temperatura4"] = Temperatura4;
        obj["Temperatura5"] = Temperatura5; obj["Temperatura6"] = Temperatura6;
        obj["Temperatura7"] = Temperatura7; obj["Temperatura8"] = Temperatura8;

        // Обогрев
        obj["Obogrev_V1"] = Obogrev_V1; obj["Obogrev_A1"] = Obogrev_A1; obj["Obogrev_W1"] = Obogrev_W1;
        obj["Obogrev_V2"] = Obogrev_V2; obj["Obogrev_A2"] = Obogrev_A2; obj["Obogrev_W2"] = Obogrev_W2;
        obj["Obogrev_V3"] = Obogrev_V3; obj["Obogrev_A3"] = Obogrev_A3; obj["Obogrev_W3"] = Obogrev_W3;
        obj["Obogrev_V4"] = Obogrev_V4; obj["Obogrev_A4"] = Obogrev_A4; obj["Obogrev_W4"] = Obogrev_W4;
        obj["Obogrev_V5"] = Obogrev_V5; obj["Obogrev_A5"] = Obogrev_A5; obj["Obogrev_W5"] = Obogrev_W5;
        obj["Obogrev_V6"] = Obogrev_V6; obj["Obogrev_A6"] = Obogrev_A6; obj["Obogrev_W6"] = Obogrev_W6;
        obj["Obogrev_V7"] = Obogrev_V7; obj["Obogrev_A7"] = Obogrev_A7; obj["Obogrev_W7"] = Obogrev_W7;
        obj["Obogrev_V8"] = Obogrev_V8; obj["Obogrev_A8"] = Obogrev_A8; obj["Obogrev_W8"] = Obogrev_W8;
        obj["Obogrev_V9"] = Obogrev_V9; obj["Obogrev_A9"] = Obogrev_A9; obj["Obogrev_W9"] = Obogrev_W9;
        obj["Obogrev_V10"] = Obogrev_V10; obj["Obogrev_A10"] = Obogrev_A10; obj["Obogrev_W10"] = Obogrev_W10;
        obj["Obogrev_V11"] = Obogrev_V11; obj["Obogrev_A11"] = Obogrev_A11; obj["Obogrev_W11"] = Obogrev_W11;
        obj["Obogrev_V12"] = Obogrev_V12; obj["Obogrev_A12"] = Obogrev_A12; obj["Obogrev_W12"] = Obogrev_W12;

        return obj;
    }
};


#endif // DEVICEDATA_H
