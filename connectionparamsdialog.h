#ifndef CONNECTIONPARAMSDIALOG_H
#define CONNECTIONPARAMSDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QSerialPortInfo>
#include "mainwindow.h"

class ConnectionParamsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ConnectionParamsDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Параметры соединения");
        setFixedSize(550, 750);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(10);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // === 1. Блок "Интерфейс" ===
        QGroupBox *interfaceGroup = new QGroupBox("Интерфейс", this);
        QFormLayout *interfaceLayout = new QFormLayout(interfaceGroup);

        interfaceCombo = new QComboBox(this);
        interfaceCombo->addItems({
            "TCP/IP",
            "SNMP",
            "Modbus TCP",
            "Modbus RTU",
            "RS-232",
            "RS-422",
            "RS-485"
        });
        interfaceCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QComboBox:focus { border: 1px solid #005A9E; }"
        );
        interfaceLayout->addRow("Тип интерфейса:", interfaceCombo);

        mainLayout->addWidget(interfaceGroup);

        // === 2. Блок "Адрес подключения" (для TCP/IP, Modbus TCP, SNMP) ===
        addressGroup = new QGroupBox("Адрес подключения", this);
        QFormLayout *addressLayout = new QFormLayout(addressGroup);

        ipEdit = new QLineEdit(this);
        ipEdit->setPlaceholderText("192.168.1.100");
        ipEdit->setStyleSheet(
            "QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        addressLayout->addRow("IP-адрес:", ipEdit);

        portSpin = new QSpinBox(this);
        portSpin->setRange(1, 65535);
        portSpin->setValue(502);
        portSpin->setStyleSheet(
            "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        addressLayout->addRow("Порт:", portSpin);

        mainLayout->addWidget(addressGroup);

        // === 3. Блок "Последовательный порт" (для Modbus RTU, RS-232/422/485) ===
        serialGroup = new QGroupBox("Последовательный порт", this);
        QFormLayout *serialLayout = new QFormLayout(serialGroup);

        comPortCombo = new QComboBox(this);
        comPortCombo->setEditable(true);
        refreshComPorts();
        comPortCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QComboBox:focus { border: 1px solid #005A9E; }"
        );
        serialLayout->addRow("COM-порт:", comPortCombo);

        baudRateCombo = new QComboBox(this);
        baudRateCombo->addItems({"9600", "19200", "38400", "57600", "115200"});
        baudRateCombo->setCurrentText("9600");
        baudRateCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        serialLayout->addRow("Скорость (baud):", baudRateCombo);

        dataBitsCombo = new QComboBox(this);
        dataBitsCombo->addItems({"7", "8"});
        dataBitsCombo->setCurrentText("8");
        dataBitsCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        serialLayout->addRow("Биты данных:", dataBitsCombo);

        parityCombo = new QComboBox(this);
        parityCombo->addItems({"None", "Even", "Odd"});
        parityCombo->setCurrentText("None");
        parityCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        serialLayout->addRow("Чётность:", parityCombo);

        stopBitsCombo = new QComboBox(this);
        stopBitsCombo->addItems({"1", "2"});
        stopBitsCombo->setCurrentText("1");
        stopBitsCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        serialLayout->addRow("Стоп-биты:", stopBitsCombo);

        mainLayout->addWidget(serialGroup);

        // === 4. Блок "SNMP V1" ===
        snmpV1Group = new QGroupBox("SNMP V1", this);
        snmpV1Group->setCheckable(true);
        snmpV1Group->setChecked(false);
        snmpV1Group->setStyleSheet(
            "QGroupBox { font-weight: bold; border: 1px solid #B0B0B0; "
            "border-radius: 4px; margin-top: 10px; padding-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }"
        );
        QFormLayout *snmpV1Layout = new QFormLayout(snmpV1Group);

        communityReadEdit = new QLineEdit(this);
        communityReadEdit->setText("public");
        communityReadEdit->setPlaceholderText("Обычно: public");
        communityReadEdit->setStyleSheet(
            "QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        snmpV1Layout->addRow("Community (чтение):", communityReadEdit);

        communityWriteEdit = new QLineEdit(this);
        communityWriteEdit->setText("private");
        communityWriteEdit->setPlaceholderText("Обычно: private");
        communityWriteEdit->setStyleSheet(
            "QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        snmpV1Layout->addRow("Community (запись):", communityWriteEdit);

        mainLayout->addWidget(snmpV1Group);

        // === 5. Блок "SNMP V3" ===
        snmpV3Group = new QGroupBox("SNMP V3", this);
        snmpV3Group->setCheckable(true);
        snmpV3Group->setChecked(false);
        snmpV3Group->setStyleSheet(
            "QGroupBox { font-weight: bold; border: 1px solid #B0B0B0; "
            "border-radius: 4px; margin-top: 10px; padding-top: 10px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }"
        );
        QFormLayout *snmpV3Layout = new QFormLayout(snmpV3Group);

        snmpV3UsernameEdit = new QLineEdit(this);
        snmpV3UsernameEdit->setStyleSheet(
            "QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        snmpV3Layout->addRow("Имя пользователя:", snmpV3UsernameEdit);

        snmpV3AuthProtoCombo = new QComboBox(this);
        snmpV3AuthProtoCombo->addItems({"MD5", "SHA"});
        snmpV3AuthProtoCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        snmpV3Layout->addRow("Протокол аутентификации:", snmpV3AuthProtoCombo);

        snmpV3AuthPassEdit = new QLineEdit(this);
        snmpV3AuthPassEdit->setEchoMode(QLineEdit::Password);
        snmpV3AuthPassEdit->setStyleSheet(
            "QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        snmpV3Layout->addRow("Пароль аутентификации:", snmpV3AuthPassEdit);

        snmpV3PrivProtoCombo = new QComboBox(this);
        snmpV3PrivProtoCombo->addItems({"DES", "AES"});
        snmpV3PrivProtoCombo->setStyleSheet(
            "QComboBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        snmpV3Layout->addRow("Протокол шифрования:", snmpV3PrivProtoCombo);

        snmpV3PrivPassEdit = new QLineEdit(this);
        snmpV3PrivPassEdit->setEchoMode(QLineEdit::Password);
        snmpV3PrivPassEdit->setStyleSheet(
            "QLineEdit { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        snmpV3Layout->addRow("Пароль шифрования:", snmpV3PrivPassEdit);

        mainLayout->addWidget(snmpV3Group);

        mainLayout->addStretch();

        // === Кнопки ===
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *okBtn = new QPushButton("OK", this);
        okBtn->setDefault(true);
        okBtn->setStyleSheet(
            "QPushButton { background-color: #005A9E; color: white; padding: 8px 20px; "
            "border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background-color: #004A8E; }"
        );
        QPushButton *cancelBtn = new QPushButton("Отмена", this);
        cancelBtn->setStyleSheet(
            "QPushButton { padding: 8px 20px; border: 1px solid #B0B0B0; border-radius: 4px; }"
            "QPushButton:hover { background-color: #E0E0E0; }"
        );
        btnLayout->addStretch();
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &QDialog::accept);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(interfaceCombo, &QComboBox::currentTextChanged,
                this, &ConnectionParamsDialog::onInterfaceChanged);

        // Инициализация видимости групп
        onInterfaceChanged("TCP/IP");
    }

    // ✅ Установить данные из структуры
    void setConnectionData(const EquipmentConnection &conn)
    {
        interfaceCombo->setCurrentText(conn.connectionInterface);
        ipEdit->setText(conn.ipAddress);
        portSpin->setValue(conn.port);

        // Последовательный порт
        if (!conn.comPort.isEmpty()) {
            comPortCombo->setCurrentText(conn.comPort);
        }
        if (conn.baudRate > 0) {
            baudRateCombo->setCurrentText(QString::number(conn.baudRate));
        }
        if (conn.dataBits > 0) {
            dataBitsCombo->setCurrentText(QString::number(conn.dataBits));
        }
        if (!conn.parity.isEmpty()) {
            parityCombo->setCurrentText(conn.parity);
        }
        if (conn.stopBits > 0) {
            stopBitsCombo->setCurrentText(QString::number(conn.stopBits));
        }

        // SNMP параметры
        snmpV1Group->setChecked(conn.snmpV1Enabled);
        communityReadEdit->setText(conn.snmpV1CommunityRead);
        communityWriteEdit->setText(conn.snmpV1CommunityWrite);
        snmpV3Group->setChecked(conn.snmpV3Enabled);
        snmpV3UsernameEdit->setText(conn.snmpV3Username);
        snmpV3AuthProtoCombo->setCurrentText(conn.snmpV3AuthProtocol);
        snmpV3AuthPassEdit->setText(conn.snmpV3AuthPassword);
        snmpV3PrivProtoCombo->setCurrentText(conn.snmpV3PrivProtocol);
        snmpV3PrivPassEdit->setText(conn.snmpV3PrivPassword);
    }

    // ✅ Получить данные в структуру
    EquipmentConnection getConnectionData() const
    {
        EquipmentConnection conn;
        conn.connectionInterface = interfaceCombo->currentText();
        conn.ipAddress = ipEdit->text();
        conn.port = portSpin->value();

        // Последовательный порт
        conn.comPort = comPortCombo->currentText();
        conn.baudRate = baudRateCombo->currentText().toInt();
        conn.dataBits = dataBitsCombo->currentText().toInt();
        conn.parity = parityCombo->currentText();
        conn.stopBits = stopBitsCombo->currentText().toInt();

        // SNMP параметры
        conn.snmpV1Enabled = snmpV1Group->isChecked();
        conn.snmpV1CommunityRead = communityReadEdit->text();
        conn.snmpV1CommunityWrite = communityWriteEdit->text();
        conn.snmpV3Enabled = snmpV3Group->isChecked();
        conn.snmpV3Username = snmpV3UsernameEdit->text();
        conn.snmpV3AuthProtocol = snmpV3AuthProtoCombo->currentText();
        conn.snmpV3AuthPassword = snmpV3AuthPassEdit->text();
        conn.snmpV3PrivProtocol = snmpV3PrivProtoCombo->currentText();
        conn.snmpV3PrivPassword = snmpV3PrivPassEdit->text();

        return conn;
    }

private slots:
    void onInterfaceChanged(const QString &interface)
    {
        // Определяем, какие блоки показывать
        bool showAddress = false;
        bool showSerial = false;
        bool showSnmp = false;

        if (interface == "TCP/IP" || interface == "Modbus TCP") {
            showAddress = true;
            portSpin->setValue(502);
        }
        else if (interface == "SNMP") {
            showAddress = true;
            showSnmp = true;
            portSpin->setValue(161);
        }
        else if (interface == "Modbus RTU" || interface == "RS-232" ||
                 interface == "RS-422" || interface == "RS-485") {
            showSerial = true;
        }

        addressGroup->setVisible(showAddress);
        serialGroup->setVisible(showSerial);
        snmpV1Group->setVisible(showSnmp);
        snmpV3Group->setVisible(showSnmp);
    }

private:
    void refreshComPorts()
    {
        comPortCombo->clear();
        const auto ports = QSerialPortInfo::availablePorts();
        for (const QSerialPortInfo &info : ports) {
            comPortCombo->addItem(info.portName());
        }
        // Если портов нет — добавляем заглушки для ручного ввода
        if (comPortCombo->count() == 0) {
            comPortCombo->addItem("COM1");
            comPortCombo->addItem("COM2");
            comPortCombo->addItem("/dev/ttyUSB0");
            comPortCombo->addItem("/dev/ttyS0");
        }
    }

    QComboBox *interfaceCombo;

    // TCP/IP адрес
    QGroupBox *addressGroup;
    QLineEdit *ipEdit;
    QSpinBox *portSpin;

    // Последовательный порт
    QGroupBox *serialGroup;
    QComboBox *comPortCombo;
    QComboBox *baudRateCombo;
    QComboBox *dataBitsCombo;
    QComboBox *parityCombo;
    QComboBox *stopBitsCombo;

    // SNMP V1
    QGroupBox *snmpV1Group;
    QLineEdit *communityReadEdit;
    QLineEdit *communityWriteEdit;

    // SNMP V3
    QGroupBox *snmpV3Group;
    QLineEdit *snmpV3UsernameEdit;
    QComboBox *snmpV3AuthProtoCombo;
    QLineEdit *snmpV3AuthPassEdit;
    QComboBox *snmpV3PrivProtoCombo;
    QLineEdit *snmpV3PrivPassEdit;
};

#endif // CONNECTIONPARAMSDIALOG_H
