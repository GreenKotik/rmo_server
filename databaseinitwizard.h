#ifndef DATABASEINITWIZARD_H
#define DATABASEINITWIZARD_H

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
#include <QTextEdit>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QCoreApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDebug>
#include <QScrollBar>       // ✅ ДОБАВИТЬ ЭТУ СТРОКУ
#include <QApplication>       // ✅ ДОБАВИТЬ ЭТУ СТРОКУ
#include "databasemanager.h"  // Для CryptoHelper

// ============================================================================
// ✅ МАСТЕР ИНИЦИАЛИЗАЦИИ БАЗЫ ДАННЫХ
// Запускается при первом запуске программы, когда БД ещё не создана
// ============================================================================
class DatabaseInitWizard : public QDialog
{
    Q_OBJECT
public:
    explicit DatabaseInitWizard(const QString &currentHost, int currentPort,
                                 const QString &currentDbName,
                                 QWidget *parent = nullptr)
        : QDialog(parent)
        , m_host(currentHost)
        , m_port(currentPort)
        , m_dbName(currentDbName)
    {
        setWindowTitle("Мастер инициализации базы данных");
        setFixedSize(600, 600);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // === Заголовок ===
        QLabel *headerLabel = new QLabel(
            "<b>🔧 Первоначальная настройка базы данных</b><br>"
            "База данных <b>rmo_db</b> не найдена на сервере PostgreSQL.<br>"
            "Мастер создаст базу данных, таблицу пользователей и администратора.",
            this);
        headerLabel->setWordWrap(true);
        headerLabel->setStyleSheet("color: #005A9E; font-size: 12px; margin-bottom: 10px;");
        mainLayout->addWidget(headerLabel);

        // === Блок "Подключение к PostgreSQL" ===
        QGroupBox *connGroup = new QGroupBox("Параметры подключения к PostgreSQL", this);
        QFormLayout *connLayout = new QFormLayout(connGroup);

        m_hostEdit = new QLineEdit(m_host, this);
        m_hostEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        connLayout->addRow("Хост:", m_hostEdit);

        m_portSpin = new QSpinBox(this);
        m_portSpin->setRange(1, 65535);
        m_portSpin->setValue(m_port);
        m_portSpin->setStyleSheet(
            "QSpinBox { padding: 4px; border: 1px solid #B0B0B0; border-radius: 3px; }"
        );
        connLayout->addRow("Порт:", m_portSpin);

        m_dbNameEdit = new QLineEdit(m_dbName, this);
        m_dbNameEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        connLayout->addRow("Имя новой базы данных:", m_dbNameEdit);

        mainLayout->addWidget(connGroup);

        // === Блок "Учётные данные PostgreSQL" ===
        QGroupBox *pgGroup = new QGroupBox("Учётные данные суперпользователя PostgreSQL", this);
        QFormLayout *pgLayout = new QFormLayout(pgGroup);

        m_pgUserEdit = new QLineEdit("postgres", this);
        m_pgUserEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        pgLayout->addRow("Пользователь PostgreSQL:", m_pgUserEdit);

        QHBoxLayout *passLayout = new QHBoxLayout();
        m_pgPassEdit = new QLineEdit(this);
        m_pgPassEdit->setEchoMode(QLineEdit::Password);
        m_pgPassEdit->setPlaceholderText("Введите пароль postgres...");
        m_pgPassEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        passLayout->addWidget(m_pgPassEdit);

        m_showPassCheck = new QCheckBox("Показать", this);
        connect(m_showPassCheck, &QCheckBox::toggled, this, [this](bool checked) {
            m_pgPassEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        });
        passLayout->addWidget(m_showPassCheck);
        pgLayout->addRow("Пароль PostgreSQL:", passLayout);

        mainLayout->addWidget(pgGroup);

        // === Блок "Администратор RMO" ===
        QGroupBox *adminGroup = new QGroupBox("Создание администратора RMO", this);
        QFormLayout *adminLayout = new QFormLayout(adminGroup);

        m_adminUserEdit = new QLineEdit("admin", this);
        m_adminUserEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        adminLayout->addRow("Логин администратора:", m_adminUserEdit);

        QHBoxLayout *adminPassLayout = new QHBoxLayout();
        m_adminPassEdit = new QLineEdit("admin", this);
        m_adminPassEdit->setEchoMode(QLineEdit::Password);
        m_adminPassEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        adminPassLayout->addWidget(m_adminPassEdit);

        m_showAdminPassCheck = new QCheckBox("Показать", this);
        connect(m_showAdminPassCheck, &QCheckBox::toggled, this, [this](bool checked) {
            m_adminPassEdit->setEchoMode(checked ? QLineEdit::Normal : QLineEdit::Password);
        });
        adminPassLayout->addWidget(m_showAdminPassCheck);
        adminLayout->addRow("Пароль администратора:", adminPassLayout);

        mainLayout->addWidget(adminGroup);

        // === Лог операций ===
        m_logEdit = new QTextEdit(this);
        m_logEdit->setReadOnly(true);
        m_logEdit->setMaximumHeight(100);
        m_logEdit->setStyleSheet(
            "QTextEdit { background-color: #F5F5F5; border: 1px solid #B0B0B0; "
            "font-family: 'Consolas', monospace; font-size: 11px; }"
        );
        m_logEdit->setPlaceholderText("Лог операций будет отображаться здесь...");
        mainLayout->addWidget(m_logEdit);

        // === Кнопки ===
        QHBoxLayout *btnLayout = new QHBoxLayout();
        m_createBtn = new QPushButton("🔧 Создать базу данных", this);
        m_createBtn->setDefault(true);
        m_createBtn->setStyleSheet(
            "QPushButton { background-color: #2E7D32; color: white; padding: 8px 20px; "
            "border-radius: 4px; font-weight: bold; }"
            "QPushButton:hover { background-color: #1B5E20; }"
        );
        QPushButton *cancelBtn = new QPushButton("Отмена", this);
        cancelBtn->setStyleSheet(
            "QPushButton { padding: 8px 20px; border: 1px solid #B0B0B0; border-radius: 4px; }"
            "QPushButton:hover { background-color: #E0E0E0; }"
        );
        btnLayout->addWidget(m_createBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(m_createBtn, &QPushButton::clicked, this, &DatabaseInitWizard::onCreateClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);

        m_pgPassEdit->setFocus();

        appendLog("Готов к инициализации. Нажмите 'Создать базу данных'.");
    }

    // ✅ Получить результат: была ли инициализация успешной
    bool isInitialized() const { return m_initialized; }

private:
    void appendLog(const QString &message)
    {
        QString timestamp = QDateTime::currentDateTime().toString("HH:mm:ss");
        m_logEdit->append(QString("[%1] %2").arg(timestamp, message));
        // ✅ Прокрутка вниз
        QScrollBar *sb = m_logEdit->verticalScrollBar();
        sb->setValue(sb->maximum());
    }

    void onCreateClicked()
    {
        // === Валидация полей ===
        QString host = m_hostEdit->text().trimmed();
        int port = m_portSpin->value();
        QString dbName = m_dbNameEdit->text().trimmed();
        QString pgUser = m_pgUserEdit->text().trimmed();
        QString pgPass = m_pgPassEdit->text();
        QString adminUser = m_adminUserEdit->text().trimmed();
        QString adminPass = m_adminPassEdit->text();

        if (host.isEmpty() || dbName.isEmpty() || pgUser.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Заполните все обязательные поля.");
            return;
        }
        if (adminUser.isEmpty() || adminPass.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Укажите логин и пароль администратора RMO.");
            return;
        }
        if (adminPass.length() < 5) {
            QMessageBox::warning(this, "Ошибка", "Пароль администратора слишком короткий (минимум 5 символов).");
            return;
        }

        // === Блокируем кнопку ===
        m_createBtn->setEnabled(false);
        m_createBtn->setText("Выполняется...");
        QApplication::processEvents();

        // === ШАГ 1: Подключение к служебной БД "postgres" ===
        appendLog("Шаг 1/5: Подключение к служебной БД 'postgres'...");

        QSqlDatabase tempDb = QSqlDatabase::addDatabase("QPSQL", "init_temp_connection");
        tempDb.setHostName(host);
        tempDb.setPort(port);
        tempDb.setDatabaseName("postgres");  // ✅ Служебная БД, всегда есть
        tempDb.setUserName(pgUser);
        tempDb.setPassword(pgPass);

        if (!tempDb.open()) {
            appendLog("❌ ОШИБКА: " + tempDb.lastError().text());
            QMessageBox::critical(this, "Ошибка подключения",
                "Не удалось подключиться к PostgreSQL.\n\n"
                "Проверьте:\n"
                "• Запущен ли PostgreSQL сервер\n"
                "• Правильность логина/пароля пользователя postgres\n"
                "• Доступность хоста " + host + ":" + QString::number(port));
            // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
            {
                QSqlDatabase db = QSqlDatabase::database("init_temp_connection");
                if (db.isOpen()) db.close();
            }
            QSqlDatabase::removeDatabase("init_temp_connection");
            m_createBtn->setEnabled(true);
            m_createBtn->setText("🔧 Создать базу данных");
            return;
        }
        appendLog("✅ Успешное подключение к PostgreSQL");

        // === ШАГ 2: Проверка существования БД ===
        appendLog("Шаг 2/5: Проверка существования базы данных '" + dbName + "'...");

        QSqlQuery checkQuery(tempDb);
        checkQuery.prepare("SELECT 1 FROM pg_database WHERE datname = :dbname");
        checkQuery.bindValue(":dbname", dbName);

        bool dbExists = false;
        if (checkQuery.exec() && checkQuery.next()) {
            dbExists = true;
            appendLog("ℹ️  База данных '" + dbName + "' уже существует");
        }

        // === ШАГ 3: Создание БД (если нет) ===
        if (!dbExists) {
            appendLog("Шаг 3/5: Создание базы данных '" + dbName + "'...");

            // ⚠️ ВАЖНО: CREATE DATABASE нельзя выполнять в транзакции
            // и нельзя использовать bind values для имени БД
            QSqlQuery createDbQuery(tempDb);
            // ✅ Экранируем имя БД (защита от SQL-инъекций)
            QString safeDbName = dbName;
            safeDbName.replace("\"", "\"\"");

            if (!createDbQuery.exec(QString("CREATE DATABASE \"%1\"").arg(safeDbName))) {
                appendLog("❌ ОШИБКА создания БД: " + createDbQuery.lastError().text());
                QMessageBox::critical(this, "Ошибка",
                    "Не удалось создать базу данных:\n" + createDbQuery.lastError().text());
                // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
                {
                    QSqlDatabase db = QSqlDatabase::database("init_temp_connection");
                    if (db.isOpen()) db.close();
                }
                QSqlDatabase::removeDatabase("init_temp_connection");
                m_createBtn->setEnabled(true);
                m_createBtn->setText("🔧 Создать базу данных");
                return;
            }
            appendLog("✅ База данных '" + dbName + "' успешно создана");
        } else {
            appendLog("Шаг 3/5: Пропущен (БД уже существует)");
        }

        // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
        {
            QSqlDatabase db = QSqlDatabase::database("init_temp_connection");
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase("init_temp_connection");

        // === ШАГ 4: Подключение к новой БД и создание таблицы ===
        appendLog("Шаг 4/5: Создание таблицы пользователей...");

        QSqlDatabase newDb = QSqlDatabase::addDatabase("QPSQL", "init_new_db");
        newDb.setHostName(host);
        newDb.setPort(port);
        newDb.setDatabaseName(dbName);
        newDb.setUserName(pgUser);
        newDb.setPassword(pgPass);

        if (!newDb.open()) {
            appendLog("❌ ОШИБКА подключения к новой БД: " + newDb.lastError().text());
            QMessageBox::critical(this, "Ошибка",
                "Не удалось подключиться к созданной БД:\n" + newDb.lastError().text());
            // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
            {
                QSqlDatabase db = QSqlDatabase::database("init_new_db");
                if (db.isOpen()) db.close();
            }
            QSqlDatabase::removeDatabase("init_new_db");
            m_createBtn->setEnabled(true);
            m_createBtn->setText("🔧 Создать базу данных");
            return;
        }

        // ✅ Создание таблицы users (IF NOT EXISTS — безопасно)
        QSqlQuery createTableQuery(newDb);
        QString createTableSql = R"(
            CREATE TABLE IF NOT EXISTS users (
                id SERIAL PRIMARY KEY,
                username TEXT NOT NULL,
                password TEXT NOT NULL,
                role TEXT NOT NULL,
                is_active BOOLEAN DEFAULT TRUE,
                password_changed_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
            )
        )";

        if (!createTableQuery.exec(createTableSql)) {
            appendLog("❌ ОШИБКА создания таблицы: " + createTableQuery.lastError().text());
            QMessageBox::critical(this, "Ошибка",
                "Не удалось создать таблицу users:\n" + createTableQuery.lastError().text());
            // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
            {
                QSqlDatabase db = QSqlDatabase::database("init_new_db");
                if (db.isOpen()) db.close();
            }
            QSqlDatabase::removeDatabase("init_new_db");
            m_createBtn->setEnabled(true);
            m_createBtn->setText("🔧 Создать базу данных");
            return;
        }
        appendLog("✅ Таблица 'users' создана/проверена");

        // === ШАГ 5: Создание администратора ===
        appendLog("Шаг 5/5: Создание администратора '" + adminUser + "'...");

        // ✅ Проверяем, есть ли уже такой пользователь
        QSqlQuery checkAdminQuery(newDb);
        checkAdminQuery.prepare("SELECT COUNT(*) FROM users WHERE username = :username");
        checkAdminQuery.bindValue(":username", CryptoHelper::encrypt(adminUser));

        bool adminExists = false;
        if (checkAdminQuery.exec() && checkAdminQuery.next()) {
            adminExists = (checkAdminQuery.value(0).toInt() > 0);
        }

        if (adminExists) {
            appendLog("ℹ️  Администратор '" + adminUser + "' уже существует. Сброс пароля...");

            // ✅ Сбрасываем пароль
            QSqlQuery resetQuery(newDb);
            resetQuery.prepare(
                "UPDATE users SET password = :password, role = :role, is_active = TRUE, "
                "password_changed_at = CURRENT_TIMESTAMP WHERE username = :username"
            );
            resetQuery.bindValue(":password", CryptoHelper::createPasswordHash(adminPass));
            resetQuery.bindValue(":role", CryptoHelper::encrypt("admin"));
            resetQuery.bindValue(":username", CryptoHelper::encrypt(adminUser));

            if (!resetQuery.exec()) {
                appendLog("❌ ОШИБКА сброса пароля: " + resetQuery.lastError().text());
            } else {
                appendLog("✅ Пароль администратора сброшен");
            }
        } else {
            // ✅ Создаём нового администратора
            QSqlQuery insertAdminQuery(newDb);
            insertAdminQuery.prepare(
                "INSERT INTO users (username, password, role, is_active) "
                "VALUES (:username, :password, :role, TRUE)"
            );
            insertAdminQuery.bindValue(":username", CryptoHelper::encrypt(adminUser));
            insertAdminQuery.bindValue(":password", CryptoHelper::createPasswordHash(adminPass));
            insertAdminQuery.bindValue(":role", CryptoHelper::encrypt("admin"));

            if (!insertAdminQuery.exec()) {
                appendLog("❌ ОШИБКА создания администратора: " + insertAdminQuery.lastError().text());
                QMessageBox::critical(this, "Ошибка",
                    "Не удалось создать администратора:\n" + insertAdminQuery.lastError().text());
                // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
                {
                    QSqlDatabase db = QSqlDatabase::database("init_new_db");
                    if (db.isOpen()) db.close();
                }
                QSqlDatabase::removeDatabase("init_new_db");
                m_createBtn->setEnabled(true);
                m_createBtn->setText("🔧 Создать базу данных");
                return;
            }
            appendLog("✅ Администратор '" + adminUser + "' успешно создан");
        }

        // ✅ ПРАВИЛЬНОЕ ЗАКРЫТИЕ
        {
            QSqlDatabase db = QSqlDatabase::database("init_new_db");
            if (db.isOpen()) db.close();
        }
        QSqlDatabase::removeDatabase("init_new_db");

        // === ШАГ 6: Сохранение настроек в конфиг ===
        appendLog("Сохранение настроек в rmo_config.json...");

        QString configPath = QCoreApplication::applicationDirPath() + "/rmo_config.json";
        QFile file(configPath);
        QJsonObject obj;
        if (file.exists() && file.open(QIODevice::ReadOnly)) {
            QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
            obj = doc.object();
            file.close();
        }

        QJsonObject dbConfig;
        dbConfig["host"] = host;
        dbConfig["port"] = port;
        dbConfig["dbName"] = dbName;
        dbConfig["user"] = pgUser;  // ✅ Используем того же пользователя PostgreSQL
        dbConfig["password"] = CryptoHelper::encrypt(pgPass);
        obj["database"] = dbConfig;

        if (file.open(QIODevice::WriteOnly)) {
            file.write(QJsonDocument(obj).toJson());
            file.close();
            appendLog("✅ Настройки сохранены в rmo_config.json");
        } else {
            appendLog("⚠️  Не удалось сохранить конфиг: " + file.errorString());
        }

        // === УСПЕХ ===
        appendLog("");
        appendLog("🎉 ИНИЦИАЛИЗАЦИЯ ЗАВЕРШЕНА УСПЕШНО!");
        appendLog("Теперь программа будет подключаться к БД автоматически.");

        m_initialized = true;

        QMessageBox::information(this, "Успех",
            QString("✅ База данных успешно инициализирована!\n\n"
                    "Создано:\n"
                    "• База данных: %1\n"
                    "• Таблица: users\n"
                    "• Администратор: %2\n\n"
                    "Нажмите 'OK' для продолжения работы с программой.")
                .arg(dbName, adminUser));

        accept();
    }

private:
    QString m_host;
    int m_port;
    QString m_dbName;

    QLineEdit *m_hostEdit;
    QSpinBox *m_portSpin;
    QLineEdit *m_dbNameEdit;
    QLineEdit *m_pgUserEdit;
    QLineEdit *m_pgPassEdit;
    QCheckBox *m_showPassCheck;
    QLineEdit *m_adminUserEdit;
    QLineEdit *m_adminPassEdit;
    QCheckBox *m_showAdminPassCheck;
    QTextEdit *m_logEdit;
    QPushButton *m_createBtn;

    bool m_initialized = false;
};

#endif // DATABASEINITWIZARD_H
