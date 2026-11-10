#include "pollscheduler.h"
#include "basedocwidget.h"  // ✅ Включаем здесь, а не в .h

PollScheduler::PollScheduler(BaseDocWidget *docWidget, QObject *parent)
    : QObject(parent)
    , m_docWidget(docWidget)
    , m_pollTimer(new QTimer(this))
    , m_fastActiveChains(0)
    , m_slowActiveChains(0)
    , m_chainCounter(0)
    , m_fastMode(false)
{
    // ✅ Таймер опроса (по умолчанию 1000 мс)
    m_pollTimer->setInterval(1000);
    connect(m_pollTimer, &QTimer::timeout, this, &PollScheduler::startPollCycle);

    qDebug() << "[PollScheduler] Создан планировщик опроса для документа:"
             << (docWidget ? docWidget->filePath() : "unknown");
}

PollScheduler::~PollScheduler()
{
    qDebug() << "[PollScheduler] Деструктор НАЧАЛО";

    // 1. Останавливаем таймер
    if (m_pollTimer) {
        m_pollTimer->stop();
        m_pollTimer->disconnect();
        qDebug() << "[PollScheduler] Таймер остановлен";
    }

    // 2. Останавливаем все потоки
    stop();

    // 3. Удаляем все потоки
    for (QThread *thread : m_threads) {
        if (thread) {
            thread->quit();
            thread->wait(1000);
            delete thread;
        }
    }
    m_threads.clear();

    qDebug() << "[PollScheduler] Деструктор ЗАВЕРШЕНИЕ";
}

