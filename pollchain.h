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
        , m_inRecoveryMode(false)
        , m_failedAttempts(0)
    {
        qDebug() << "[PollChain" << m_chainId << "] Создана цепочка (Pool: Fast)";
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

    void addDriver(std::unique_ptr<IDeviceDriver> driver, const EquipmentConnection &conn)
    {
        driver->setConnectionParams(conn);
        DriverInfo info;
        info.driver = std::move(driver);
        info.connection = conn;
        m_drivers.push_back(std::move(info));
        qDebug() << "[PollChain" << m_chainId << "] Добавлено устройство:"
                 << conn.name << "(deviceId:" << conn.deviceId << ")";
    }

    int deviceCount() const { return static_cast<int>(m_drivers.size()); }

    void enqueueCommand(int deviceIndex, const ControlCommand &cmd)
    {
        QMutexLocker locker(&m_commandMutex);
        PendingCommand pending;
        pending.deviceIndex = deviceIndex;
        pending.command = cmd;
        m_commandQueue.enqueue(pending);
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

        bool chainHasConnectionLost = false;

        for (int i = 0; i < static_cast<int>(m_drivers.size()); ++i) {
            if (m_stopped) break;

            auto &info = m_drivers[i];
            if (!info.driver) continue;

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
                bool success = info.driver->executeCommand(pendingCmd);
                emit commandExecuted(m_chainId, i, success,
                                     success ? "OK" : "Ошибка");
            } else {
                if (info.driver->isConnected()) {
                    auto data = info.driver->poll();
                    if (data) {
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

        if (chainHasConnectionLost) {
            if (m_inRecoveryMode) {
                m_failedAttempts++;
                resetSuccessCount();
                if (m_failedAttempts >= MAX_FAILED_ATTEMPTS) {
                    m_inRecoveryMode = false;
                    emit connectionLost(m_chainId);
                }
            } else {
                resetSuccessCount();
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
    };

    struct PendingCommand {
        int deviceIndex;
        ControlCommand command;
    };

    int m_chainId;
    bool m_stopped;
    std::vector<DriverInfo> m_drivers;
    QQueue<PendingCommand> m_commandQueue;
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
