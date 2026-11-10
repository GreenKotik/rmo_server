#include "modulatorwidget.h"
#include "ui_modulatorform.h"
#include "databasemanager.h"  // ✅ Для записи в БД
#include "modulatorcontroldialog.h"
#include "equipmentsettingsdialog.h"  // ✅ ДОБАВИТЬ
#include "mainwindow.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QContextMenuEvent>
#include <QDebug>


ModulatorWidget::ModulatorWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::ModulatorForm)
{
    ui->setupUi(this);
    setWindowTitle("Модулятор");

    // ✅ Инициализация индикаторов в состояние "Нет данных" (Gray)
    ui->ModSwitch->setState(RingIndicator::State::Gray);

    ui->LedInd_ASI_1->setState(LedIndicator::State::Gray);
    ui->LedInd_ASI_2->setState(LedIndicator::State::Gray);

    ui->LedInd_RF_Output_1->setState(LedIndicator::State::Gray);
    ui->LedInd_RF_Output_2->setState(LedIndicator::State::Gray);

    ui->LedInd_Remote->setState(LedIndicator::State::Gray);
    ui->LedInd_Local->setState(LedIndicator::State::Gray);
    ui->LedInd_Auto->setState(LedIndicator::State::Gray);

    ui->LedInd_Power_1->setState(LedIndicator::State::Gray);
    ui->LedInd_Power_2->setState(LedIndicator::State::Gray);

    ui->LedInd_Fault->setState(LedIndicator::State::Gray);

    // ✅ Инициализация текстовых полей
    ui->label->setText("--.-");
    ui->label_2->setText("--.-");
    ui->label_5->setText("--.-");
    ui->label_7->setText("--.-");
    ui->label_11->setText("--.-");
    ui->label_10->setText("--.-");

    ui->labelASI_3->setText("ASI 1 TS Sync Lost");
    ui->labelASI_3->setStyleSheet("color: gray;");
    ui->labelASI_4->setText("ASI 2 TS Sync Lost");
    ui->labelASI_4->setStyleSheet("color: gray;");

    ui->label_20->setText("Modulator ?");
    ui->label_20->setStyleSheet("color: gray;");
}

ModulatorWidget::~ModulatorWidget()
{
    delete ui;
}

QSize ModulatorWidget::documentFixedSize() const
{
    return QSize(620, 230); // 665, 273 ✅ Актуальный размер из вашей формы
}

void ModulatorWidget::loadFromFile(const QString &filePath)
{
    loadFromFilePreserveSections(filePath);
}

void ModulatorWidget::saveToFile()
{
    if (m_filePath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Путь к файлу не задан. Используйте 'Сохранить как...'");
        return;
    }

    if (saveToFilePreserveSections(m_filePath)) {
        setModified(false);
        setWindowTitle(QFileInfo(m_filePath).completeBaseName() + ".mod");
    } else {
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл: " + m_filePath);
    }
}

void ModulatorWidget::processDocClientData(const QJsonObject &data)
{
    if (data.value("action").toString() == "KanalModulatorDataUpdate") {
        const QJsonObject payload = data.value("data").toObject();
        const bool writeToDb = payload.value("write_to_db").toBool(false);

        if (writeToDb && !m_filePath.isEmpty()) {
            const bool ok = DatabaseManager::instance().logDocumentData(
                m_filePath, 4, payload);
            if (!ok) {
                qWarning() << "[ModulatorWidget/CLIENT] ❌ Не удалось записать в БД";
            }
        }
    }
}

QJsonObject ModulatorWidget::getDocumentParameters() const
{
    return QJsonObject();
}

void ModulatorWidget::setDocumentParameters(const QJsonObject &params)
{
    Q_UNUSED(params);
}

void ModulatorWidget::onFieldChanged()
{
    setModified(true);
    QJsonObject state;
    broadcastDocUpdate(state);
}

void ModulatorWidget::onApplyClicked()
{
    QMessageBox::information(this, "Действие", "Параметры модулятора успешно применены.");
}

