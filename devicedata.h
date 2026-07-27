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
struct TemperaturaData : public IDeviceData {
    int m_deviceId;
    double dTemperatura = 0.0;
    DeviceErrors errors;

    TemperaturaData(int deviceId) : m_deviceId(deviceId) {}

    int deviceId() const override { return m_deviceId; }
    bool hasErrors() const override { return errors.count > 0; }
    DeviceErrors getErrors() const override { return errors; }

    QJsonObject toJson() const override {
        QJsonObject obj;
        obj["deviceId"] = m_deviceId;
        obj["temperature"] = dTemperatura;

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

// ============================================================================
// ✅ ВСПОМОГАТЕЛЬНАЯ ФАБРИКА ДЛЯ СОЗДАНИЯ ПУСТЫХ ОБЪЕКТОВ ДАННЫХ
// ============================================================================
inline std::shared_ptr<IDeviceData> createDeviceData(int deviceId)
{
    // Используем макросы из deviceids.h для определения категории
    if (IS_TRANSMITTER(deviceId) || IS_TX_SWITCH(deviceId)) {
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

    // Если тип неизвестен или специфичен, можно вернуть заглушку или расширить фабрику
    qWarning() << "[DeviceData] Неизвестный тип устройства для deviceId:" << deviceId;
    return nullptr;
}

#endif // DEVICEDATA_H
