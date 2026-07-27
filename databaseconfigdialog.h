#ifndef DATABASECONFIGDIALOG_H
#define DATABASECONFIGDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QSpinBox>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QCoreApplication>
#include "databasemanager.h" // Для CryptoHelper

class DatabaseConfigDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DatabaseConfigDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Конфигурация базы данных");
        setFixedSize(500, 400);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // === 1. Параметры подключения ===
        QGroupBox *connGroup = new QGroupBox("Параметры подключения к PostgreSQL", this);
        QFormLayout *connLayout = new QFormLayout(connGroup);

        hostEdit = new QLineEdit(this);
        hostEdit->setPlaceholderText("localhost");
        hostEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        connLayout->addRow("Хост:", hostEdit);

        portSpin = new QSpinBox(this);
        portSpin->setRange(1, 65535);
        portSpin->setValue(5432);
        portSpin->setStyleSheet(
            "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        connLayout->addRow("Порт:", portSpin);

        dbNameEdit = new QLineEdit(this);
        dbNameEdit->setPlaceholderText("rmo_db");
        dbNameEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        connLayout->addRow("Имя базы данных:", dbNameEdit);

        userEdit = new QLineEdit(this);
        userEdit->setPlaceholderText("postgres");
        userEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        connLayout->addRow("Пользователь:", userEdit);

        // ✅ Пароль с возможностью показать/скрыть
        QHBoxLayout *passLayout = new QHBoxLayout();
        passEdit = new QLineEdit(this);
        passEdit->setEchoMode(QLineEdit::Password);
        passEdit->setPlaceholderText("Введите пароль...");
        passEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        passLayout->addWidget(passEdit);

        showPassCheck = new QCheckBox("Показать", this);
        connect(showPassCheck, &QCheckBox::toggled, this, [this](bool checked) {
            passEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        });
        passLayout->addWidget(showPassCheck);

        connLayout->addRow("Пароль:", passLayout);

        mainLayout->addWidget(connGroup);

        // === 2. Подсказка ===
        QLabel *hintLabel = new QLabel(
            "💡 После изменения настроек потребуется перезапуск программы\n"
            "для применения изменений к подключению к базе данных.", this);
        hintLabel->setStyleSheet("color: #666; font-size: 11px;");
        hintLabel->setWordWrap(true);
        mainLayout->addWidget(hintLabel);

        mainLayout->addStretch();

        // === Кнопки ===
        QHBoxLayout *btnLayout = new QHBoxLayout();

        QPushButton *testBtn = new QPushButton("🔌 Проверить подключение", this);
        testBtn->setStyleSheet(
            "QPushButton { background-color: #2E7D32; color: white; padding: 8px 16px; "
            "border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background-color: #1B5E20; }"
        );
        connect(testBtn, &QPushButton::clicked, this, &DatabaseConfigDialog::onTestConnection);

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

        btnLayout->addWidget(testBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(okBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(okBtn, &QPushButton::clicked, this, &DatabaseConfigDialog::onSave);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        // Загрузка текущих настроек
        loadConfig();
    }

private:
    QLineEdit *hostEdit;
    QSpinBox *portSpin;
    QLineEdit *dbNameEdit;
    QLineEdit *userEdit;
    QLineEdit *passEdit;
    QCheckBox *showPassCheck;

    void loadConfig()
    {
        QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
        QFile file(configPath);

        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            QJsonObject obj = doc.object();
            QJsonObject dbConfig = obj.value("database").toObject();

            hostEdit->setText(dbConfig.value("host").toString("localhost"));
            portSpin->setValue(dbConfig.value("port").toInt(5432));
            dbNameEdit->setText(dbConfig.value("dbName").toString("rmo_db"));
            userEdit->setText(dbConfig.value("user").toString("postgres"));

            // ✅ Пароль хранится в зашифрованном виде
            QString encryptedPass = dbConfig.value("password").toString();
            if (!encryptedPass.isEmpty()) {
                passEdit->setText(CryptoHelper::decrypt(encryptedPass));
            }

            file.close();
        } else {
            // Значения по умолчанию
            hostEdit->setText("localhost");
            portSpin->setValue(5432);
            dbNameEdit->setText("rmo_db");
            userEdit->setText("postgres");
        }
    }

private slots:
    void onTestConnection()
    {
        QString host = hostEdit->text().trimmed();
        int port = portSpin->value();
        QString dbName = dbNameEdit->text().trimmed();
        QString user = userEdit->text().trimmed();
        QString pass = passEdit->text();

        if (host.isEmpty() || dbName.isEmpty() || user.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Заполните все обязательные поля.");
            return;
        }

        // Пытаемся подключиться к БД
        QSqlDatabase testDb = QSqlDatabase::addDatabase("QPSQL", "test_connection");
        testDb.setHostName(host);
        testDb.setPort(port);
        testDb.setDatabaseName(dbName);
        testDb.setUserName(user);
        testDb.setPassword(pass);

        if (testDb.open()) {
            QMessageBox::information(this, "Успех",
                QString("✅ Подключение успешно!\n\n"
                        "Хост: %1:%2\nБаза данных: %3\nПользователь: %4")
                    .arg(host).arg(port).arg(dbName).arg(user));
            testDb.close();
        } else {
            QMessageBox::critical(this, "Ошибка подключения",
                QString("❌ Не удалось подключиться к базе данных.\n\n"
                        "Ошибка: %1\n\n"
                        "Проверьте:\n"
                        "• Запущен ли PostgreSQL сервер\n"
                        "• Правильность хоста, порта, имени БД\n"
                        "• Правильность логина и пароля")
                    .arg(testDb.lastError().text()));
        }

        QSqlDatabase::removeDatabase("test_connection");
    }

    void onSave()
    {
        QString host = hostEdit->text().trimmed();
        QString dbName = dbNameEdit->text().trimmed();
        QString user = userEdit->text().trimmed();

        if (host.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите хост базы данных.");
            hostEdit->setFocus();
            return;
        }
        if (dbName.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите имя базы данных.");
            dbNameEdit->setFocus();
            return;
        }
        if (user.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите имя пользователя.");
            userEdit->setFocus();
            return;
        }

        QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
        QFile file(configPath);

        QJsonObject obj;

        // Читаем существующий конфиг (чтобы не потерять другие настройки)
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            obj = doc.object();
            file.close();
        }

        // ✅ Формируем секцию database
        QJsonObject dbConfig;
        dbConfig["host"] = host;
        dbConfig["port"] = portSpin->value();
        dbConfig["dbName"] = dbName;
        dbConfig["user"] = user;

        // ✅ Пароль шифруем перед сохранением
        QString password = passEdit->text();
        if (!password.isEmpty()) {
            dbConfig["password"] = CryptoHelper::encrypt(password);
        } else {
            dbConfig["password"] = "";
        }

        obj["database"] = dbConfig;

        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
            file.close();

            QMessageBox::information(this, "Успех",
                "Конфигурация базы данных сохранена.\n"
                "Изменения вступят в силу при следующем запуске программы.");
            accept();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось сохранить конфигурацию.");
        }
    }
};

#endif // DATABASECONFIGDIALOG_H