void PollScheduler::initializeChains()
{
    // Останавливаем старые цепочки
    stop();

    // ✅ НОВОЕ: Сбрасываем счётчик цепочек, чтобы ID не росли бесконечно
    m_chainCounter = 0;
    m_fastCycleInProgress = false;   // ✅ ДОБАВИТЬ
    m_slowCycleInProgress = false;   // ✅ ДОБАВИТЬ

    // ✅ Сброс Time Pool времени при пересоздании
    {
        QMutexLocker locker(&m_timePoolMutex);
        m_lastTimePoolTime = QDateTime();
    }

    // Получаем подключения из документа
    QList<EquipmentConnection> connections = m_docWidget->getEquipmentConnections();

    qDebug() << "[PollScheduler] Инициализация цепочек, подключений:" << connections.size();

    // Фильтруем только валидные подключения



    // Пары (подключение, исходный индекс в m_equipmentConnections)
    using ConnWithIndex = std::pair<EquipmentConnection, int>;
    QList<ConnWithIndex> validConnections;

    for (int i = 0; i < connections.size(); ++i) {
        const EquipmentConnection &conn = connections[i];
        if (conn.errorCode == 0 && conn.connected && conn.deviceId != ID_UNKNOWN) {
                validConnections.append(std::make_pair(conn, i));
        } else {
            qDebug() << "[PollScheduler] Пропущено устройство:" << conn.name
                     << "(errorCode:" << conn.errorCode
                     << ", connected:" << conn.connected
                     << ", deviceId:" << conn.deviceId << ")";
        }
    }


    if (validConnections.isEmpty()) {
        qWarning() << "[PollScheduler] Нет валидных подключений для опроса!";
        return;
    }

    // Группируем устройства в цепочки
    // Группировка в цепочки — по-прежнему по useStream, но с сохранением индексов
    std::vector<std::vector<ConnWithIndex>> chains;
    std::vector<ConnWithIndex> currentChain;

    for (const ConnWithIndex &pair : validConnections) {
        const EquipmentConnection &conn = pair.first;
        if (conn.useStream || currentChain.empty()) {
            if (!currentChain.empty()) {
                chains.push_back(currentChain);
                currentChain.clear();
            }
        }
        currentChain.push_back(pair);
    }
    if (!currentChain.empty()) chains.push_back(currentChain);



    if (!currentChain.empty()) {
        chains.push_back(currentChain);
    }

    qDebug() << "[PollScheduler] Создано цепочек:" << chains.size();

    // Создаём PollChain для каждой группы
    for (const auto &chainDevices : chains) {
        int chainId = ++m_chainCounter;

        // ✅ Создаём цепочку
        PollChain *chain = new PollChain(chainId);

        // ✅ Драйверы создаются БЕЗ parent (nullptr) — иначе их
        //    нельзя будет переместить в рабочий поток через
        //    moveToThread() (Qt выдаст "Cannot move objects with a parent").
        for (const ConnWithIndex &pair : chainDevices) {
            const EquipmentConnection &conn = pair.first;
            const int slotIndex = pair.second;   // ← исходный индекс в m_equipmentConnections

            auto driver = DriverFactory::createDriver(conn.deviceId, nullptr);
            if (driver) {
                chain->addDriver(std::move(driver), conn, slotIndex);   // ← 3-й аргумент
            } else {
                qWarning() << "[PollScheduler] Не удалось создать драйвер для:"
                           << conn.name << "(deviceId:" << conn.deviceId << ")";
            }
        }
        // ✅ Пропускаем пустые цепочки (нет валидных драйверов)
        if (chain->deviceCount() == 0) {
            qWarning() << "[PollScheduler] Цепочка" << chainId
                       << "пустая (нет валидных драйверов), пропускаем";
            delete chain;
            continue;
        }

        // ✅ Создаём QThread для цепочки
        QThread *thread = new QThread(this);

        // ✅ Перемещаем цепочку в рабочий поток
        chain->moveToThread(thread);

        // ✅ КРИТИЧЕСКИ ВАЖНО: перемещаем ВСЕХ драйверов в тот же поток.
        //    moveToThread(chain) НЕ перемещает детей автоматически.
        //    А так как драйверы созданы без parent — moveToThread
        //    сработает корректно.
        chain->moveDriversToThread(thread);

        // ✅ Подключаем общие сигналы
        connect(chain, &PollChain::pollResult, this, &PollScheduler::onPollResult);
        connect(chain, &PollChain::commandExecuted, this, &PollScheduler::onCommandExecuted);
        connect(chain, &PollChain::errorOccurred, this, &PollScheduler::onErrorOccurred);

        // ✅ НОВЫЙ СИГНАЛ: Потеря связи
        connect(chain, &PollChain::connectionLost, this, &PollScheduler::onConnectionLost);

        // ✅ НОВЫЙ СИГНАЛ: Готовность вернуться в Fast Pool
        connect(chain, &PollChain::readyToReturnToFast, this, &PollScheduler::onReadyToReturnToFast);

        // ✅ Подключаем сигнал завершения цикла в зависимости от пула
        // (По умолчанию все цепочки в Fast Pool, но это может измениться)
        connect(chain, &PollChain::cycleFinished, this, &PollScheduler::onFastCycleFinished);

        // ✅ ОПРЕДЕЛЯЕМ ПУЛ ЦЕПОЧКИ на основе поля slowPool
        // Если хотя бы одно устройство в цепочке помечено как slowPool — вся цепочка в Slow Pool
        bool shouldBeSlow = false;
        for (const ConnWithIndex &pair : chainDevices) {
            if (pair.first.slowPool) { shouldBeSlow = true; break; }
        }
        if (shouldBeSlow) {
            // ✅ Цепочка в Slow Pool
            chain->setPoolType(PoolType::Slow);
            chain->setSlowReason(SlowReason::Manual);

            // ✅ Переподключаем сигнал cycleFinished к onSlowCycleFinished
            disconnect(chain, &PollChain::cycleFinished, this, &PollScheduler::onFastCycleFinished);
            connect(chain, &PollChain::cycleFinished, this, &PollScheduler::onSlowCycleFinished);

            m_slowChains.push_back(chain);
            qDebug() << "[PollScheduler] Запущена цепочка" << chainId
                     << "с" << chain->deviceCount() << "устройствами (Pool: Slow, причина: вручную)";
        } else {
            // ✅ Цепочка в Fast Pool (по умолчанию)
            m_fastChains.push_back(chain);
            qDebug() << "[PollScheduler] Запущена цепочка" << chainId
                     << "с" << chain->deviceCount() << "устройствами (Pool: Fast)";
        }

        m_threads.push_back(thread);

        // Запускаем поток
        thread->start();    }

    connectAllDevices();
}

