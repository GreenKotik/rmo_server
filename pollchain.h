#ifndef POLLCHAIN_H
#define POLLCHAIN_H

#include <QObject>
#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QDebug>
#include <memory>
#include <vector>
#include "idevicedriver.h"
#include "devicedata.h"
#include "basedocwidget.h" // Для EquipmentConnection

// ============================================================================
// ✅ НОВЫЕ ТИПЫ ДЛЯ УПРАВЛЕНИЯ ПУЛАМИ
// ============================================================================
enum class PoolType { Fast, Slow };
enum class SlowReason { Manual, ConnectionLost };

// ============================================================================
// ✅ ЦЕПОЧКА ОПРОСА (работает в отдельном потоке)
// ============================================================================
class PollChain : public QObject
{
    Q_OBJECT
public:
    explicit PollChain(int chainId, QObject *parent = nullptr)
        : QObject(parent)
        , m_chainId(chainId)
        , m_stopped(false)
        , m_poolType(PoolType::Fast)
        , m_slowReason(SlowReason::Manual)
        , m_successCount(0)
    {
        qDebug() << "[PollChain" << m_chainId << "] Создана цепочка опроса (Pool:"
                 << (m_poolType == PoolType::Fast ? "Fast" : "Slow") << ")";
    }

    ~PollChain()
    {
        qDebug() << "[PollChain" << m_chainId << "] Уничтожена";
    }

    // ✅ Добавить устройство в цепочку (в порядке опроса)
    void addDriver(std::unique_ptr<IDeviceDriver> driver, const EquipmentConnection &conn)
    {
        DriverInfo info;
        info.driver = std::move(driver);
        info.connection = conn;
        m_drivers.push_back(std::move(info));
        qDebug() << "[PollChain" << m_chainId << "] Добавлено устройство:"
                 << conn.name << "(deviceId:" << conn.deviceId << ")";
    }

    // ✅ Получить количество устройств в цепочке
    int deviceCount() const { return static_cast<int>(m_drivers.size()); }

    // ✅ Добавить команду управления в очередь
    void enqueueCommand(int deviceIndex, const ControlCommand &cmd)
    {
        QMutexLocker locker(&m_commandMutex);
        PendingCommand pending;
        pending.deviceIndex = deviceIndex;
        pending.command = cmd;
        m_commandQueue.enqueue(pending);
        qDebug() << "[PollChain" << m_chainId << "] Команда добавлена в очередь для устройства"
                 << deviceIndex << "(код:" << cmd.commandCode << ")";
    }

    // ✅ Остановить цепочку (вызывается перед удалением)
    Q_INVOKABLE void stop()
    {
        m_stopped = true;
        qDebug() << "[PollChain" << m_chainId << "] Остановка запрошена";
    }

    // ✅ Подключить все устройства (вызывается один раз при старте)
    Q_INVOKABLE void connectAllDevices()
    {
        for (auto &info : m_drivers) {
            if (info.driver && !info.driver->isConnected()) {
                if (info.driver->connect()) {
                    qDebug() << "[PollChain" << m_chainId << "] Подключено:" << info.connection.name;
                } else {
                    qWarning() << "[PollChain" << m_chainId << "] Ошибка подключения:"
                               << info.connection.name;
                    // Добавляем аварию "Нет связи"
                    auto errorData = createDeviceData(info.connection.deviceId);
                    if (errorData) {
                        errorData->getErrors().addCode(250); // Код 250 = "Нет связи"
                        emit pollResult(m_chainId, 0, errorData);
                    }
                }
            }
        }
    }

    // =========================================================================
    // ✅ НОВЫЕ МЕТОДЫ ДЛЯ УПРАВЛЕНИЯ ПУЛАМИ
    // =========================================================================
    void setPoolType(PoolType type) { m_poolType = type; }
    PoolType getPoolType() const { return m_poolType; }

    void setSlowReason(SlowReason reason) { m_slowReason = reason; }
    SlowReason getSlowReason() const { return m_slowReason; }