// ============================================================================
// Перехват данных от драйвера (модулятор)
// ============================================================================
void ModulatorWidget::processPollResult(int chainId, int deviceIndex,
                                        std::shared_ptr<IDeviceData> data)
{
    // 1. Сначала вызываем базовый метод
    BaseDocWidget::processPollResult(chainId, deviceIndex, data);
    if (!data) return;

    // 2. Находим слот по deviceId
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

    // 3. Данные от модулятора (ModulatorData)
    // В документе "Модулятор" ожидаются слоты:
    //   0 - Усилитель-распределитель (заглушка)
    //   1 - Модулятор 1 (ModulatorData)
    //   2 - Модулятор 2 (ModulatorData)
    //   3 - Переключатель резервирования (DevSwData — пока заглушка)
    //   4 - Анализатор потока (заглушка)
    if (auto modData = std::dynamic_pointer_cast<ModulatorData>(data)) {
        m_dataBuffer.update([modData, devErrors, slotIndex](KanalModulatorData &kData) {
            if (slotIndex == 1) {
                // Модулятор 1
                kData.mod1_serianumber = modData->serianumber;
                kData.mod1_productType = modData->productType;
                kData.mod1_productName = modData->productName;
                kData.mod1_temperature = modData->temperature;

                kData.mod1_alarmInputAsi1TsSynchroStatus = modData->alarmInputAsi1TsSynchroStatus;
                kData.mod1_alarmInputTSoIp1TsSynchroStatus = modData->alarmInputTSoIp1TsSynchroStatus;
                kData.mod1_alarmInputActiveInFifoOverflowStatus = modData->alarmInputActiveInFifoOverflowStatus;

                kData.mod1_configProcessBaudrate = modData->configProcessBaudrate;

                kData.mod1_monitorInputAsi1TsSynchro = modData->monitorInputAsi1TsSynchro;
                kData.mod1_monitorInputAsi1UserfulBitrate = modData->monitorInputAsi1UserfulBitrate;
                kData.mod1_monitorInputAsi1TotalBitrate = modData->monitorInputAsi1TotalBitrate;
                kData.mod1_monitorInputTSoIp1TsSynchro = modData->monitorInputTSoIp1TsSynchro;
                kData.mod1_monitorInputTSoIp1UserfulBitrate = modData->monitorInputTSoIp1UserfulBitrate;
                kData.mod1_monitorInputTSoIp1TotalBitrate = modData->monitorInputTSoIp1TotalBitrate;

                kData.mod1_monitorInputActiveAsi1TsSynchro = modData->monitorInputActiveAsi1TsSynchro;
                kData.mod1_monitorInputActiveAsi1UserfulBitrate = modData->monitorInputActiveAsi1UserfulBitrate;
                kData.mod1_monitorInputActiveAsi1TotalBitrate = modData->monitorInputActiveAsi1TotalBitrate;
                kData.mod1_monitorInputActiveTSoIp1UserfulBitrate = modData->monitorInputActiveTSoIp1UserfulBitrate;
                kData.mod1_monitorInputActiveTSoIp1TotalBitrate = modData->monitorInputActiveTSoIp1TotalBitrate;
                kData.mod1_monitorInputActiveTSoIp1TsSynchro = modData->monitorInputActiveTSoIp1TsSynchro;

                kData.mod1_monitorProcessOutputSignalBandwidth = modData->monitorProcessOutputSignalBandwidth;
                kData.mod1_monitorProcessOutputChannelBitrate = modData->monitorProcessOutputChannelBitrate;

                kData.mod1_outputRfIfBandPowerLevel = modData->outputRfIfBandPowerLevel;
                kData.mod1_outputRfLbBandPowerLevel = modData->outputRfLbBandPowerLevel;
                kData.mod1_outputConfigRfLbBandCarrierFrequency = modData->outputConfigRfLbBandCarrierFrequency;
                kData.mod1_outputConfigRfIfBandCarrierFrequency = modData->outputConfigRfIfBandCarrierFrequency;
                kData.mod1_outputConfigRfMute = modData->outputConfigRfMute;
                kData.mod1_outputConfigRfUnmute = modData->outputConfigRfUnmute;
                kData.mod1_outputConfigRfMuteAtStartup = modData->outputConfigRfMuteAtStartup;
                kData.mod1_outputConfigRfCarrierModulated = modData->outputConfigRfCarrierModulated;
                kData.mod1_outputConfigRfCustomRolloffValue = modData->outputConfigRfCustomRolloffValue;

                kData.mod1_monitorIp1EthernetLock = modData->monitorIp1EthernetLock;
                kData.mod1_monitorIp1MpegTsBitrate = modData->monitorIp1MpegTsBitrate;

                kData.mod1_monitorRelayWarnInState = modData->monitorRelayWarnInState;
                kData.mod1_monitorRelayCritState = modData->monitorRelayCritState;

                kData.mod1_monitorRedundancyStatusGpioRelay2 = modData->monitorRedundancyStatusGpioRelay2;
                kData.mod1_monitorRedundancyStatusGpioRelay3 = modData->monitorRedundancyStatusGpioRelay3;

                kData.mod1_monitorTimeCurrentUtc = modData->monitorTimeCurrentUtc;

                kData.mod1_monitorPowerSupply1Status = modData->monitorPowerSupply1Status;
                kData.mod1_monitorPowerSupply2Status = modData->monitorPowerSupply2Status;

                kData.mod1_monitorFan1Speed = modData->monitorFan1Speed;
                kData.mod1_monitorFan2Speed = modData->monitorFan2Speed;
                kData.mod1_monitorFan3Speed = modData->monitorFan3Speed;
                kData.mod1_monitorFan4Speed = modData->monitorFan4Speed;
                kData.mod1_monitorFan5Speed = modData->monitorFan5Speed;

                kData.errorsMod1 = devErrors;
            }
            else if (slotIndex == 2) {
                // Модулятор 2
                kData.mod2_serianumber = modData->serianumber;
                kData.mod2_productType = modData->productType;
                kData.mod2_productName = modData->productName;
                kData.mod2_temperature = modData->temperature;

                kData.mod2_alarmInputAsi1TsSynchroStatus = modData->alarmInputAsi1TsSynchroStatus;
                kData.mod2_alarmInputTSoIp1TsSynchroStatus = modData->alarmInputTSoIp1TsSynchroStatus;
                kData.mod2_alarmInputActiveInFifoOverflowStatus = modData->alarmInputActiveInFifoOverflowStatus;

                kData.mod2_configProcessBaudrate = modData->configProcessBaudrate;

                kData.mod2_monitorInputAsi1TsSynchro = modData->monitorInputAsi1TsSynchro;
                kData.mod2_monitorInputAsi1UserfulBitrate = modData->monitorInputAsi1UserfulBitrate;
                kData.mod2_monitorInputAsi1TotalBitrate = modData->monitorInputAsi1TotalBitrate;
                kData.mod2_monitorInputTSoIp1TsSynchro = modData->monitorInputTSoIp1TsSynchro;
                kData.mod2_monitorInputTSoIp1UserfulBitrate = modData->monitorInputTSoIp1UserfulBitrate;
                kData.mod2_monitorInputTSoIp1TotalBitrate = modData->monitorInputTSoIp1TotalBitrate;

                kData.mod2_monitorInputActiveAsi1TsSynchro = modData->monitorInputActiveAsi1TsSynchro;
                kData.mod2_monitorInputActiveAsi1UserfulBitrate = modData->monitorInputActiveAsi1UserfulBitrate;
                kData.mod2_monitorInputActiveAsi1TotalBitrate = modData->monitorInputActiveAsi1TotalBitrate;
                kData.mod2_monitorInputActiveTSoIp1UserfulBitrate = modData->monitorInputActiveTSoIp1UserfulBitrate;
                kData.mod2_monitorInputActiveTSoIp1TotalBitrate = modData->monitorInputActiveTSoIp1TotalBitrate;
                kData.mod2_monitorInputActiveTSoIp1TsSynchro = modData->monitorInputActiveTSoIp1TsSynchro;

                kData.mod2_monitorProcessOutputSignalBandwidth = modData->monitorProcessOutputSignalBandwidth;
                kData.mod2_monitorProcessOutputChannelBitrate = modData->monitorProcessOutputChannelBitrate;

                kData.mod2_outputRfIfBandPowerLevel = modData->outputRfIfBandPowerLevel;
                kData.mod2_outputRfLbBandPowerLevel = modData->outputRfLbBandPowerLevel;
                kData.mod2_outputConfigRfLbBandCarrierFrequency = modData->outputConfigRfLbBandCarrierFrequency;
                kData.mod2_outputConfigRfIfBandCarrierFrequency = modData->outputConfigRfIfBandCarrierFrequency;
                kData.mod2_outputConfigRfMute = modData->outputConfigRfMute;
                kData.mod2_outputConfigRfUnmute = modData->outputConfigRfUnmute;
                kData.mod2_outputConfigRfMuteAtStartup = modData->outputConfigRfMuteAtStartup;
                kData.mod2_outputConfigRfCarrierModulated = modData->outputConfigRfCarrierModulated;
                kData.mod2_outputConfigRfCustomRolloffValue = modData->outputConfigRfCustomRolloffValue;

                kData.mod2_monitorIp1EthernetLock = modData->monitorIp1EthernetLock;
                kData.mod2_monitorIp1MpegTsBitrate = modData->monitorIp1MpegTsBitrate;

                kData.mod2_monitorRelayWarnInState = modData->monitorRelayWarnInState;
                kData.mod2_monitorRelayCritState = modData->monitorRelayCritState;

                kData.mod2_monitorRedundancyStatusGpioRelay2 = modData->monitorRedundancyStatusGpioRelay2;
                kData.mod2_monitorRedundancyStatusGpioRelay3 = modData->monitorRedundancyStatusGpioRelay3;

                kData.mod2_monitorTimeCurrentUtc = modData->monitorTimeCurrentUtc;

                kData.mod2_monitorPowerSupply1Status = modData->monitorPowerSupply1Status;
                kData.mod2_monitorPowerSupply2Status = modData->monitorPowerSupply2Status;

                kData.mod2_monitorFan1Speed = modData->monitorFan1Speed;
                kData.mod2_monitorFan2Speed = modData->monitorFan2Speed;
                kData.mod2_monitorFan3Speed = modData->monitorFan3Speed;
                kData.mod2_monitorFan4Speed = modData->monitorFan4Speed;
                kData.mod2_monitorFan5Speed = modData->monitorFan5Speed;

                kData.errorsMod2 = devErrors;
            }
        });
    }
}


