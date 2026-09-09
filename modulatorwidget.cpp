#include "modulatorwidget.h"
#include "ui_modulatorform.h"
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
    // 1. Сначала вызываем базовый метод (для индикатора заголовка, аварий и т.д.)
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

    if (data->asiSyncLost1) {
        ui->LedInd_ASI_1->setState(LedIndicator::State::Red);
        ui->labelASI_3->setText("ASI 1 TS Sync LOST");
        ui->labelASI_3->setStyleSheet("color: red; font-weight: bold;");
    } else {
        ui->LedInd_ASI_1->setState(LedIndicator::State::Green);
        ui->labelASI_3->setText("ASI 1 TS Sync OK");
        ui->labelASI_3->setStyleSheet("color: green;");
    }

    ui->LedInd_RF_Output_1->setState(data->rfOutputOk1 ? LedIndicator::State::Green : LedIndicator::State::Red);

    // === Монитор 2 ===
    ui->label_7->setText(QString::number(data->usefulBitRate2, 'f', 3));
    ui->label_11->setText(QString::number(data->totalBitRate2, 'f', 3));
    ui->label_10->setText(QString::number(data->outputLevel2, 'f', 1));

    if (data->asiSyncLost2) {
        ui->LedInd_ASI_2->setState(LedIndicator::State::Red);
        ui->labelASI_4->setText("ASI 2 TS Sync LOST");
        ui->labelASI_4->setStyleSheet("color: red; font-weight: bold;");
    } else {
        ui->LedInd_ASI_2->setState(LedIndicator::State::Green);
        ui->labelASI_4->setText("ASI 2 TS Sync OK");
        ui->labelASI_4->setStyleSheet("color: green;");
    }

    ui->LedInd_RF_Output_2->setState(data->rfOutputOk2 ? LedIndicator::State::Green : LedIndicator::State::Red);

    // === SW секция: Режим ===
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

    // === Активный модулятор и питание ===
    if (data->activeModulator == 1) {
        ui->label_20->setText("Modulator 1");
        ui->label_20->setStyleSheet("color: green; font-weight: bold;");
        ui->ModSwitch->setState(RingIndicator::State::Green);
    } else if (data->activeModulator == 2) {
        ui->label_20->setText("Modulator 2");
        ui->label_20->setStyleSheet("color: #FFC107; font-weight: bold;"); // Желтый/Янтарный
        ui->ModSwitch->setState(RingIndicator::State::Yellow);
    } else {
        ui->label_20->setText("Modulator ?");
        ui->label_20->setStyleSheet("color: gray;");
        ui->ModSwitch->setState(RingIndicator::State::Gray);
    }

    ui->LedInd_Power_1->setState(data->power1On ? LedIndicator::State::Green : LedIndicator::State::Gray);
    ui->LedInd_Power_2->setState(data->power2On ? LedIndicator::State::Green : LedIndicator::State::Gray);

    // === Авария (Fault) ===
    if (data->swFault || data->hasErrors()) {
        ui->LedInd_Fault->setState(LedIndicator::State::Red);
        // Если есть общая авария, кольцевой индикатор тоже становится красным
        if (data->activeModulator != 0) {
            ui->ModSwitch->setState(RingIndicator::State::Red);
        }
    } else {
        ui->LedInd_Fault->setState(LedIndicator::State::Gray);
    }
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

    // Пункт 1: Настройка параметров (из базового класса)
    QAction *settingsAct = menu.addAction("⚙️ Настройка параметров");

    menu.addSeparator();

    // ✅ НОВЫЙ пункт: Расширенное управление
    QAction *controlAct = menu.addAction("🎛️ Расширенное управление...");

    // Показываем меню в позиции клика
    QAction *selected = menu.exec(event->globalPos());

    // Обработка выбора
    if (selected == settingsAct) {
        EquipmentSettingsDialog dlg(mainWin);
        dlg.setDocumentFilePath(m_filePath);
        dlg.setCurrentUser(mainWin->getCurrentUser());
        dlg.exec();
    } else if (selected == controlAct) {
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
