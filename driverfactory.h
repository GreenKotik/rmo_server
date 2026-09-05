#ifndef DRIVERFACTORY_H
#define DRIVERFACTORY_H

#include <memory>
#include <QDebug>
#include "idevicedriver.h"
#include "deviceids.h"
#include "temperaturedriver.h"
#include "boreas3driver.h"  // ✅ ДОБАВИТЬ
#include "temonitorv6driver.h"
#include "teletehdriver.h"
#include "devswdriver.h"

// ============================================================================
// ✅ ФАБРИКА ДРАЙВЕРОВ УСТРОЙСТВ
// ============================================================================
class DriverFactory
{
public:
    static std::unique_ptr<IDeviceDriver> createDriver(int deviceId, QObject *parent = nullptr)
    {
        switch (deviceId) {
            case  ID_TEMP_SNS_BOREAS_3:
                return std::make_unique<Boreas3Driver>(deviceId, parent);

            case  ID_TEMP_SNS_TE_MONITOR_V6:
                return std::make_unique<TEMonitorV6Driver>(deviceId, parent);

            case ID_MOD_TELETEH_TT_MC_72:
                return std::make_unique<TeletehDriver>(deviceId, parent);

            case ID_MOD_SW_DEV:
                return std::make_unique<DevSwDriver>(deviceId, parent);

            default:
                qWarning() << "[DriverFactory] Драйвер для deviceId" << deviceId << "ещё не реализован.";
                return nullptr;
        }
    }

    // ✅ Получить название категории устройства
    static QString getCategoryName(int deviceId)
    {
        if (IS_ANTENNA_CONTROLLER(deviceId)) return "Контроллер антенны";
        if (IS_ENCODER(deviceId)) return "Энкодер";
        if (IS_POSITIONER(deviceId)) return "Позиционер";
        if (IS_PILOT_RECEIVER(deviceId)) return "Приёмник пилот-сигнала";
        if (IS_TRANSMITTER(deviceId)) return "Передатчик";
        if (IS_TX_SWITCH(deviceId)) return "Свич передатчика";
        if (IS_UP_CONVERTER(deviceId)) return "UP-конвертор";
        if (IS_UPC_SWITCH(deviceId)) return "Свич UP-конвертора";
        if (IS_TEMP_SENSOR(deviceId)) return "Датчик температуры";
        if (IS_TEMP_RECEIVER(deviceId)) return "Приёмник температуры";
        if (IS_DEHYDRATOR(deviceId)) return "Дегидратор";
        if (IS_ANALYZER(deviceId)) return "Анализатор";
        if (IS_UPS(deviceId)) return "ИБП";
        if (IS_CONTROL_POWER(deviceId)) return "Контрольно-силовое оборудование";
        if (IS_MNEMOSCHEMA(deviceId)) return "Мнемосхема";
        if (IS_MODULATOR(deviceId)) return "Модулятор";

        return "Неизвестное устройство";
    }
};

#endif // DRIVERFACTORY_H
