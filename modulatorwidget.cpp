#include "modulatorwidget.h"
#include "ui_modulatorform.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QMessageBox>
#include <QDebug>

ModulatorWidget::ModulatorWidget(QWidget *parent)
    : BaseDocWidget(parent), ui(new Ui::ModulatorForm)
{
    ui->setupUi(this);
    setWindowTitle("Модулятор");

    // ✅ Инициализация всех индикаторов в серое состояние (нет данных)
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

    // ✅ Инициализация числовых значений
    ui->label->setText("--.-");
    ui->label_2->setText("--.-");
    ui->label_5->setText("--.-");
    ui->label_7->setText("--.-");
    ui->label_11->setText("--.-");
    ui->label_10->setText("--.-");

    // ✅ Инициализация текстовых меток
    ui->labelASI_3->setText("ASI 1 TS Sync Lost");
    ui->labelASI_3->setStyleSheet("color: gray;");
    ui->labelASI_4->setText("ASI 2 TS Sync Lost");
    ui->labelASI_4->setStyleSheet("color: gray;");

    qDebug() << "[ModulatorWidget] Виджет создан";
}

ModulatorWidget::~ModulatorWidget()
{
    delete ui;
}

QSize ModulatorWidget::documentFixedSize() const
{
    return QSize(620, 230); //(660, 273);
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
    Q_UNUSED(data);
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
// ✅ ОБРАБОТКА РЕЗУЛЬТАТОВ ОПРОСА
// ============================================================================
void ModulatorWidget::processPollResult(int chainId, int deviceIndex, std::shared_ptr<IDeviceData> data)
{
    // 1. Сначала вызываем базовый метод (для индикатора, аварий и т.д.)
    BaseDocWidget::processPollResult(chainId, deviceIndex, data);

    // 2. Проверяем, являются ли данные данными модулятора
    auto modData = std::dynamic_pointer_cast<ModulatorData>(data);
    if (modData) {
        updateUIFromModulatorData(modData);
    }
}

// ============================================================================
// ✅ ОБНОВЛЕНИЕ UI ИЗ ДАННЫХ МОДУЛЯТОРА
// ============================================================================
void ModulatorWidget::updateUIFromModulatorData(const std::shared_ptr<ModulatorData> &data)
{
    if (!data) return;

    // === Монитор 1 ===
    ui->label->setText(QString::number(data->usefulBitRate1, 'f', 3));
    ui->label_2->setText(QString::number(data->totalBitRate1, 'f', 3));
    ui->label_5->setText(QString::number(data->outputLevel1, 'f', 1));

    // ASI 1
    if (data->asiSyncLost1) {
        ui->LedInd_ASI_1->setState(LedIndicator::State::Red);
        ui->labelASI_3->setText("ASI 1 TS Sync LOST");
        ui->labelASI_3->setStyleSheet("color: red; font-weight: bold;");
    } else {
        ui->LedInd_ASI_1->setState(LedIndicator::State::Green);
        ui->labelASI_3->setText("ASI 1 TS Sync OK");
        ui->labelASI_3->setStyleSheet("color: green;");
    }

    // RF Output 1
    ui->LedInd_RF_Output_1->setState(data->rfOutputOk1 ? LedIndicator::State::Green : LedIndicator::State::Red);

    // === Монитор 2 ===
    ui->label_7->setText(QString::number(data->usefulBitRate2, 'f', 3));
    ui->label_11->setText(QString::number(data->totalBitRate2, 'f', 3));
    ui->label_10->setText(QString::number(data->outputLevel2, 'f', 1));

    // ASI 2
    if (data->asiSyncLost2) {
        ui->LedInd_ASI_2->setState(LedIndicator::State::Red);
        ui->labelASI_4->setText("ASI 2 TS Sync LOST");
        ui->labelASI_4->setStyleSheet("color: red; font-weight: bold;");
    } else {
        ui->LedInd_ASI_2->setState(LedIndicator::State::Green);
        ui->labelASI_4->setText("ASI 2 TS Sync OK");
        ui->labelASI_4->setStyleSheet("color: green;");
    }

    // RF Output 2
    ui->LedInd_RF_Output_2->setState(data->rfOutputOk2 ? LedIndicator::State::Green : LedIndicator::State::Red);

    // === SW секция ===
    // Режим Remote/Local/Auto
    ui->LedInd_Remote->setState(LedIndicator::State::Gray);
    ui->LedInd_Local->setState(LedIndicator::State::Gray);
    ui->LedInd_Auto->setState(LedIndicator::State::Gray);

    switch (data->swMode) {
        case ModulatorData::SwMode::Remote:
            ui->LedInd_Remote->setState(LedIndicator::State::Green);
            break;
        case ModulatorData::SwMode::Local:
            ui->LedInd_Local->setState(LedIndicator::State::Green);
            break;
        case ModulatorData::SwMode::Auto:
            ui->LedInd_Auto->setState(LedIndicator::State::Green);
            break;
        default:
            break;
    }

    // Активный модулятор (1 или 2)
    if (data->activeModulator == 1) {
        ui->ModSwitch->setState(RingIndicator::State::Green);
    } else if (data->activeModulator == 2) {
        ui->ModSwitch->setState(RingIndicator::State::Yellow);
    } else {
        ui->ModSwitch->setState(RingIndicator::State::Gray);
    }

    // Power 1 / Power 2
    ui->LedInd_Power_1->setState(data->power1On ? LedIndicator::State::Green : LedIndicator::State::Gray);
    ui->LedInd_Power_2->setState(data->power2On ? LedIndicator::State::Green : LedIndicator::State::Gray);

    // Fault
    ui->LedInd_Fault->setState(data->swFault ? LedIndicator::State::Red : LedIndicator::State::Gray);
}
