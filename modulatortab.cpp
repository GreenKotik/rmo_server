#include "modulatortab.h"
#include "basedocwidget.h"
#include "devicedata.h"
#include "deviceids.h"
#include "modulatordriver.h"

#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QTableWidgetItem>
#include <QShowEvent>
#include <QDebug>

// ============================================================================
// ✅ КОНСТРУКТОР
// ============================================================================
ModulatorTab::ModulatorTab(BaseDocWidget *docWidget, int slotIndex, QWidget *parent)
    : ModulatorTabBase(docWidget, parent)
    , ui(new Ui::ModulatorTabForm)
    , m_slotIndex(slotIndex)
{
    ui->setupUi(this);

    // Определяем deviceId и имя по slotIndex в m_equipmentConnections
    const auto connections = docWidget->getEquipmentConnections();
    if (slotIndex >= 0 && slotIndex < connections.size()) {
        m_deviceId   = connections[slotIndex].deviceId;
        m_deviceName = connections[slotIndex].name;
    }

    ui->labelTitle->setText(QString("Модулятор %1%2")
                                .arg(slotIndex)
                                .arg(m_deviceName.isEmpty() ? QString()
                                                            : QString(" — ") + m_deviceName));

    // Шапка таблицы
    ui->tableValues->setColumnCount(2);
    ui->tableValues->setHorizontalHeaderLabels(QStringList() << "Параметр" << "Значение");
    ui->tableValues->horizontalHeader()->setStretchLastSection(false);
    ui->tableValues->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ui->tableValues->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    ui->tableValues->verticalHeader()->setVisible(false);

    // Конфиг модели
    loadConfigForDevice();
    rebuildValuesTable();

    // Кнопки — заглушки (команды отдельной задачей)
    connect(ui->btnTurnOn,      &QPushButton::clicked, this, &ModulatorTab::onTurnOnClicked);
    connect(ui->btnTurnOff,     &QPushButton::clicked, this, &ModulatorTab::onTurnOffClicked);
    connect(ui->btnResetAlarms, &QPushButton::clicked, this, &ModulatorTab::onResetAlarmsClicked);
}

ModulatorTab::~ModulatorTab()
{
    delete ui;
}

// ============================================================================
// ✅ КОНФИГ ПО deviceId
// ============================================================================
void ModulatorTab::loadConfigForDevice()
{
    m_configLoaded = false;
    if (m_deviceId == ID_UNKNOWN) {
        qWarning() << "[ModulatorTab] deviceId не определён для slot" << m_slotIndex;
        return;
    }
    m_configLoaded = m_config.loadForDevice(m_deviceId);
    if (!m_configLoaded) {
        qWarning() << "[ModulatorTab] Конфиг не найден для deviceId:" << m_deviceId;
    }
}

// ============================================================================
// ✅ ПОСТРОЕНИЕ СТРОК ТАБЛИЦЫ (один раз при смене конфига)
// ============================================================================
void ModulatorTab::rebuildValuesTable()
{
    if (!m_configLoaded) {
        ui->tableValues->setRowCount(0);
        return;
    }

    const auto &params = m_config.params();
    ui->tableValues->setRowCount(params.size());
    for (int i = 0; i < params.size(); ++i) {
        const ModulatorParam &p = params[i];
        const QString caption = p.label.isEmpty() ? p.key : p.label;

        auto *captionItem = new QTableWidgetItem(caption);
        captionItem->setData(Qt::UserRole, p.target);   // для быстрого поиска
        ui->tableValues->setItem(i, 0, captionItem);

        ui->tableValues->setItem(i, 1, new QTableWidgetItem("--"));
    }
    ui->tableValues->resizeRowsToContents();
}

// ============================================================================
// ✅ ОБНОВЛЕНИЕ ЗНАЧЕНИЙ
// ============================================================================
void ModulatorTab::refreshTableValues(const ModulatorData &data)
{
    if (!m_configLoaded) return;

    const auto &params = m_config.params();
    for (int i = 0; i < params.size() && i < ui->tableValues->rowCount(); ++i) {
        const ModulatorParam &p = params[i];
        QTableWidgetItem *valItem = ui->tableValues->item(i, 1);
        if (!valItem) continue;

        const QVariant v = ModulatorDriver::valueByTarget(data, p.target);
        QString text;
        if (!v.isValid()) {
            text = "--";
        } else if (p.type == ParamType::String) {
            text = v.toString();
            if (text.isEmpty()) text = "--";
        } else if (p.type == ParamType::Int || p.type == ParamType::Bool) {
            text = QString::number(static_cast<int>(v.toDouble()));
        } else {
            text = QString::number(v.toDouble(), 'f', 2);
        }
        valItem->setText(text);
    }
}

// ============================================================================
// ✅ REFRESH (по таймеру из ModulatorControlDialog)
// ============================================================================
void ModulatorTab::refreshData()
{
    if (!m_configLoaded) return;

    // ModulatorData лежит в BaseDocWidget::m_lastDeviceData по deviceId
    auto data = std::dynamic_pointer_cast<ModulatorData>(
        m_docWidget->getDeviceData(m_deviceId));

    if (!data) {
        ui->labelStatus->setText("Статус: нет данных");
        ui->labelStatus->setStyleSheet("color: #666666;");
        // Очистить значения
        for (int i = 0; i < ui->tableValues->rowCount(); ++i) {
            if (auto *it = ui->tableValues->item(i, 1)) it->setText("--");
        }
        return;
    }

    // Значения
    refreshTableValues(*data);

    // Статус — по ошибкам
    if (data->hasErrors()) {
        QStringList codes;
        for (int c : data->getErrors().codes) codes << QString::number(c);
        ui->labelStatus->setText("Статус: АВАРИЯ [" + codes.join(", ") + "]");
        ui->labelStatus->setStyleSheet("color: #C62828; font-weight: bold;");
    } else {
        ui->labelStatus->setText("Статус: НОРМА");
        ui->labelStatus->setStyleSheet("color: #2E7D32; font-weight: bold;");
    }
}

// ============================================================================
// ✅ SHOW EVENT — перечитать конфиг (мог измениться в редакторе)
// ============================================================================
void ModulatorTab::showEvent(QShowEvent *event)
{
    ModulatorTabBase::showEvent(event);

    const bool wasLoaded = m_configLoaded;
    loadConfigForDevice();
    // Если конфиг был/стал загружен — перестроить таблицу
    if (m_configLoaded || wasLoaded) {
        rebuildValuesTable();
    }
}

// ============================================================================
// ✅ КОМАНДЫ — ЗАГЛУШКИ (отдельная задача)
// ============================================================================
void ModulatorTab::onTurnOnClicked()
{
    // TODO: сформировать ControlCommand и отправить через m_docWidget->sendCommandToDevice
}

void ModulatorTab::onTurnOffClicked()
{
    // TODO
}

void ModulatorTab::onResetAlarmsClicked()
{
    // TODO
}
