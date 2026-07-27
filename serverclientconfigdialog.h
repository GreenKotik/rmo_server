#ifndef SERVERCLIENTCONFIGDIALOG_H
#define SERVERCLIENTCONFIGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QStackedWidget>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>

class ServerClientConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ServerClientConfigDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Конфигурация сервер/клиент");
        setFixedSize(500, 450);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // === 1. Название программы ===
        QGroupBox *nameGroup = new QGroupBox("Общие настройки", this);
        QFormLayout *nameLayout = new QFormLayout(nameGroup);

        programNameEdit = new QLineEdit(this);
        programNameEdit->setPlaceholderText("Например: RMO Server");
        programNameEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        nameLayout->addRow("Название программы:", programNameEdit);

        mainLayout->addWidget(nameGroup);

        // === 2. Выбор режима ===
        QGroupBox *modeGroup = new QGroupBox("Режим работы", this);
        QVBoxLayout *modeLayout = new QVBoxLayout(modeGroup);

        modeCombo = new QComboBox(this);
        modeCombo->addItems({"Сервер", "Клиент"});
        modeCombo->setStyleSheet(
            "QComboBox { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QComboBox:focus { border: 1px solid #005A9E; }"
        );

        QHBoxLayout *modeRow = new QHBoxLayout();
        modeRow->addWidget(new QLabel("Режим:", this));
        modeRow->addWidget(modeCombo);
        modeRow->addStretch();
        modeLayout->addLayout(modeRow);

        mainLayout->addWidget(modeGroup);

        // === 3. Параметры (StackedWidget для переключения) ===
        stackedWidget = new QStackedWidget(this);

        // --- Страница 0: Параметры сервера ---
        QWidget *serverPage = new QWidget();
        QFormLayout *serverLayout = new QFormLayout(serverPage);

        serverPortSpin = new QSpinBox(this);
        serverPortSpin->setRange(1, 65535);
        serverPortSpin->setValue(5000);
        serverPortSpin->setStyleSheet(
            "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        serverLayout->addRow("Порт сервера:", serverPortSpin);

        QLabel *serverHint = new QLabel(
            "Сервер будет слушать входящие подключения\n"
            "клиентов на указанном порту.", this);
        serverHint->setStyleSheet("color: #666; font-size: 11px;");
        serverHint->setWordWrap(true);
        serverLayout->addRow("", serverHint);

        stackedWidget->addWidget(serverPage);

        // --- Страница 1: Параметры клиента ---
        QWidget *clientPage = new QWidget();
        QFormLayout *clientLayout = new QFormLayout(clientPage);

        clientIpEdit = new QLineEdit(this);
        clientIpEdit->setPlaceholderText("127.0.0.1");
        clientIpEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        clientLayout->addRow("IP-адрес сервера:", clientIpEdit);

        clientPortSpin = new QSpinBox(this);
        clientPortSpin->setRange(1, 65535);
        clientPortSpin->setValue(5000);
        clientPortSpin->setStyleSheet(
            "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        clientLayout->addRow("Порт сервера:", clientPortSpin);

        QLabel *clientHint = new QLabel(
            "Клиент подключится к серверу по указанному\n"
            "IP-адресу и порту.", this);
        clientHint->setStyleSheet("color: #666; font-size: 11px;");
        clientHint->setWordWrap(true);
        clientLayout->addRow("", clientHint);

        stackedWidget->addWidget(clientPage);

        mainLayout->addWidget(stackedWidget);

        mainLayout->addStretch();

        // === Кнопки ===
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *okBtn = new QPushButton("Сохранить", this);
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

        connect(okBtn, &QPushButton::clicked, this, &ServerClientConfigDialog::onSave);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(modeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                stackedWidget, &QStackedWidget::setCurrentIndex);

        // Загрузка текущих настроек
        loadConfig();
    }

    // ✅ Получить настройки после закрытия
    QString getProgramName() const { return programNameEdit->text(); }
    QString getMode() const { return modeCombo->currentText() == "Сервер" ? "server" : "client"; }
    int getServerPort() const { return serverPortSpin->value(); }
    QString getClientServerIp() const { return clientIpEdit->text(); }
    int getClientServerPort() const { return clientPortSpin->value(); }

private:
    QLineEdit *programNameEdit;
    QComboBox *modeCombo;
    QStackedWidget *stackedWidget;

    // Сервер
    QSpinBox *serverPortSpin;

    // Клиент
    QLineEdit *clientIpEdit;
    QSpinBox *clientPortSpin;

    void loadConfig()
    {
        QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
        QFile file(configPath);

        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject obj = doc.object();

            programNameEdit->setText(obj.value("programName").toString("RMO"));

            QString mode = obj.value("mode").toString("server");
            if (mode == "server") {
                modeCombo->setCurrentIndex(0);
            } else {
                modeCombo->setCurrentIndex(1);
            }

            serverPortSpin->setValue(obj.value("serverPort").toInt(5000));
            clientIpEdit->setText(obj.value("clientServerIp").toString("127.0.0.1"));
            clientPortSpin->setValue(obj.value("clientServerPort").toInt(5000));

            file.close();
        } else {
            // Значения по умолчанию
            programNameEdit->setText("RMO");
            modeCombo->setCurrentIndex(0);
        }
    }

private slots:
    void onSave()
    {
        if (programNameEdit->text().trimmed().isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите название программы.");
            programNameEdit->setFocus();
            return;
        }

        QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
        QFile file(configPath);

        QJsonObject obj;

        // Читаем существующий конфиг (чтобы не потерять documents)
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            obj = doc.object();
            file.close();
        }

        // Обновляем настройки сервера/клиента
        obj["programName"] = programNameEdit->text().trimmed();
        obj["mode"] = modeCombo->currentText() == "Сервер" ? "server" : "client";
        obj["serverPort"] = serverPortSpin->value();
        obj["clientServerIp"] = clientIpEdit->text();
        obj["clientServerPort"] = clientPortSpin->value();

        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
            file.close();

            QMessageBox::information(this, "Успех",
                "Конфигурация сохранена.\n"
                "Изменения вступят в силу при следующем запуске программы.");
            accept();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось сохранить конфигурацию.");
        }
    }
};

#endif // SERVERCLIENTCONFIGDIALOG_H