// ============================================================================
// ✅ ЭТАП 3: Цепочка обработки (UI + БД + Клиенты)
// ============================================================================
void ModulatorWidget::updateUIFromBuffer()
{
    KanalModulatorData currentData = m_dataBuffer.getLatest();

    // ✅ Время из Time Pool-цепочки (фиксируется в рабочем потоке).
    currentData.timestamp = lastCycleTime();

    // --- А. Обновление UI ---
    // ✅ Output Level для модулятора 1 (LB band) → label_5
    ui->label_5->setText(QString::number(currentData.mod1_outputRfLbBandPowerLevel, 'f', 2));

    // Позже перепишем updateUIFromModulatorData под новые поля.
    qDebug() << "[ModulatorWidget] updateUIFromBuffer:"
             << "mod1_productName=" << currentData.mod1_productName
             << "mod1_temperature=" << currentData.mod1_temperature
             << "mod2_productName=" << currentData.mod2_productName
             << "mod2_temperature=" << currentData.mod2_temperature;

    // --- Б. Агрегация ошибок по всем устройствам ---
    currentData.ErrorsAll = buildErrorsAllString();

    // --- В. Определяем, пора ли писать в БД ---
    currentData.writeToDb = shouldWriteToDb(currentData.ErrorsAll);

    // --- Г. Запись в БД (только по флагу) ---
    if (currentData.writeToDb) {
        if (!m_filePath.isEmpty()) {
            const bool ok = DatabaseManager::instance().logDocumentData(
                m_filePath, 4, currentData.toJsonObject());
            if (!ok) {
                qWarning() << "[ModulatorWidget] ❌ Не удалось записать данные в БД";
            }
        } else {
            qWarning() << "[ModulatorWidget] ⚠️ m_filePath пуст, запись в БД пропущена";
        }
    }

    // --- Г. Рассылка клиентам (один раз за цикл) ---
    QJsonObject networkPayload;
    networkPayload["action"] = "KanalModulatorDataUpdate";
    networkPayload["data"] = currentData.toJsonObject();
    broadcastDocUpdate(networkPayload);

    // --- Д. ФИНАЛИЗАЦИЯ ЦИКЛА ---
    finalizePollCycle();
}