void PollScheduler::connectAllDevices()
{
    for (PollChain *chain : m_fastChains) {
        QMetaObject::invokeMethod(chain, "connectAllDevices", Qt::QueuedConnection);
    }
    for (PollChain *chain : m_slowChains) {
        QMetaObject::invokeMethod(chain, "connectAllDevices", Qt::QueuedConnection);
    }
    qDebug() << "[PollScheduler] Запрошено подключение всех устройств";
}

void PollScheduler::start()
{
    if (!m_pollTimer->isActive()) {
        m_pollTimer->start();

        // ✅ Немедленно запускаем первый цикл опроса
        QTimer::singleShot(0, this, &PollScheduler::startPollCycle);

        qDebug() << "[PollScheduler] Таймер опроса запущен (интервал:"
                 << m_pollTimer->interval() << "мс)"
                 << "| Fast режим:" << (m_fastMode ? "ВКЛ" : "ВЫКЛ")
                 << "| Первый цикл будет выполнен немедленно";
    }
}

void PollScheduler::stop()
{
    qDebug() << "[PollScheduler] Остановка планировщика...";

    // 1. Останавливаем таймер
    m_pollTimer->stop();

    // 2. Останавливаем все цепочки
    for (PollChain *chain : m_fastChains) {
        if (chain) chain->stop();
    }
    for (PollChain *chain : m_slowChains) {
        if (chain) chain->stop();
    }

    // 3. Останавливаем все потоки
    for (QThread *thread : m_threads) {
        if (thread && thread->isRunning()) {
            thread->quit();
            thread->wait(3000);  // Ждём завершения потока
        }
    }

    // 4. Удаляем цепочки вручную
    for (PollChain *chain : m_fastChains) {
        delete chain;
    }
    for (PollChain *chain : m_slowChains) {
        delete chain;
    }

    // 5. Удаляем потоки вручную
    for (QThread *thread : m_threads) {
        delete thread;
    }

    // 6. Очищаем списки
    m_fastChains.clear();
    m_slowChains.clear();
    m_threads.clear();

    // ✅ НОВОЕ: обнуляем указатели на Time Pool
    m_timePoolChain = nullptr;
    m_timePoolThread = nullptr;

    m_fastActiveChains = 0;
    m_slowActiveChains = 0;
    m_fastCycleInProgress = false;   // ✅ ДОБАВИТЬ
    m_slowCycleInProgress = false;   // ✅ ДОБАВИТЬ

    qDebug() << "[PollScheduler] Остановлен корректно";
}

void PollScheduler::setPollingInterval(int msec)
{
    m_pollTimer->setInterval(msec);
    qDebug() << "[PollScheduler] Установлен интервал опроса:" << msec << "мс";
}

void PollScheduler::setFastMode(bool enabled)
{
    m_fastMode = enabled;

    if (m_fastMode) {
        // ✅ Таймер НЕ трогаем — он нужен для Slow Pool.
        //    Fast Pool теперь перезапускается через onFastCycleFinished.
        QTimer::singleShot(0, this, &PollScheduler::startPollCycle);
        qDebug() << "[PollScheduler] Режим Fast включён "
                    "(Fast Pool — непрерывно, Slow Pool — по таймеру)";
    } else {
        // ✅ Таймер продолжает работать как обычно
        qDebug() << "[PollScheduler] Режим Fast выключен "
                    "(оба пула — по таймеру, интервал:"
                 << m_pollTimer->interval() << "мс)";
    }
}

void PollScheduler::sendCommand(const QString &deviceName, const ControlCommand &cmd)
{
    if (cmd.slotIndex < 0) {
        qWarning() << "[PollScheduler] Команда без slotIndex, адресация невозможна:"
                   << deviceName;
        return;
    }

    for (PollChain *chain : m_fastChains) {
        if (chain->hasSlot(cmd.slotIndex)) {
            QMetaObject::invokeMethod(chain, "enqueueCommand", Qt::QueuedConnection,
                                      Q_ARG(ControlCommand, cmd));
            qDebug() << "[PollScheduler] Команда отправлена в Fast цепочку"
                     << chain->getId() << "слот:" << cmd.slotIndex;
            return;
        }
    }
    for (PollChain *chain : m_slowChains) {
        if (chain->hasSlot(cmd.slotIndex)) {
            QMetaObject::invokeMethod(chain, "enqueueCommand", Qt::QueuedConnection,
                                      Q_ARG(ControlCommand, cmd));
            qDebug() << "[PollScheduler] Команда отправлена в Slow цепочку"
                     << chain->getId() << "слот:" << cmd.slotIndex;
            return;
        }
    }

    qWarning() << "[PollScheduler] Не найдена цепочка со слотом" << cmd.slotIndex
               << "(устройство:" << deviceName << ")";
}