    void incrementSuccessCount() { m_successCount++; }
    void resetSuccessCount() { m_successCount = 0; }
    int getSuccessCount() const { return m_successCount; }
    // ✅ НОВЫЙ МЕТОД: Получить ID цепочки
    int getId() const { return m_chainId; }

    // ✅ НОВЫЕ МЕТОДЫ ДЛЯ УПРАВЛЕНИЯ РЕЖИМОМ ВОССТАНОВЛЕНИЯ
    void setRecoveryMode(bool enabled) { m_inRecoveryMode = enabled; }
    bool isInRecoveryMode() const { return m_inRecoveryMode; }

    void resetFailedAttempts() { m_failedAttempts = 0; }
    int getFailedAttempts() const { return m_failedAttempts; }

signals:
    // ✅ Результат опроса устройства
    void pollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data);

    // ✅ Результат выполнения команды
    void commandExecuted(int chainId, int deviceIndex, bool success, const QString &message);

    // ✅ Цикл опроса завершён
    void cycleFinished(int chainId);

    // ✅ Ошибка
    void errorOccurred(int chainId, int deviceIndex, const QString &error);

    // ✅ НОВЫЙ СИГНАЛ: Обнаружена потеря связи (код 250) в этой цепочке
    void connectionLost(int chainId);

    // ✅ НОВЫЙ СИГНАЛ: Цепочка готова вернуться в Fast Pool (3 успешных опроса подряд)
    void readyToReturnToFast(int chainId);

