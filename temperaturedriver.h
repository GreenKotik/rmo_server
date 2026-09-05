#ifndef TEMPERATUREDRIVER_H
#define TEMPERATUREDRIVER_H

#include "idevicedriver.h"
#include <QRandomGenerator>
#include <QDebug>

// ============================================================================
// ✅ ТЕСТОВЫЙ ДРАЙВЕР ДЛЯ ДАТЧИКА ТЕМПЕРАТУРЫ
// ============================================================================
class TemperatureDriver : public IDeviceDriver
{
    Q_OBJECT
public:
    explicit TemperatureDriver(int deviceId, QObject *parent = nullptr)
        : IDeviceDriver(deviceId, parent)
    {
        qDebug() << "[TemperatureDriver] Создан драйвер для deviceId:" << deviceId;
    }

    bool connect() override
    {
        // ✅ Имитация подключения (в будущем — реальное подключение к устройству)
        m_connected = true;
        emit connected(m_deviceId);
        qDebug() << "[TemperatureDriver] Подключено (имитация)";
        return true;
    }

    void disconnect() override
    {
        m_connected = false;
        emit disconnected(m_deviceId);
        qDebug() << "[TemperatureDriver] Отключено";
    }

    std::shared_ptr<IDeviceData> poll() override
    {
        if (!m_connected) {
            qWarning() << "[TemperatureDriver] Устройство не подключено!";
            return nullptr;
        }

        // ✅ Создаём структуру данных
        auto data = std::make_shared<TemperaturaData>(m_deviceId);

        // ✅ Имитация получения температуры (случайное значение от 15 до 35°C)
//        data->dTemperatura = 15.0 + QRandomGenerator::global()->bounded(20.0);

        // ✅ Имитация проверки ошибок (10% шанс аварии)
        if (QRandomGenerator::global()->bounded(10) == 0) {
            data->errors.addCode(250); // Код 250 = "Нет связи" (из AlarmRegistry)
            qWarning() << "[TemperatureDriver] Обнаружена авария: код 250";
        }

        emit pollCompleted(m_deviceId, data);
        return data;
    }

    bool executeCommand(const ControlCommand &cmd) override
    {
        // ✅ Имитация выполнения команды
        qDebug() << "[TemperatureDriver] Выполнение команды:" << cmd.commandCode;

        // В будущем здесь будет реальная отправка команды на устройство
        emit commandExecuted(m_deviceId, true, "Команда выполнена (имитация)");
        return true;
    }
};

#endif // TEMPERATUREDRIVER_H
