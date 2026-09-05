#ifndef DEVICEDATA_H
#define DEVICEDATA_H

#include <QString>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QVariant>
#include <QMap>
#include <vector>
#include <memory>
#include <QDebug>              // ✅ Для qWarning() в createDeviceData()
#include "deviceids.h"
#include "alarmregistry.h"     // ✅ НОВОЕ: для AlarmRegistry::instance()

// ============================================================================
// ✅ СТРУКТУРА ДЛЯ ХРАНЕНИЯ КОДОВ ОШИБОК (Динамический размер)
// ============================================================================
struct DeviceErrors {
    int count = 0;                      // Количество аварий (аналог DecodeErrors[0])
    std::vector<int> codes;             // Сами коды аварий (аналог DecodeErrors[1..N])

    // ✅ Добавить код ошибки (автоматически обновляет count)
    void addCode(int code) {
        codes.push_back(code);
        count = static_cast<int>(codes.size());
    }

    // ✅ Очистить все ошибки
    void clear() {
        codes.clear();
        count = 0;
    }

    // ✅ Проверить наличие ошибок со Status=1 (критические)
    bool hasCriticalErrors() const {
        for (int code : codes) {
            if (AlarmRegistry::instance().getStatus(code) == 1) {
                return true;
            }
        }
        return false;
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

// --- 2. Данные датчика температуры (аналог StructDataTemperatura) ---
// --- Унифицированные данные температуры (для Boreas 3 и TE Monitor V6) ---
struct TemperaturaData : public IDeviceData {
    int m_deviceId;

    // ✅ Расширено до 8 датчиков (максимум для TE Monitor V6)
    // Для Boreas 3 используются только индексы 0-3
    double temperatures[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int alarms[8] = {0, 0, 0, 0, 0, 0, 0, 0};  // Статус каждого датчика
    int activeCount = 0;  // Количество активных датчиков (4 для Boreas, 8 для TE Monitor)

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
        obj["activeCount"] = activeCount;
        QJsonArray tempArray;
        QJsonArray alarmArray;
        for (int i = 0; i < activeCount; ++i) {
            tempArray.append(temperatures[i]);
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

// --- Данные датчика температуры Boreas 3 (4 датчика + 4 статуса) ---
struct Boreas3Data : public IDeviceData {
    int m_deviceId;
    double temperatures[4] = {0.0, 0.0, 0.0, 0.0};
    int alarms[4] = {0, 0, 0, 0};
    DeviceErrors errors;

    Boreas3Data(int deviceId) : m_deviceId(deviceId) {}

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
        for (int i = 0; i < 4; ++i) tempArray.append(temperatures[i]);
        obj["temperatures"] = tempArray;
        QJsonArray alarmArray;
        for (int i = 0; i < 4; ++i) alarmArray.append(alarms[i]);
        obj["alarms"] = alarmArray;
        QJsonArray errArray;
        for (int code : errors.codes) errArray.append(code);
        obj["errors"] = errArray;
        return obj;
    }
};

// --- Данные датчика температуры TE Monitor V6 (8 датчиков + битовая маска аварий) ---
struct TEMonitorV6Data : public IDeviceData {
    int m_deviceId;
    double temperatures[8] = {0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0};
    int alarmStatus = 0;  // Битовая маска: бит i = датчик i+1
    DeviceErrors errors;

    TEMonitorV6Data(int deviceId) : m_deviceId(deviceId) {}

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
        for (int i = 0; i < 8; ++i) tempArray.append(temperatures[i]);
        obj["temperatures"] = tempArray;
        obj["alarmStatus"] = alarmStatus;
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

    // Монитор 1
    double usefulBitRate1 = 0.0;    // Mbit/s
    double totalBitRate1 = 0.0;     // Mbit/s
    double outputLevel1 = 0.0;      // dBm
    bool asiSyncLost1 = false;
    bool rfOutputOk1 = false;

    // Монитор 2
    double usefulBitRate2 = 0.0;    // Mbit/s
    double totalBitRate2 = 0.0;     // Mbit/s
    double outputLevel2 = 0.0;      // dBm
    bool asiSyncLost2 = false;
    bool rfOutputOk2 = false;

    // Переключатель (SW)
    enum class SwMode { Unknown, Local, Remote, Auto };
    SwMode swMode = SwMode::Unknown;
    int activeModulator = 0;        // 1 или 2
    bool power1On = false;
    bool power2On = false;
    bool swFault = false;

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
        obj["usefulBitRate1"] = usefulBitRate1;
        obj["totalBitRate1"] = totalBitRate1;
        obj["outputLevel1"] = outputLevel1;
        obj["asiSyncLost1"] = asiSyncLost1;
        obj["rfOutputOk1"] = rfOutputOk1;
        obj["usefulBitRate2"] = usefulBitRate2;
        obj["totalBitRate2"] = totalBitRate2;
        obj["outputLevel2"] = outputLevel2;
        obj["asiSyncLost2"] = asiSyncLost2;
        obj["rfOutputOk2"] = rfOutputOk2;
        obj["swMode"] = static_cast<int>(swMode);
        obj["activeModulator"] = activeModulator;
        obj["power1On"] = power1On;
        obj["power2On"] = power2On;
        obj["swFault"] = swFault;
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
    if (deviceId == ID_TEMP_SNS_BOREAS_3) {
        return std::make_shared<Boreas3Data>(deviceId);
    }

    if (deviceId == ID_TEMP_SNS_TE_MONITOR_V6) {
        return std::make_shared<TEMonitorV6Data>(deviceId);
    }

    // Если тип неизвестен или специфичен, можно вернуть заглушку или расширить фабрику
    qWarning() << "[DeviceData] Неизвестный тип устройства для deviceId:" << deviceId;
    return nullptr;
}

#endif // DEVICEDATA_H
