#include "switchtab.h"
#include "basedocwidget.h"
#include "commandconfirmdialog.h"
#include "mainwindow.h"
#include <QDebug>

SwitchTab::SwitchTab(BaseDocWidget *docWidget, QWidget *parent)
    : ModulatorTabBase(docWidget, parent)
    , ui(new Ui::SwitchTabForm)
    , m_slotIndex(3)
    , m_deviceId(ID_UNKNOWN)
{
    ui->setupUi(this);

    auto connections = docWidget->getEquipmentConnections();
    if (m_slotIndex < connections.size()) {
        m_deviceId = connections[m_slotIndex].deviceId;
        m_deviceName = connections[m_slotIndex].name;
    }

    ui->labelTitle->setText(QString("<b>%1</b>").arg(m_deviceName));

    connect(ui->btnToMod1, &QPushButton::clicked, this, &SwitchTab::onSwitchToMod1Clicked);
    connect(ui->btnToMod2, &QPushButton::clicked, this, &SwitchTab::onSwitchToMod2Clicked);
    connect(ui->btnAuto, &QPushButton::clicked, this, &SwitchTab::onSetAutoModeClicked);
    connect(ui->btnManual, &QPushButton::clicked, this, &SwitchTab::onSetManualModeClicked);
}

SwitchTab::~SwitchTab()
{
    delete ui;
}

void SwitchTab::refreshData()
{
    if (!m_docWidget || m_deviceId == ID_UNKNOWN) return;

    auto data = m_docWidget->getDeviceData(m_deviceId);
    if (!data) {
        ui->labelStatus->setText("Статус: нет данных");
        ui->labelActiveMod->setText("Активный модулятор: --");
        ui->labelMode->setText("Режим: --");
        return;
    }

    // TODO: Когда появится реальный драйвер, отображать реальные данные
    ui->labelStatus->setText("Статус: OK");
    ui->labelActiveMod->setText("Активный модулятор: Mod 1");
    ui->labelMode->setText("Режим: Auto");
}

void SwitchTab::onSwitchToMod1Clicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Переключить на Модулятор 1",
        "Произойдёт переключение активного пути на Модулятор 1.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 10;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[SwitchTab] Команда 'Переключить на Mod 1' отправлена";
    }
}

void SwitchTab::onSwitchToMod2Clicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Переключить на Модулятор 2",
        "Произойдёт переключение активного пути на Модулятор 2.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 11;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[SwitchTab] Команда 'Переключить на Mod 2' отправлена";
    }
}

void SwitchTab::onSetAutoModeClicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Установить режим Auto",
        "Переключатель перейдёт в автоматический режим работы.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 20;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[SwitchTab] Команда 'Режим Auto' отправлена";
    }
}

void SwitchTab::onSetManualModeClicked()
{
    MainWindow *mainWin = qobject_cast<MainWindow*>(m_docWidget->parent());
    QString currentUser = mainWin ? mainWin->getCurrentUser() : "";

    CommandConfirmDialog dlg(
        "Установить режим Manual",
        "Переключатель перейдёт в ручной режим работы.",
        "engineer",
        currentUser,
        this
    );

    if (dlg.exec() == QDialog::Accepted) {
        ControlCommand cmd;
        cmd.commandId = 21;
        m_docWidget->sendCommandToDevice(m_deviceName, cmd);
        qDebug() << "[SwitchTab] Команда 'Режим Manual' отправлена";
    }
}
