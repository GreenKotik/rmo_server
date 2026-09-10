#include "peredatchikwidget.h"
#include "ui_peredatchikform.h"
#include "databasemanager.h"  // ✅ Для записи в БД
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>

PeredatchikWidget::PeredatchikWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::PeredatchikForm)
{
    ui->setupUi(this);

    // Инициализация UI значениями по умолчанию
    ui->labelTemp1->setText("Температура 1: --.- °C");
    ui->labelTemp2->setText("Температура 2: --.- °C");
    ui->labelTemp3->setText("Температура 3: --.- °C");
    ui->labelTemp4->setText("Температура 4: --.- °C");
}

PeredatchikWidget::~PeredatchikWidget()
{
    delete ui;
}

QSize PeredatchikWidget::documentFixedSize() const
{
    return QSize(475, 174);
}

void PeredatchikWidget::loadFromFile(const QString &path)
{
    loadFromFilePreserveSections(path);
    // m_filePath устанавливается в базовом классе, он критически важен для записи в БД
}

void PeredatchikWidget::saveToFile()
{
    saveToFilePreserveSections(filePath());
}

void PeredatchikWidget::processDocClientData(const QJsonObject &data)
{
    Q_UNUSED(data);
}

QJsonObject PeredatchikWidget::getDocumentParameters() const
{
    return QJsonObject();
}

void PeredatchikWidget::setDocumentParameters(const QJsonObject &params)
{
    Q_UNUSED(params);
}

void PeredatchikWidget::onFieldChanged()
{
    setModified(true);
    QJsonObject state;
    broadcastDocUpdate(state);
}

void PeredatchikWidget::onApplyClicked()
{
    // Логика применения настроек
}

// ============================================================================
// ✅ ЭТАП 1: Перехват данных от драйвера (BOREAS-3)
// ============================================================================
void PeredatchikWidget::processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data)
{
    // 1. Сначала вызываем базовый метод (он обновляет статусы связи, аварии и т.д.)
    BaseDocWidget::processPollResult(chainId, deviceIndex, data);

    // 2. Проверяем, являются ли пришедшие данные данными от BOREAS-3
    auto boreasData = std::dynamic_pointer_cast<Boreas3Data>(data);
    if (boreasData) {
        // Находим индекс слота в конфигурации по deviceId
        int slotIndex = -1;
        auto connections = getEquipmentConnections();
        for (int i = 0; i < connections.size(); ++i) {
            if (connections[i].deviceId == boreasData->deviceId()) {
                slotIndex = i;
                break;
            }
        }

        // 3. Если это 12-й слот (индекс 11), обновляем буфер и запускаем цепочку
        if (slotIndex == 11) {
            // ✅ Безопасно обновляем буфер записи (потокобезопасно)
            m_dataBuffer.update([boreasData](TransmitterData &tData) {
                tData.temperatures[0] = boreasData->temperatures[0];
                tData.temperatures[1] = boreasData->temperatures[1];
                tData.temperatures[2] = boreasData->temperatures[2];
                tData.temperatures[3] = boreasData->temperatures[3];
            });

            // ✅ ЭТАП 2: Синхронный запуск цепочки обработки
            // Qt::QueuedConnection гарантирует, что processDataChain() выполнится
            // в главном потоке сразу после возврата из текущего processPollResult
            QMetaObject::invokeMethod(this, &PeredatchikWidget::processDataChain, Qt::QueuedConnection);
        }
    }
}

// ============================================================================
// ✅ ЭТАП 3: Цепочка обработки (UI + БД + Клиенты)
// ============================================================================
void PeredatchikWidget::processDataChain()
{
    // ✅ Читаем последние данные из буфера (getLatest не очищает буфер, предотвращая мерцание)
    TransmitterData currentData = m_dataBuffer.swapAndGet();

    // --- А. Обновление Виджета (UI) ---
    ui->labelTemp1->setText(QString("Температура 1: %1 °C").arg(currentData.temperatures[0], 0, 'f', 1));
    ui->labelTemp2->setText(QString("Температура 2: %1 °C").arg(currentData.temperatures[1], 0, 'f', 1));
    ui->labelTemp3->setText(QString("Температура 3: %1 °C").arg(currentData.temperatures[2], 0, 'f', 1));
    ui->labelTemp4->setText(QString("Температура 4: %1 °C").arg(currentData.temperatures[3], 0, 'f', 1));

    // --- Б. Запись в Базу Данных ---
    if (!m_filePath.isEmpty()) {
        // ✅ ПЕРЕДАЁМ ПОЛНЫЙ ПУТЬ! DatabaseManager сам извлечёт и очистит имя.
        bool success = DatabaseManager::instance().logDocumentData(m_filePath, 2, currentData.toJsonObject());
        if (!success) {
            qWarning() << "[PeredatchikWidget] ❌ Не удалось записать данные в БД";
        } else {
            qDebug() << "[PeredatchikWidget] ✅ Данные успешно записаны в БД";
        }
    } else {
        qWarning() << "[PeredatchikWidget] ⚠️ m_filePath пуст, запись в БД пропущена!";
    }

    // --- В. Отправка Клиентам ---
    QJsonObject networkPayload;
    networkPayload["action"] = "TransmitterDataUpdate";
    networkPayload["data"] = currentData.toJsonObject();

    // Рассылаем всем подключенным клиентам через базовый класс
    broadcastDocUpdate(networkPayload);
}
