#include "commandconfirmdialog.h"
#include "databasemanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QMessageBox>
#include <QDebug>

CommandConfirmDialog::CommandConfirmDialog(const QString &commandName,
                                           const QString &description,
                                           const QString &requiredRole,
                                           const QString &currentUser,
                                           QWidget *parent)
    : QDialog(parent)
    , m_commandName(commandName)
    , m_requiredRole(requiredRole)
    , m_currentUser(currentUser)
{
    setWindowTitle("Подтверждение команды");
    setModal(true);
    setFixedSize(450, 280);

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(12);
    mainLayout->setContentsMargins(16, 16, 16, 16);

    // Заголовок команды
    QLabel *titleLabel = new QLabel(QString("<b>Команда:</b> %1").arg(commandName), this);
    titleLabel->setStyleSheet("font-size: 14px; color: #005A9E;");
    mainLayout->addWidget(titleLabel);

    // Описание
    QLabel *descLabel = new QLabel(description, this);
    descLabel->setWordWrap(true);
    descLabel->setStyleSheet("color: #333333; padding: 8px; "
                              "background-color: #F5F5F5; "
                              "border: 1px solid #D0D0D0; "
                              "border-radius: 4px;");
    mainLayout->addWidget(descLabel);

    // Информация о пользователе
    QString userText = currentUser.isEmpty()
        ? "Пользователь: <b style='color: #C62828;'>не авторизован</b>"
        : QString("Пользователь: <b>%1</b>").arg(currentUser);
    QLabel *userLabel = new QLabel(userText, this);
    mainLayout->addWidget(userLabel);

    // Статус доступа
    m_statusLabel = new QLabel(this);
    m_statusLabel->setStyleSheet("padding: 8px; border-radius: 4px; font-weight: bold;");
    mainLayout->addWidget(m_statusLabel);

    mainLayout->addStretch();

    // Кнопки
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton *cancelButton = new QPushButton("Отмена", this);
    cancelButton->setFixedSize(100, 32);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    buttonLayout->addWidget(cancelButton);

    m_executeButton = new QPushButton("Выполнить", this);
    m_executeButton->setFixedSize(120, 32);
    m_executeButton->setStyleSheet(
        "QPushButton { background-color: #2E7D32; color: white; font-weight: bold; border-radius: 4px; }"
        "QPushButton:hover { background-color: #1B5E20; }"
        "QPushButton:disabled { background-color: #BDBDBD; }"
    );
    connect(m_executeButton, &QPushButton::clicked, this, &CommandConfirmDialog::onExecuteClicked);
    buttonLayout->addWidget(m_executeButton);

    mainLayout->addLayout(buttonLayout);

    // Обновляем статус при создании
    updateStatus();
}

bool CommandConfirmDialog::hasPermission() const
{
    // Если пользователь не авторизован — нет прав
    if (m_currentUser.isEmpty()) {
        return false;
    }

    auto &db = DatabaseManager::instance();
    QString userRole = db.getUserRole(m_currentUser);

    // Проверяем соответствие требуемой роли
    // "admin" имеет все права, "engineer" имеет права инженера
    if (m_requiredRole == "engineer") {
        return (userRole == "engineer" || userRole == "admin");
    } else if (m_requiredRole == "admin") {
        return (userRole == "admin");
    }

    return false;
}

void CommandConfirmDialog::updateStatus()
{
    if (m_currentUser.isEmpty()) {
        m_statusLabel->setText("⚠ Требуется авторизация для выполнения команды");
        m_statusLabel->setStyleSheet(
            "padding: 8px; border-radius: 4px; font-weight: bold; "
            "background-color: #FFF3E0; color: #E65100;");
        m_executeButton->setEnabled(false);
    } else if (!hasPermission()) {
        m_statusLabel->setText("⛔ Недостаточно прав для выполнения команды");
        m_statusLabel->setStyleSheet(
            "padding: 8px; border-radius: 4px; font-weight: bold; "
            "background-color: #FFEBEE; color: #C62828;");
        m_executeButton->setEnabled(false);
    } else {
        m_statusLabel->setText("✓ У вас есть права на выполнение этой команды");
        m_statusLabel->setStyleSheet(
            "padding: 8px; border-radius: 4px; font-weight: bold; "
            "background-color: #E8F5E9; color: #2E7D32;");
        m_executeButton->setEnabled(true);
    }
}

void CommandConfirmDialog::onExecuteClicked()
{
    if (!hasPermission()) {
        QMessageBox::warning(this, "Доступ запрещён",
                             "У вас недостаточно прав для выполнения этой команды.");
        return;
    }

    m_confirmed = true;
    qDebug() << "[CommandConfirmDialog] Команда подтверждена:" << m_commandName
             << "пользователем:" << m_currentUser;
    accept();
}
