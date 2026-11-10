#ifndef CHANGEPASSWORDDIALOG_H
#define CHANGEPASSWORDDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QMessageBox>
#include <QRegularExpression>
#include "databasemanager.h"

class ChangePasswordDialog : public QDialog
{
    Q_OBJECT
public:
    explicit ChangePasswordDialog(const QString &username, const QString &role, QWidget *parent = nullptr)
        : QDialog(parent), m_username(username), m_role(role)
    {
        setWindowTitle("Смена пароля");
        setFixedSize(420, 340);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        // Заголовок
        QLabel *headerLabel = new QLabel(
            QString("<b>Смена пароля для пользователя:</b><br>%1").arg(username), this);
        headerLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(headerLabel);

        // Информация о требованиях
        int minLength = (role == "admin") ? 16 : 14;
        QLabel *requirementsLabel = new QLabel(
            QString("<b>Требования к паролю:</b><br>"
                    "• Минимум %1 символов<br>"
                    "• Хотя бы одна заглавная буква (A-Z, А-Я)<br>"
                    "• Хотя бы одна строчная буква (a-z, а-я)<br>"
                    "• Хотя бы одна цифра (0-9)<br>"
                    "• Хотя бы один спецсимвол (!@#$%^&*...)")
                .arg(minLength), this);
        requirementsLabel->setStyleSheet("color: #555; font-size: 11px;");
        requirementsLabel->setWordWrap(true);
        mainLayout->addWidget(requirementsLabel);

        // ✅ Форма ввода — только два поля: новый пароль и подтверждение
        QFormLayout *formLayout = new QFormLayout();

        m_newPassEdit = new QLineEdit(this);
        m_newPassEdit->setEchoMode(QLineEdit::Password);
        m_newPassEdit->setPlaceholderText("Введите новый пароль...");
        m_newPassEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        formLayout->addRow("Новый пароль:", m_newPassEdit);

        m_confirmPassEdit = new QLineEdit(this);
        m_confirmPassEdit->setEchoMode(QLineEdit::Password);
        m_confirmPassEdit->setPlaceholderText("Повторите новый пароль...");
        m_confirmPassEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        formLayout->addRow("Подтверждение:", m_confirmPassEdit);

        mainLayout->addLayout(formLayout);

        // Индикатор валидности пароля
        m_validationLabel = new QLabel("", this);
        m_validationLabel->setWordWrap(true);
        m_validationLabel->setStyleSheet("font-size: 11px;");
        mainLayout->addWidget(m_validationLabel);

        // Кнопки
        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *okBtn = new QPushButton("Сменить пароль", this);
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

        // Подключение сигналов
        connect(okBtn, &QPushButton::clicked, this, &ChangePasswordDialog::onOkClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_newPassEdit, &QLineEdit::textChanged, this, &ChangePasswordDialog::validatePassword);
        connect(m_confirmPassEdit, &QLineEdit::textChanged, this, &ChangePasswordDialog::validatePassword);

        m_newPassEdit->setFocus();
    }

private slots:
    void validatePassword()
    {
        QString newPass = m_newPassEdit->text();
        QString confirmPass = m_confirmPassEdit->text();

        int minLength = (m_role == "admin") ? 16 : 14;
        QStringList errors;

        // Проверка длины
        if (newPass.length() < minLength) {
            errors << QString("• Минимум %1 символов (сейчас: %2)")
                          .arg(minLength).arg(newPass.length());
        }

        // Проверка заглавной буквы
        if (!newPass.contains(QRegularExpression("[A-ZА-ЯЁ]"))) {
            errors << "• Нужна хотя бы одна заглавная буква";
        }

        // Проверка строчной буквы
        if (!newPass.contains(QRegularExpression("[a-zа-яё]"))) {
            errors << "• Нужна хотя бы одна строчная буква";
        }

        // Проверка цифры
        if (!newPass.contains(QRegularExpression("[0-9]"))) {
            errors << "• Нужна хотя бы одна цифра";
        }

        // Проверка спецсимвола
        if (!newPass.contains(QRegularExpression("[!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>\\/?~`]"))) {
            errors << "• Нужен хотя бы один спецсимвол";
        }

        // Проверка совпадения паролей
        if (!confirmPass.isEmpty() && newPass != confirmPass) {
            errors << "• Пароли не совпадают";
        }

        // Обновляем индикатор
        if (errors.isEmpty() && !newPass.isEmpty()) {
            m_validationLabel->setText("✅ Пароль соответствует всем требованиям");
            m_validationLabel->setStyleSheet("font-size: 11px; color: #2E7D32; font-weight: bold;");
        } else if (errors.isEmpty()) {
            m_validationLabel->setText("");
        } else {
            m_validationLabel->setText(errors.join("<br>"));
            m_validationLabel->setStyleSheet("font-size: 11px; color: #C62828;");
        }
    }

    void onOkClicked()
    {
        QString newPass = m_newPassEdit->text();
        QString confirmPass = m_confirmPassEdit->text();

        // ✅ Проверка нового пароля
        if (newPass.isEmpty()) {
            QMessageBox::warning(this, "Ошибка", "Введите новый пароль.");
            m_newPassEdit->setFocus();
            return;
        }

        int minLength = (m_role == "admin") ? 16 : 14;
        QStringList errors;

        if (newPass.length() < minLength) {
            errors << QString("Минимум %1 символов").arg(minLength);
        }
        if (!newPass.contains(QRegularExpression("[A-ZА-ЯЁ]"))) {
            errors << "Заглавная буква";
        }
        if (!newPass.contains(QRegularExpression("[a-zа-яё]"))) {
            errors << "Строчная буква";
        }
        if (!newPass.contains(QRegularExpression("[0-9]"))) {
            errors << "Цифра";
        }
        if (!newPass.contains(QRegularExpression("[!@#$%^&*()_+\\-=\\[\\]{};':\"\\\\|,.<>\\/?~`]"))) {
            errors << "Спецсимвол";
        }

        if (!errors.isEmpty()) {
            QMessageBox::warning(this, "Слабый пароль",
                "Пароль не соответствует требованиям:\n• " + errors.join("\n• "));
            m_newPassEdit->setFocus();
            return;
        }

        if (newPass != confirmPass) {
            QMessageBox::warning(this, "Ошибка", "Пароли не совпадают.");
            m_confirmPassEdit->clear();
            m_confirmPassEdit->setFocus();
            return;
        }

        // ✅ Сохраняем новый пароль (без проверки текущего)
        if (DatabaseManager::instance().resetPasswordByUsername(m_username, newPass)) {
            // ✅ Логирование смены пароля пользователем.
            //    ВАЖНО: сам пароль в журнал НЕ пишем.
            DatabaseManager::instance().logUserAction(
                "Пользователи",
                QString(),
                QString("Пользователь '%1' сменил свой пароль").arg(m_username));
            QMessageBox::information(this, "Успех",
                "Пароль успешно изменён.\n"
                "Используйте новый пароль при следующей авторизации.");
            accept();
            DatabaseManager::instance().logUserAction(
                "Пользователи", QString(),
                QString("Смена пароля пользователя '%1'").arg(m_username));
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось изменить пароль.");
        }
    }

private:
    QString m_username;
    QString m_role;
    QLineEdit *m_newPassEdit;      // ✅ Только новый пароль
    QLineEdit *m_confirmPassEdit;  // ✅ Только подтверждение
    QLabel *m_validationLabel;
};

#endif // CHANGEPASSWORDDIALOG_H