// ============================================================================
// ✅ НОВОЕ: Открытие диалога расширенного управления
// ============================================================================
void ModulatorWidget::openControlDialog()
{
    // ⚠️ ВРЕМЕННАЯ ЗАГЛУШКА — пока не реализован ModulatorControlDialog
    // TODO: Когда создадим ModulatorControlDialog, раскомментировать код ниже:

    if (m_controlDialog && m_controlDialog->isVisible()) {
        m_controlDialog->raise();
        m_controlDialog->activateWindow();
        return;
    }

    m_controlDialog = new ModulatorControlDialog(this, this);
    m_controlDialog->setAttribute(Qt::WA_DeleteOnClose);
    m_controlDialog->exec();
    m_controlDialog = nullptr;


/*    QMessageBox::information(this, "Расширенное управление",
                             "Диалог расширенного управления будет реализован в следующем шаге.\n\n"
                             "Здесь будут табы:\n"
                             "• Модулятор 1\n"
                             "• Модулятор 2\n"
                             "• Переключатель резервирования\n"
                             "• Усилитель-распределитель\n"
                             "• Анализатор потока");
*/
}

// ============================================================================
// ✅ НОВОЕ: Переопределение контекстного меню
// ============================================================================
void ModulatorWidget::contextMenuEvent(QContextMenuEvent *event)
{
    // Ищем MainWindow в иерархии родителей
    MainWindow *mainWin = nullptr;
    QWidget *p = parentWidget();
    while (p) {
        mainWin = qobject_cast<MainWindow*>(p);
        if (mainWin) break;
        p = p->parentWidget();
    }
    if (!mainWin) {
        // Если MainWindow не найден — вызываем базовую реализацию
        BaseDocWidget::contextMenuEvent(event);
        return;
    }

    // Создаём меню
    QMenu menu(this);
    menu.setStyleSheet(
        "QMenu { "
        "  background-color: #FFFFFF; "
        "  border: 1px solid #B0B0B0; "
        "  padding: 4px; "
        "} "
        "QMenu::item { "
        "  padding: 6px 24px 6px 12px; "
        "  background-color: transparent; "
        "} "
        "QMenu::item:selected { "
        "  background-color: #005A9E; "
        "  color: white; "
        "}"
    );

    // ✅ НОВЫЙ пункт: Просмотр графика (тот же, что в BaseDocWidget)
    QAction *graphAct = menu.addAction("📊 Просмотр графика");

    // Пункт: Настройка параметров
    QAction *settingsAct = menu.addAction("⚙️ Настройка параметров");

    // ✅ Пункт: Расширенное управление (только для модулятора)
    QAction *controlAct = menu.addAction("🎛️ Расширенное управление...");

    // Показываем меню в позиции клика
    QAction *selected = menu.exec(event->globalPos());

    // Обработка выбора
    if (selected == graphAct) {
        // ✅ Используем protected-слот базового класса
        onViewDataGraph();
    }
    else if (selected == settingsAct) {
        EquipmentSettingsDialog dlg(mainWin);
        dlg.setDocumentFilePath(m_filePath);
        dlg.setCurrentUser(mainWin->getCurrentUser());
        dlg.exec();
    }
    else if (selected == controlAct) {
        openControlDialog();
    }
}

// ============================================================================
// ✅ НОВОЕ: Слот для открытия диалога (на случай, если понадобится из UI)
// ============================================================================
void ModulatorWidget::onOpenControlDialog()
{
    openControlDialog();
}

// ============================================================================
// ✅ Хук после загрузки файла — загрузить конфигурацию (если есть)
// ============================================================================
void ModulatorWidget::onFileLoaded()
{
    qDebug() << "[ModulatorWidget] onFileLoaded для:" << m_filePath;
    // Специфичных настроек для модулятора пока нет.
    // Метод оставлен для единообразия с другими виджетами.
}
