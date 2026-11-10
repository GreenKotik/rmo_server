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
#include "basedocwidget.h"

enum class PoolType { Fast, Slow };
enum class SlowReason { Manual, ConnectionLost };
class PollScheduler;

class PollChain : public QObject
{
    Q_OBJECT
public:
    // ✅ НОВЫЙ КОНСТРУКТОР: scheduler + флаг isTimePool
    explicit PollChain(int chainId,
                       PollScheduler *scheduler = nullptr,
                       bool isTimePool = false,
                       QObject *parent = nullptr)
        : QObject(parent)
        , m_chainId(chainId)
        , m_scheduler(scheduler)
        , m_isTimePool(isTimePool)
        , m_stopped(false)
        , m_poolType(PoolType::Fast)
        , m_slowReason(SlowReason::Manual)
        , m_successCount(0)
        , m_inRecoveryMode(false)
        , m_failedAttempts(0)
    {
        if (m_isTimePool) {
            qDebug() << "[PollChain" << m_chainId << "] Создана TIME POOL-цепочка (Pool: Fast)";
        } else {
            qDebug() << "[PollChain" << m_chainId << "] Создана цепочка (Pool: Fast)";
        }
    }

    ~PollChain()
    {
        qDebug() << "[PollChain" << m_chainId << "] Деструктор НАЧАЛО";

        // 1. Останавливаем цепочку
        m_stopped = true;

        // 2. Очищаем очередь команд
        {
            QMutexLocker locker(&m_commandMutex);
            m_commandQueue.clear();
        }

        // 3. Удаляем все драйверы
        for (auto &info : m_drivers) {
            if (info.driver) {
                QString deviceName = info.connection.name;
                info.driver.reset();
                qDebug() << "[PollChain" << m_chainId << "] Драйвер удалён:" << deviceName;
            }
        }
        m_drivers.clear();

        qDebug() << "[PollChain" << m_chainId << "] Деструктор ЗАВЕРШЕНИЕ";
    }

    void addDriver(std::unique_ptr<IDeviceDriver> driver,
                   const EquipmentConnection &conn,
                   int slotIndex)          // ← НОВЫЙ параметр
    {
        driver->setConnectionParams(conn);
        driver->setDeviceSettings(conn.deviceSettings);

        DriverInfo info;
        info.driver = std::move(driver);
        info.connection = conn;
        info.slotIndex = slotIndex;    // ← сохраняем
        m_drivers.push_back(std::move(info));
        qDebug() << "[PollChain" << m_chainId << "] Добавлено устройство:"
                 << conn.name << "(deviceId:" << conn.deviceId
                 << ", slot:" << slotIndex << ")";
    }

    bool hasSlot(int slotIndex) const
    {
        for (const auto &info : m_drivers) {
            if (info.slotIndex == slotIndex) return true;
        }
        return false;
    }

    int deviceCount() const { return static_cast<int>(m_drivers.size()); }

    // ✅ НОВОЕ: Переместить всех драйверов в указанный поток.
    // Должно вызываться ПОСЛЕ moveToThread(chain) из initializeChains.
    // Требует, чтобы у драйверов не было parent (иначе Qt выдаст
    // "Cannot move objects with a parent" и проигнорирует вызов).
    void moveDriversToThread(QThread *thread)
    {
        if (!thread) return;
        for (auto &info : m_drivers) {
            if (info.driver) {
                info.driver->moveToThread(thread);
            }
        }
    }

    // ✅ НОВОЕ: Получить указатели на драйверы (для диагностики/отладки).
    std::vector<IDeviceDriver*> driverPtrs() const
    {
        std::vector<IDeviceDriver*> result;
        result.reserve(m_drivers.size());
        for (const auto &info : m_drivers) {
            result.push_back(info.driver.get());
        }
        return result;
    }

    Q_INVOKABLE void enqueueCommand(const ControlCommand &cmd)
    {
        if (cmd.slotIndex < 0) {
            qWarning() << "[PollChain" << m_chainId << "] Команда без slotIndex, отброшена";
            return;
        }
        QMutexLocker locker(&m_commandMutex);
        m_commandQueue.enqueue(cmd);
    }

    Q_INVOKABLE void stop()
    {
        m_stopped = true;
        qDebug() << "[PollChain" << m_chainId << "] Остановка запрошена";
    }

    Q_INVOKABLE void connectAllDevices()
    {
        qDebug() << "[PollChain" << m_chainId << "] Начало подключения устройств";
        for (size_t i = 0; i < m_drivers.size(); ++i) {
            if (m_stopped) break;
            auto &info = m_drivers[i];
            if (info.driver && !info.driver->isConnected()) {
                if (info.driver->connect()) {
                    qDebug() << "[PollChain" << m_chainId << "] Подключено:" << info.connection.name;
                } else {
                    qWarning() << "[PollChain" << m_chainId << "] Ошибка подключения:"
                               << info.connection.name;
                    // ✅ ИСПРАВЛЕНО: создаём данные об ошибке правильно
                    auto errorData = createDeviceData(info.connection.deviceId);
                    if (errorData) {
                        errorData->setConnectionError();  // ✅ Новый метод вместо getErrors().addCode()
                        emit pollResult(m_chainId, static_cast<int>(i), errorData);
                    }
                }
            }
        }
        qDebug() << "[PollChain" << m_chainId << "] Подключение завершено";
    }

