#include "modulatortab.h"
#include "basedocwidget.h"
#include "devicedata.h"
#include "commandconfirmdialog.h"
#include "mainwindow.h"
#include <QMessageBox>
#include <QDebug>

ModulatorTab::ModulatorTab(BaseDocWidget *docWidget, int slotIndex, QWidget *parent)
    : ModulatorTabBase(docWidget, parent)
    , ui(new Ui::ModulatorTabForm)
    , m_slotIndex(slotIndex)
    , m_deviceId(ID_UNKNOWN)
{
    ui->setupUi(this);

    auto connections = docWidget->getEquipmentConnections();
    if (slotIndex >= 0 && slotIndex < connections.size()) {
        m_deviceId = connections[slotIndex].deviceId;
        m_deviceName = connections[slotIndex].name;
    }

    ui->labelTitle->setText(QString("<b>%1</b>").arg(m_deviceName));

    connect(ui->btnTurnOn, &QPushButton::clicked, this, &ModulatorTab::onTurnOnClicked);
    connect(ui->btnTurnOff, &QPushButton::clicked, this, &ModulatorTab::onTurnOffClicked);
    connect(ui->btnResetAlarms, &QPushButton::clicked, this, &ModulatorTab::onResetAlarmsClicked);
}

ModulatorTab::~ModulatorTab()
{
    delete ui;
}

void ModulatorTab::refreshData()
{
    if (!m_docWidget || m_deviceId == ID_UNKNOWN) return;

    auto data = m_docWidget->getDeviceData(m_deviceId);
    if (!data) {
        ui->labelStatus->setText("Статус: нет данных");
        ui->labelPower->setText("Питание: --");
        ui->labelFrequency->setText("Частота: -- MHz");
        return;
    }

    // TODO: Когда появятся реальные драйверы, использовать dynamic_pointer_cast
    ui->labelStatus->setText("Статус: OK");
    ui->labelPower->setText("Питание: Вкл");
    ui->labelFrequency->setText("Частота: 14500.000 MHz");
}

void ModulatorTab::onTurnOnClicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Включить " + m_deviceName,
        "Произойдёт включение модулятора. Убедитесь, что все системы готовы к работе.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 1;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[ModulatorTab] Команда 'Включить' отправлена:" << m_deviceName;
    }
}

void ModulatorTab::onTurnOffClicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Выключить " + m_deviceName,
        "Произойдёт выключение модулятора. Убедитесь, что это не нарушит работу системы.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 2;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[ModulatorTab] Команда 'Выключить' отправлена:" << m_deviceName;
    }
}

void ModulatorTab::onResetAlarmsClicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Сбросить аварии " + m_deviceName,
        "Все текущие аварии будут сброшены. Убедитесь, что причины аварий устранены.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 3;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[ModulatorTab] Команда 'Сбросить аварии' отправлена:" << m_deviceName;
    }
}