void PollScheduler::startPollCycle()
{
    if (!m_fastMode && !m_fastCycleInProgress)
    {
        if (!m_fastChains.empty()) {
            emit cycleStarted();
            m_fastCycleInProgress = true;
            m_fastActiveChains = static_cast<int>(m_fastChains.size());
            for (PollChain *chain : m_fastChains) {
                QMetaObject::invokeMethod(chain, "runPollCycle", Qt::QueuedConnection);
            }
        } else {
            // Сюда мы больше не попадём, но пусть будет:
            emit cycleStarted();
            emit allCyclesFinished();
        }
    }

    if (!m_slowChains.empty() && !m_slowCycleInProgress) {
        m_slowCycleInProgress = true;
        m_slowActiveChains = static_cast<int>(m_slowChains.size());
        for (PollChain *chain : m_slowChains) {
            QMetaObject::invokeMethod(chain, "runPollCycle", Qt::QueuedConnection);
        }
    }
}

void PollScheduler::onPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data)
{
    emit pollResult(chainId, deviceIndex, data);
}

void PollScheduler::onCommandExecuted(int chainId, int deviceIndex, bool success, const QString &message)
{
    emit commandExecuted(chainId, deviceIndex, success, message);
}

void PollScheduler::onFastCycleFinished(int chainId)
{
    Q_UNUSED(chainId);

    m_fastActiveChains--;
    if (m_fastActiveChains < 0) {
        m_fastActiveChains = 0;
    }

    if (m_fastActiveChains <= 0) {
        m_fastActiveChains = 0;
        m_fastCycleInProgress = false;

        // ✅ Fast Pool завершён — точка записи в БД и рассылки клиентам
        emit allCyclesFinished();

        // Fast-режим — перезапуск вне таймера
        if (m_fastMode && !m_fastChains.empty()) {
            QTimer::singleShot(100, this, &PollScheduler::startPollCycle);
        }
        // Иначе — следующий запуск по тику таймера
    }
}

void PollScheduler::onSlowCycleFinished(int chainId)
{
    Q_UNUSED(chainId);

    m_slowActiveChains--;
    if (m_slowActiveChains < 0) {
        m_slowActiveChains = 0;
    }

    if (m_slowActiveChains <= 0) {
        m_slowActiveChains = 0;
        m_slowCycleInProgress = false;

        // Slow Pool ничего не эмитит — ни записи, ни рассылки.
    }
}

void PollScheduler::onErrorOccurred(int chainId, int deviceIndex, const QString &error)
{
    emit errorOccurred(chainId, deviceIndex, error);
}

void PollScheduler::onConnectionLost(int chainId)
{
    // ✅ Защита: Time Pool никогда не мигрирует
    if (m_timePoolChain && m_timePoolChain->getId() == chainId) {
        qDebug() << "[PollScheduler] Time Pool проигнорирован в onConnectionLost";
        return;
    }

    PoolType currentPool;
    PollChain *chain = findChainById(chainId, currentPool);

    if (!chain) {
        qWarning() << "[PollScheduler] Не найдена цепочка" << chainId << "для миграции";
        return;
    }

    // ✅ Если цепочка уже в Slow Pool, просто сбрасываем счётчик успешных опросов
    if (currentPool == PoolType::Slow) {
        chain->resetSuccessCount();
        qDebug() << "[PollScheduler] Цепочка" << chainId << "уже в Slow Pool, сброс счётчика";
        return;
    }

    // ✅ Переводим цепочку в Slow Pool
    migrateChainToSlow(chain, SlowReason::ConnectionLost);
}