    void setPoolType(PoolType type) { m_poolType = type; }
    PoolType getPoolType() const { return m_poolType; }
    void setSlowReason(SlowReason reason) { m_slowReason = reason; }
    SlowReason getSlowReason() const { return m_slowReason; }
    void incrementSuccessCount() { m_successCount++; }
    void resetSuccessCount() { m_successCount = 0; }
    int getSuccessCount() const { return m_successCount; }
    int getId() const { return m_chainId; }
    void setRecoveryMode(bool enabled) { m_inRecoveryMode = enabled; }
    bool isInRecoveryMode() const { return m_inRecoveryMode; }
    void resetFailedAttempts() { m_failedAttempts = 0; }
    int getFailedAttempts() const { return m_failedAttempts; }
    // ✅ Объявление. Реализация — в pollchain.cpp, чтобы иметь доступ
    //    к полному определению PollScheduler.
    void notifyTimePool(const QDateTime &time);

signals:
    void pollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data);
    void commandExecuted(int chainId, int deviceIndex, bool success, const QString &message);
    void cycleFinished(int chainId);
    void errorOccurred(int chainId, int deviceIndex, const QString &error);
    void connectionLost(int chainId);
    void readyToReturnToFast(int chainId);

public slots:
    void runPollCycle()
    {
        if (m_stopped) return;

        // ✅ НОВОЕ: Time Pool-цепочка фиксирует время начала Fast-тика.
        //    Работает в её собственном рабочем потоке. Драйверов у неё нет,
        //    поэтому цикл по m_drivers ниже просто не выполнится.
        if (m_isTimePool && m_scheduler) {
            notifyTimePool(QDateTime::currentDateTime());
        }

        bool chainHasConnectionLost = false;

        for (int i = 0; i < static_cast<int>(m_drivers.size()); ++i) {
            if (m_stopped) break;

            auto &info = m_drivers[i];
            if (!info.driver) continue;

            // Поиск команды для ЭТОГО слота (по стабильному slotIndex)
            ControlCommand pendingCmd;
            bool hasCommand = false;
            {
                QMutexLocker locker(&m_commandMutex);
                for (int j = 0; j < m_commandQueue.size(); ++j) {
                    if (m_commandQueue[j].slotIndex == info.slotIndex) {
                        pendingCmd = m_commandQueue[j];
                        m_commandQueue.removeAt(j);
                        hasCommand = true;
                        break;
                    }
                }
            }
            if (hasCommand) {
                bool success = info.driver->executeCommand(pendingCmd);
                emit commandExecuted(m_chainId, i, success, success ? "OK" : "Ошибка");
            } else {
                if (info.driver->isConnected()) {
                    auto data = info.driver->poll();
                    if (data) {
                        // ❌ УБРАНО: data->timestamp = cycleStart;
                        if (data->hasErrors()) {
                            for (int code : data->getErrors().codes) {
                                if (code == 250) { chainHasConnectionLost = true; break; }
                            }
                        }
                        emit pollResult(m_chainId, i, data);
                    } else {
                        emit errorOccurred(m_chainId, i, "Пустые данные");
                    }
                } else {
                    if (info.driver->connect()) {
                        auto data = info.driver->poll();
                        if (data) {
                            // ❌ УБРАНО: data->timestamp = cycleStart;
                            if (data->hasErrors()) {
                                for (int code : data->getErrors().codes) {
                                    if (code == 250) { chainHasConnectionLost = true; break; }
                                }
                            }
                            emit pollResult(m_chainId, i, data);
                        }
                    } else {
                        emit errorOccurred(m_chainId, i, "Устройство не подключено");
                        chainHasConnectionLost = true;
                    }
                }
            }
        }

        // ====================================================================
        // Логика переходов Fast↔Slow — БЕЗ ИЗМЕНЕНИЙ.
        // Time Pool никогда сюда не попадёт в ветку Slow:
        //   chainHasConnectionLost = false (нет драйверов),
        //   значит счётчик ошибок не растёт и connectionLost не эмитится.
        // ====================================================================
        if (chainHasConnectionLost) {
            ++m_failedAttempts;
            resetSuccessCount();

            if (m_failedAttempts >= MAX_FAILED_ATTEMPTS) {
                m_failedAttempts = 0;
                emit connectionLost(m_chainId);
            }
        } else {
            m_failedAttempts = 0;
            incrementSuccessCount();

            if (m_poolType == PoolType::Slow &&
                m_slowReason == SlowReason::ConnectionLost &&
                m_successCount >= MIN_SUCCESS_COUNT) {
                emit readyToReturnToFast(m_chainId);
            }
        }
        emit cycleFinished(m_chainId);
    }

private:
    struct DriverInfo {
        std::unique_ptr<IDeviceDriver> driver;
        EquipmentConnection connection;
        int slotIndex = -1;
    };

    struct PendingCommand {
        int deviceIndex;
        ControlCommand command;
    };

    int m_chainId;
    PollScheduler *m_scheduler = nullptr;  // ✅ НОВОЕ
    bool m_isTimePool = false;             // ✅ НОВОЕ
    bool m_stopped;
    std::vector<DriverInfo> m_drivers;
    QQueue<ControlCommand> m_commandQueue;
    QMutex m_commandMutex;
    PoolType m_poolType;
    SlowReason m_slowReason;
    int m_successCount;
    bool m_inRecoveryMode;
    int m_failedAttempts;

    static constexpr int MIN_SUCCESS_COUNT = 10;
    static constexpr int MAX_FAILED_ATTEMPTS = 5;
};

#endif // POLLCHAIN_H
