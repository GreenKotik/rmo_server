#include "peredatchikwidget.h"
#include "ui_peredatchikform.h"
#include "databasemanager.h"  // ✅ Для записи в БД
#include <QFile>
#include <QJsonDocument>
#include <QFileInfo>
#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>
#include <cmath>

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
    // ✅ Загружаем конфигурацию датчиков после чтения файла
}

void PeredatchikWidget::saveToFile()
{
    saveToFilePreserveSections(filePath());
}

void PeredatchikWidget::processDocClientData(const QJsonObject &data)
{
    // ✅ Сетевой пакет содержит action + data
    // (см. broadcastDocUpdate в updateUIFromBuffer).
    if (data.value("action").toString() != "TransmitterDataUpdate") {
        return;
    }

    const QJsonObject payload = data.value("data").toObject();
    if (payload.isEmpty()) return;

    // ✅ Клиент пишет в свою локальную БД только если сервер
    // выставил флаг write_to_db (то есть настал момент записи).
    const bool writeToDb = payload.value("write_to_db").toBool(false);

    if (writeToDb && !m_filePath.isEmpty()) {
        const bool ok = DatabaseManager::instance().logDocumentData(
            m_filePath, 2, payload);
        if (!ok) {
            qWarning() << "[PeredatchikWidget/CLIENT] ❌ Не удалось записать в БД";
        } else {
            qDebug() << "[PeredatchikWidget/CLIENT] ✅ Данные записаны в БД";
        }
    }
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
// ✅ ЭТАП 1: Перехват данных от драйвера (температурный датчик)
// ============================================================================
void PeredatchikWidget::processPollResult(int chainId, int deviceIndex,
                                          std::shared_ptr<IDeviceData> data)
{
    BaseDocWidget::processPollResult(chainId, deviceIndex, data);
    if (!data) return;

    const int devId = data->deviceId();
    int slotIndex = -1;
    const auto connections = getEquipmentConnections();
    for (int i = 0; i < connections.size(); ++i) {
        if (connections[i].deviceId == devId) {
            slotIndex = i;
            break;
        }
    }
    if (slotIndex < 0) return;

    const DeviceErrors devErrors = data->getErrors();

    if (auto tempData = std::dynamic_pointer_cast<TemperaturaData>(data)) {
        if (slotIndex == 14) {
            m_dataBuffer.update([tempData, devErrors](TransmitterData &tData) {
                for (int i = 0; i < 4; ++i) {
                    tData.temperatures[i] = tempData->temperatures[i];
                }
                tData.errorsTemperatura = devErrors;
                // ✅ НЕ трогаем tData.timestamp — его обновит updateUIFromBuffer()
            });
        }
    }
}


// ============================================================================
// ✅ ЭТАП 3: Цепочка обработки (UI + БД + Клиенты)
// ============================================================================
void PeredatchikWidget::updateUIFromBuffer()
{
    // ✅ Вызывается, когда ВСЕ цепочки Fast Pool завершили цикл
    // (см. connect в BaseDocWidget::loadFromFilePreserveSections).

    TransmitterData currentData = m_dataBuffer.getLatest();

    // ✅ Время из Time Pool-цепочки (фиксируется в рабочем потоке).
    currentData.timestamp = lastCycleTime();

    // --- А. Обновление UI ---
    // ✅ Обрабатываем NaN: если значение не получено (датчик отключен или ошибка),
    //     показываем "--.-"
    auto formatTemp = [](double t) -> QString {
        if (std::isnan(t)) return QString("--.-");
        return QString::number(t, 'f', 1);
    };

    // ✅ Собираем массив из 4 меток для удобства
    QLabel *tempLabels[4] = {
        ui->labelTemp1,
        ui->labelTemp2,
        ui->labelTemp3,
        ui->labelTemp4
    };

    for (int i = 0; i < 4; ++i) {
        QLabel *label = tempLabels[i];
        if (!label) continue;

        // Если конфиг ещё не загружен — показываем по старому (fallback)
        if (i >= m_tempConfigs.size()) {
            label->setVisible(true);
            label->setText(QString("Температура %1: %2 °C")
                               .arg(i + 1)
                               .arg(formatTemp(currentData.temperatures[i])));
            continue;
        }

        const PeredatchikTempConfig &cfg = m_tempConfigs[i];

        if (!cfg.active) {
            // ✅ Скрываем неактивную строку
            label->setVisible(false);
            continue;
        }

        // ✅ Показываем активную строку с именем из настроек
        label->setVisible(true);
        label->setText(QString("%1: %2 °C")
                           .arg(cfg.name)
                           .arg(formatTemp(currentData.temperatures[i])));
    }

    // --- Б. Агрегация ошибок по всем устройствам ---
    currentData.ErrorsAll = buildErrorsAllString();

    // --- В. Определяем, пора ли писать в БД ---
    //     Счётчик m_msSinceLastDbWrite живёт в BaseDocWidget и
    //     накапливается каждый цикл. Когда >= db_write_interval —
    //     выставляем флаг, по которому записывают все (сервер и клиенты).
    currentData.writeToDb = shouldWriteToDb(currentData.ErrorsAll);

    // --- Г. Запись в БД (только по флагу) ---
    if (currentData.writeToDb) {
        if (!m_filePath.isEmpty()) {
            const bool ok = DatabaseManager::instance().logDocumentData(
                m_filePath, 2, currentData.toJsonObject());
            if (!ok) {
                qWarning() << "[PeredatchikWidget] ❌ Не удалось записать данные в БД";
            }
        } else {
            qWarning() << "[PeredatchikWidget] ⚠️ m_filePath пуст, запись в БД пропущена";
        }
    }

    // --- Д. Рассылка клиентам (каждый цикл, флаг уходит в JSON) ---
    QJsonObject networkPayload;
    networkPayload["action"] = "TransmitterDataUpdate";
    networkPayload["data"] = currentData.toJsonObject();
    broadcastDocUpdate(networkPayload);

    // ✅ Е. ФИНАЛИЗАЦИЯ ЦИКЛА
    finalizePollCycle();
}

// ============================================================================
// ✅ Загрузка конфигурации датчиков температуры из m_fullDocument
// ============================================================================
void PeredatchikWidget::loadTemperatureConfigs()
{
    m_tempConfigs.clear();
    m_tempConfigs.resize(4);   // Всегда 4 элемента

    QJsonArray sensorsArray = m_fullDocument.value("temperature_sensors").toArray();

    for (int i = 0; i < 4; ++i) {
        PeredatchikTempConfig cfg;

        if (i < sensorsArray.size()) {
            QJsonObject sensorObj = sensorsArray[i].toObject();
            cfg.name = sensorObj.value("name").toString();
            cfg.type = sensorObj.value("type").toString("Нет");
        } else {
            cfg.type = "Нет";
        }

        cfg.active = (cfg.type != "Нет");

        // Если имя пустое — используем «Температура N»
        if (cfg.name.isEmpty()) {
            cfg.name = QString("Температура %1").arg(i + 1);
        }

        m_tempConfigs[i] = cfg;
    }

    qDebug() << "[PeredatchikWidget] loadTemperatureConfigs:"
             << "активных датчиков:" << std::count_if(
                    m_tempConfigs.begin(), m_tempConfigs.end(),
                    [](const PeredatchikTempConfig &c) { return c.active; });
}

// ============================================================================
// ✅ Хук после перезагрузки настроек — обновить кэш датчиков
// ============================================================================
void PeredatchikWidget::onSettingsReloaded()
{
    qDebug() << "[PeredatchikWidget] onSettingsReloaded — перечитываем конфиг датчиков";
    loadTemperatureConfigs();
    // UI обновится в следующем цикле updateUIFromBuffer
}

// ============================================================================
// ✅ Хук после загрузки файла — загрузить кэш датчиков
// ============================================================================
void PeredatchikWidget::onFileLoaded()
{
    qDebug() << "[PeredatchikWidget] onFileLoaded — загружаем конфиг датчиков";
    loadTemperatureConfigs();
}