void PollScheduler::migrateChainToSlow(PollChain *chain, SlowReason reason)
{
    if (!chain) return;

    // ✅ Находим цепочку в Fast Pool
    auto it = std::find(m_fastChains.begin(), m_fastChains.end(), chain);
    if (it == m_fastChains.end()) {
        qWarning() << "[PollScheduler] Цепочка не найдена в Fast Pool для миграции";
        return;
    }

    // ✅ Удаляем из Fast Pool
    m_fastChains.erase(it);

    // ✅ Отключаем старый сигнал cycleFinished
    disconnect(chain, &PollChain::cycleFinished, this, &PollScheduler::onFastCycleFinished);

    // ✅ Подключаем новый сигнал cycleFinished
    connect(chain, &PollChain::cycleFinished, this, &PollScheduler::onSlowCycleFinished);

    // ✅ Обновляем атрибуты цепочки
    chain->setPoolType(PoolType::Slow);
    chain->setSlowReason(reason);
    chain->resetSuccessCount();

    // ✅ Добавляем в Slow Pool
    m_slowChains.push_back(chain);

    qDebug() << "[PollScheduler] Цепочка" << chain->getId()
             << "перемещена из Fast в Slow Pool (причина:"
             << (reason == SlowReason::Manual ? "вручную" : "потеря связи") << ")";
}

void PollScheduler::migrateChainToFast(PollChain *chain)
{
    if (!chain) return;

    // ✅ Находим цепочку в Slow Pool
    auto it = std::find(m_slowChains.begin(), m_slowChains.end(), chain);
    if (it == m_slowChains.end()) {
        qWarning() << "[PollScheduler] Цепочка не найдена в Slow Pool для миграции";
        return;
    }

    // ✅ Удаляем из Slow Pool
    m_slowChains.erase(it);

    // ✅ Отключаем старый сигнал cycleFinished
    disconnect(chain, &PollChain::cycleFinished, this, &PollScheduler::onSlowCycleFinished);

    // ✅ Подключаем новый сигнал cycleFinished
    connect(chain, &PollChain::cycleFinished, this, &PollScheduler::onFastCycleFinished);

    // ✅ Обновляем атрибуты цепочки
    chain->setPoolType(PoolType::Fast);

    // ✅ НОВОЕ: Включаем режим восстановления после возврата в Fast Pool
    // Это предотвращает немедленную повторную миграцию при первой же ошибке
    chain->setRecoveryMode(true);
    chain->resetFailedAttempts();

    // ✅ Добавляем в Fast Pool
    m_fastChains.push_back(chain);

    qDebug() << "[PollScheduler] Цепочка перемещена из Slow в Fast Pool (режим восстановления активирован)";
}

PollChain* PollScheduler::findChainById(int chainId, PoolType &outPool) const
{
    // Ищем в Fast Pool
    for (PollChain *chain : m_fastChains) {
        if (chain->getId() == chainId) {
            outPool = PoolType::Fast;
            return chain;
        }
    }

    // Ищем в Slow Pool
    for (PollChain *chain : m_slowChains) {
        if (chain->getId() == chainId) {
            outPool = PoolType::Slow;
            return chain;
        }
    }

    return nullptr;
}

void PollScheduler::onReadyToReturnToFast(int chainId)
{
    PoolType currentPool;
    PollChain *chain = findChainById(chainId, currentPool);

    if (!chain) {
        qWarning() << "[PollScheduler] Не найдена цепочка" << chainId << "для возврата в Fast Pool";
        return;
    }

    // ✅ Проверяем, что цепочка действительно в Slow Pool
    if (currentPool != PoolType::Slow) {
        qDebug() << "[PollScheduler] Цепочка" << chainId << "уже в Fast Pool, возврат не нужен";
        return;
    }

    // ✅ Проверяем причину перевода в Slow Pool
    if (chain->getSlowReason() == SlowReason::Manual) {
        qDebug() << "[PollScheduler] Цепочка" << chainId << "помечена вручную как медленная, автоматический возврат запрещён";
        return;
    }

    // ✅ Возвращаем цепочку в Fast Pool
    migrateChainToFast(chain);
}
