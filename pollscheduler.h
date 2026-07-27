#ifndef POLL_SCHEDULER_H
#define POLL_SCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QThread>
#include <QDebug>
#include <memory>
#include <vector>
#include <QMetaType>
#include "pollchain.h"
#include "driverfactory.h"

// ✅ Forward declarations (разрываем цикл)
struct EquipmentConnection;
class BaseDocWidget;

// ✅ Регистрируем std::shared_ptr<IDeviceData> для передачи через сигналы Qt
Q_DECLARE_METATYPE(std::shared_ptr<IDeviceData>)

// ============================================================================
// ✅ ПЛАНИРОВЩИК ОПРОСА (управляет двумя пулами цепочек)
// ============================================================================
class PollScheduler : public QObject
{
    Q_OBJECT
public:
    explicit PollScheduler(BaseDocWidget *docWidget, QObject *parent = nullptr);
    ~PollScheduler();

    // ✅ Инициализировать цепочки на основе конфигурации оборудования
    void initializeChains();

    // ✅ Подключить все устройства во всех цепочках
    void connectAllDevices();

    // ✅ Запустить таймер опроса
    void start();

    // ✅ Остановить таймер опроса и все цепочки
    void stop();

    // ✅ Установить интервал опроса
    void setPollingInterval(int msec);

    // ✅ Отправить команду управления в нужную цепочку
    void sendCommand(const QString &deviceName, const ControlCommand &cmd);

    // ✅ НОВЫЙ МЕТОД: Включить/выключить режим Fast (интервал 100 мс)
    void setFastMode(bool enabled);
    bool isFastMode() const { return m_fastMode; }

signals:
    // ✅ Результат опроса устройства
    void pollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data);

    // ✅ Результат выполнения команды
    void commandExecuted(int chainId, int deviceIndex, bool success, const QString &message);

    // ✅ Цикл опроса завершён (вызывается при завершении Fast Pool)
    void allCyclesFinished();

    // ✅ Ошибка
    void errorOccurred(int chainId, int deviceIndex, const QString &error);

private slots:
    // ✅ Запустить один цикл опроса (общий таймер для обоих пулов)
    void startPollCycle();

    // ✅ Результат опроса от цепочки
    void onPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data);

    // ✅ Результат выполнения команды
    void onCommandExecuted(int chainId, int deviceIndex, bool success, const QString &message);

    // ✅ Цепочка Fast Pool завершила цикл
    void onFastCycleFinished(int chainId);

    // ✅ Цепочка Slow Pool завершила цикл
    void onSlowCycleFinished(int chainId);

    // ✅ Ошибка от цепочки
    void onErrorOccurred(int chainId, int deviceIndex, const QString &error);

    // ✅ НОВЫЙ СЛОТ: Обнаружена потеря связи в цепочке
    void onConnectionLost(int chainId);

    // ✅ НОВЫЙ СЛОТ: Цепочка готова вернуться в Fast Pool
    void onReadyToReturnToFast(int chainId);

private:
    // ✅ Вспомогательные методы для миграции цепочек между пулами
    void migrateChainToSlow(PollChain *chain, SlowReason reason);
    void migrateChainToFast(PollChain *chain);

    // ✅ Поиск цепочки по ID (возвращает указатель и пул, в котором она находится)
    PollChain* findChainById(int chainId, PoolType &outPool) const;

    bool m_stopping = false;  // ✅ НОВОЕ: Флаг для предотвращения двойной остановки
    BaseDocWidget *m_docWidget;
    QTimer *m_pollTimer;

    // ✅ ДВА ПУЛА ЦЕПОЧЕК
    std::vector<PollChain*> m_fastChains;   // Быстрый пул
    std::vector<PollChain*> m_slowChains;   // Медленный пул
    std::vector<QThread*> m_threads;        // Все потоки (для обоих пулов)

    // ✅ Счётчики активных цепочек для каждого пула
    int m_fastActiveChains;
    int m_slowActiveChains;

    int m_chainCounter;
    bool m_fastMode;  // ✅ Флаг режима Fast (интервал 100 мс)
};

#endif // POLL_SCHEDULER_H
