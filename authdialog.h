#ifndef AUTHDIALOG_H
#define AUTHDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include "databasemanager.h"

class AuthDialog : public QDialog
{
    Q_OBJECT
public:
    explicit AuthDialog(const QString &username, QWidget *parent = nullptr)
        : QDialog(parent), m_username(username)
    {
        setWindowTitle("Авторизация");
        setFixedSize(320, 180);

        // ✅ УБИРАЕМ КНОПКУ "?" (СПРАВКА) ИЗ ЗАГОЛОВКА
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);
        mainLayout->setSpacing(12);
        mainLayout->setContentsMargins(20, 20, 20, 20);

        QLabel *greetingLabel = new QLabel(
            QString("Здравствуйте, <b>%1</b>!<br>Введите пароль для начала работы:")
                .arg(username), this);
        greetingLabel->setWordWrap(true);
        greetingLabel->setAlignment(Qt::AlignCenter);
        mainLayout->addWidget(greetingLabel);

        QHBoxLayout *passLayout = new QHBoxLayout();
        QLabel *passLabel = new QLabel("Пароль:", this);
        m_passwordEdit = new QLineEdit(this);
        m_passwordEdit->setEchoMode(QLineEdit::Password);
        m_passwordEdit->setPlaceholderText("Введите пароль...");
        m_passwordEdit->setStyleSheet(
            "QLineEdit { padding: 6px; border: 1px solid #B0B0B0; border-radius: 3px; }"
            "QLineEdit:focus { border: 1px solid #005A9E; }"
        );
        passLayout->addWidget(passLabel);
        passLayout->addWidget(m_passwordEdit);
        mainLayout->addLayout(passLayout);

        m_attemptsLabel = new QLabel("", this);
        m_attemptsLabel->setAlignment(Qt::AlignCenter);
        m_attemptsLabel->setStyleSheet("color: #C62828; font-weight: bold;");
        mainLayout->addWidget(m_attemptsLabel);

        QHBoxLayout *btnLayout = new QHBoxLayout();
        QPushButton *okBtn = new QPushButton("Войти", this);
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

        connect(okBtn, &QPushButton::clicked, this, &AuthDialog::onOkClicked);
        connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
        connect(m_passwordEdit, &QLineEdit::returnPressed, this, &AuthDialog::onOkClicked);

        m_attempts = 0;
        m_passwordEdit->setFocus();
    }

    bool isAuthenticated() const { return m_authenticated; }

private slots:
    void onOkClicked()
    {
        QString enteredPass = m_passwordEdit->text();

        if (DatabaseManager::instance().checkPassword(m_username, enteredPass)) {
            m_authenticated = true;
            accept();
        } else {
            m_attempts++;
            m_passwordEdit->clear();
            m_passwordEdit->setFocus();

            if (m_attempts >= 3) {
                QMessageBox::warning(this, "Ошибка",
                    "Превышено количество попыток ввода.\nДоступ заблокирован.");
                m_authenticated = false;
                reject();
            } else {
                m_attemptsLabel->setText(
                    QString("Неверный пароль. Осталось попыток: %1")
                        .arg(3 - m_attempts));
            }
        }
    }

private:
    QString m_username;
    QLineEdit *m_passwordEdit;
    QLabel *m_attemptsLabel;
    int m_attempts;
    bool m_authenticated = false;
};

#endif // AUTHDIALOG_H
