#ifndef AUTHREQUIREDIALOG_H
#define AUTHREQUIREDIALOG_H

#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QDebug>
#include "databasemanager.h"
#include "authdialog.h"

class AuthRequiredDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AuthRequiredDialog(QWidget *parent = nullptr)
        : QDialog(parent), m_authenticated(false)
    {
        setWindowTitle("Требуется авторизация");
        setFixedSize(420, 280);
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // Заголовок
        QLabel *headerLabel = new QLabel(
            "<b>⚠️ Для сохранения изменений необходима авторизация</b>", this);
        headerLabel->setWordWrap(true);
        headerLabel->setStyleSheet("color: #C62828; font-size: 13px;");
        mainLayout->addWidget(headerLabel);

        QLabel *hintLabel = new QLabel(
            "Выберите пользователя и введите пароль:", this);
        hintLabel->setStyleSheet("color: #666; font-size: 12px;");
        mainLayout->addWidget(hintLabel);

        // Форма
        QFormLayout *formLayout = new QFormLayout();

        // ✅ ComboBox со списком пользователей
        userComboBox = new QComboBox(this);
        userComboBox->setMinimumWidth(200);
        userComboBox->setCurrentIndex(-1);
        userComboBox->setStyleSheet(
            "QComboBox { padding: 6px; border: 1px solid #B0B0B0; "
            "border-radius: 3px; background-color: white; }"
            "QComboBox:hover { border: 1px solid #005A9E; }"
        );

        // Загружаем пользователей из БД
        if (DatabaseManager::instance().isConnected()) {
            QStringList users = DatabaseManager::instance().getUsernames();
            userComboBox->addItems(users);
        } else {
            QMessageBox::warning(this, "Ошибка",
                "База данных не подключена.\nАвторизация невозможна.");
        }
        formLayout->addRow("Пользователь:", userComboBox);

        // ✅ НОВОЕ: Поле пароля прямо в этом диалоге
        passwordEdit = new QLineEdit(this);
        passwordEdit->setEchoMode(QLineEdit::Password);
        passwordEdit->setPlaceholderText("Введите пароль");
        passwordEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; "
            "border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        formLayout->addRow("Пароль:", passwordEdit);

        mainLayout->addLayout(formLayout);

        // Статус
        statusLabel = new QLabel("", this);
        statusLabel->setStyleSheet("color: #C62828; font-weight: bold; font-size: 12px;");
        statusLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(statusLabel);

        mainLayout->addStretch();

        // Кнопки
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *authBtn = new QPushButton("🔐 Авторизоваться", this);
        authBtn->setDefault(true);
        authBtn->setStyleSheet(
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
        btnLayout->addWidget(authBtn);
        btnLayout->addWidget(cancelBtn);
        mainLayout->addLayout(btnLayout);

        connect(authBtn, &QPushButton::clicked, this, &AuthRequiredDialog::onAuthenticate);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        // ✅ Enter в поле пароля = авторизация
        connect(passwordEdit, &QLineEdit::returnPressed, this, &AuthRequiredDialog::onAuthenticate);
    }

    // ✅ Получить имя авторизованного пользователя
    QString authenticatedUser() const { return m_authenticatedUser; }
    bool isAuthenticated() const { return m_authenticated; }

private slots:
    void onAuthenticate()
    {
        QString username = userComboBox->currentText();
        QString password = passwordEdit->text();

        if (username.isEmpty()) {
            statusLabel->setText("⚠️ Выберите пользователя");
            return;
        }
        if (password.isEmpty()) {
            statusLabel->setText("⚠️ Введите пароль");
            passwordEdit->setFocus();
            return;
        }

        // ✅ Используем AuthDialog для проверки (он уже умеет работать с БД)
        AuthDialog authDlg(username, this);

        // ✅ Передаём пароль в AuthDialog через его поле (если есть такой метод)
        // Если нет — используем прямой вызов через DatabaseManager
        bool authOk = false;

        // Пробуем через существующий механизм AuthDialog
        // AuthDialog обычно имеет метод для установки пароля или принимает его в конструкторе
        // Если такой возможности нет, используем DatabaseManager напрямую
        if (DatabaseManager::instance().isConnected()) {
            // ✅ ПРЯМАЯ ПРОВЕРКА ЧЕРЕЗ БД (надёжнее, чем через диалог)
            authOk = DatabaseManager::instance().verifyPassword(username, password);
        }

        if (authOk) {
            m_authenticated = true;
            m_authenticatedUser = username;

            // Сохраняем роль
            QString role = DatabaseManager::instance().getUserRole(username);
            DatabaseManager::instance().setCurrentUserRole(role);

            qDebug() << "[AuthRequired] Пользователь" << username
                     << "успешно авторизован через диалог";
            accept();
        } else {
            statusLabel->setText("❌ Неверный пароль или пользователь неактивен");
            passwordEdit->clear();
            passwordEdit->setFocus();
            qDebug() << "[AuthRequired] Неверный пароль для пользователя" << username;
        }
    }

private:
    QComboBox *userComboBox;
    QLineEdit *passwordEdit;
    QLabel *statusLabel;
    bool m_authenticated;
    QString m_authenticatedUser;
};

#endif // AUTHREQUIREDIALOG_H