public slots:
    // ✅ Выполнить один цикл опроса (вызывается из главного потока через invokeMethod)
    void runPollCycle()
    {
        if (m_stopped) {
            qDebug() << "[PollChain" << m_chainId << "] Цепочка остановлена, цикл пропущен";
            return;
        }

        bool chainHasConnectionLost = false;

        // Последовательно опрашиваем все устройства в цепочке
        for (int i = 0; i < static_cast<int>(m_drivers.size()); ++i) {
            if (m_stopped) break;

            auto &info = m_drivers[i];
            if (!info.driver) continue;

            // ✅ Проверяем очередь команд для этого устройства
            ControlCommand pendingCmd;
            bool hasCommand = false;
            {
                QMutexLocker locker(&m_commandMutex);
                for (int j = 0; j < m_commandQueue.size(); ++j) {
                    if (m_commandQueue[j].deviceIndex == i) {
                        pendingCmd = m_commandQueue[j].command;
                        m_commandQueue.removeAt(j);
                        hasCommand = true;
                        break;
                    }
                }
            }

            if (hasCommand) {
                // ✅ Выполняем команду вместо опроса
                qDebug() << "[PollChain" << m_chainId << "] Выполнение команды для устройства"
                         << i << "(код:" << pendingCmd.commandCode << ")";
                bool success = info.driver->executeCommand(pendingCmd);
                emit commandExecuted(m_chainId, i, success,
                    success ? "Команда выполнена" : "Ошибка выполнения команды");
            } else {
                // ✅ Обычный опрос
                if (info.driver->isConnected()) {
                    auto data = info.driver->poll();
                    if (data) {
                        // ✅ ПРОВЕРКА НА ПОТЕРЮ СВЯЗИ (код 250)
                        if (data->hasErrors()) {
                            const DeviceErrors &errors = data->getErrors();
                            for (int code : errors.codes) {
                                if (code == 250) { // 250 = "Нет связи"
                                    chainHasConnectionLost = true;
                                    break;
                                }
                            }
                        }
                        emit pollResult(m_chainId, i, data);
                    } else {
                        emit errorOccurred(m_chainId, i, "Получены пустые данные от устройства");
                    }
                } else {
                    // Устройство не подключено — пробуем переподключиться
                    if (info.driver->connect()) {
                        auto data = info.driver->poll();
                        if (data) {
                            if (data->hasErrors()) {
                                const DeviceErrors &errors = data->getErrors();
                                for (int code : errors.codes) {
                                    if (code == 250) {
                                        chainHasConnectionLost = true;
                                        break;
                                    }
                                }
                            }
                            emit pollResult(m_chainId, i, data);
                        }
                    } else {
                        emit errorOccurred(m_chainId, i, "Устройство не подключено");
                        chainHasConnectionLost = true; // Не удалось подключиться = потеря связи
                    }
                }
            }
        }

        // ✅ ЛОГИКА УПРАВЛЕНИЯ СЧЁТЧИКОМ УСПЕШНЫХ ОПРОСОВ С ПЕРИОДОМ ОХЛАЖДЕНИЯ
        if (chainHasConnectionLost) {
            // ✅ Потеря связи
            if (m_inRecoveryMode) {
                // ✅ В режиме восстановления — НЕ мигрируем сразу, даём ещё попытки
                m_failedAttempts++;
                resetSuccessCount();

                qDebug() << "[PollChain" << m_chainId << "] Ошибка в режиме восстановления, попытка"
                         << m_failedAttempts << "из" << MAX_FAILED_ATTEMPTS;

                if (m_failedAttempts >= MAX_FAILED_ATTEMPTS) {
                    // ✅ Превышен лимит попыток — мигрируем в Slow Pool
                    m_inRecoveryMode = false;  // ✅ Сбрасываем флаг
                    emit connectionLost(m_chainId);
                    qDebug() << "[PollChain" << m_chainId << "] Превышен лимит попыток ("
                             << MAX_FAILED_ATTEMPTS << "), миграция в Slow Pool";
                }
                // ✅ НЕ отправляем connectionLost, если лимит не превышен
            } else {
                // ✅ Не в режиме восстановления — мигрируем сразу
                resetSuccessCount();
                emit connectionLost(m_chainId);
            }
        } else {
            // ✅ Успешный опрос
            m_failedAttempts = 0;  // ✅ Сбрасываем счётчик неудачных попыток
            // ✅ НЕ сбрасываем m_inRecoveryMode здесь — он сбрасывается только при миграции
            incrementSuccessCount();

            // ✅ Проверяем, готова ли цепочка вернуться в Fast Pool
            if (m_poolType == PoolType::Slow &&
                m_slowReason == SlowReason::ConnectionLost &&
                m_successCount >= MIN_SUCCESS_COUNT) {
                emit readyToReturnToFast(m_chainId);
                qDebug() << "[PollChain" << m_chainId << "] "
                         << MIN_SUCCESS_COUNT << " успешных опросов подряд, запрос на возврат в Fast Pool";
            }
        }

        emit cycleFinished(m_chainId);
    }

private:
    // ✅ Структура для хранения информации о драйвере
    struct DriverInfo {
        std::unique_ptr<IDeviceDriver> driver;
        EquipmentConnection connection;
    };

    // ✅ Структура для команды в очереди
    struct PendingCommand {
        int deviceIndex;
        ControlCommand command;
    };

    int m_chainId;
    bool m_stopped;
    std::vector<DriverInfo> m_drivers;
    QQueue<PendingCommand> m_commandQueue;
    QMutex m_commandMutex;

    // ✅ НОВЫЕ ПОЛЯ ДЛЯ УПРАВЛЕНИЯ ПУЛАМИ
    PoolType m_poolType;
    SlowReason m_slowReason;
    int m_successCount;

    // ✅ НОВЫЕ ПОЛЯ ДЛЯ ПЕРИОДА ОХЛАЖДЕНИЯ
    static constexpr int MIN_SUCCESS_COUNT = 10;  // ✅ Количество правильных попыток для возврата в Fastpool
    static constexpr int MAX_FAILED_ATTEMPTS = 5;  // ✅ Количество попыток перед повторной миграцией в SlowPool

    bool m_inRecoveryMode = false;  // ✅ Флаг режима восстановления (после возврата в Fast Pool)
    int m_failedAttempts = 0;       // ✅ Счётчик неудачных попыток в режиме восстановления
};

#endif // POLLCHAIN_H
