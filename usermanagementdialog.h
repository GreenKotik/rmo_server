#ifndef USERMANAGEMENTDIALOG_H
#define USERMANAGEMENTDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QInputDialog>
#include <QHeaderView>
#include "databasemanager.h"


// ✅ ФУНКЦИИ КОНВЕРТАЦИИ РОЛЕЙ (только для интерфейса)
static QString roleToDisplay(const QString &role)
{
    if (role == "operator") return "Оператор";
    if (role == "engineer") return "Инженер";
    if (role == "admin") return "Администратор";
    return role; // Если роль неизвестна, возвращаем как есть
}

static QString displayToRole(const QString &display)
{
    if (display == "Оператор") return "operator";
    if (display == "Инженер") return "engineer";
    if (display == "Администратор") return "admin";
    return display;
}


class UserManagementDialog : public QDialog
{
    Q_OBJECT
public:
    explicit UserManagementDialog(QWidget *parent = nullptr)
        : QDialog(parent)
    {
        setWindowTitle("Конфигурация доступа - Управление пользователями");
        setMinimumSize(900, 600);

        // ✅ УБИРАЕМ КНОПКУ "?" (СПРАВКА) ИЗ ЗАГОЛОВКА
        setWindowFlags(windowFlags() & ~Qt::WindowContextHelpButtonHint);

        QVBoxLayout *mainLayout = new QVBoxLayout(this);

        // Таблица пользователей
        userTable = new QTableWidget(this);
        userTable->setColumnCount(6); // ✅ БЫЛО 7, СТАЛО 6 (убрали ID)
        userTable->setHorizontalHeaderLabels({
            "Имя пользователя", "Роль", "Статус",
            "Пароль изменён", "Создан", "Срок пароля"
        });
        userTable->horizontalHeader()->setStretchLastSection(true);
        userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        userTable->setSelectionMode(QAbstractItemView::SingleSelection);
        userTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        userTable->setAlternatingRowColors(true);
        userTable->setStyleSheet(
            "QTableWidget { gridline-color: #D0D0D0; }"
            "QHeaderView::section { background-color: #005A9E; color: white; padding: 6px; }"
        );

        mainLayout->addWidget(userTable);

        // Панель кнопок
        QHBoxLayout *btnLayout = new QHBoxLayout();

        QPushButton *addBtn = new QPushButton("➕ Добавить пользователя", this);
        addBtn->setStyleSheet("QPushButton { background-color: #2E7D32; color: white; padding: 8px 16px; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #1B5E20; }");
        connect(addBtn, &QPushButton::clicked, this, &UserManagementDialog::onAddUser);

        QPushButton *deleteBtn = new QPushButton("🗑️ Удалить", this);
        deleteBtn->setStyleSheet("QPushButton { background-color: #C62828; color: white; padding: 8px 16px; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #B71C1C; }");
        connect(deleteBtn, &QPushButton::clicked, this, &UserManagementDialog::onDeleteUser);

        QPushButton *changeRoleBtn = new QPushButton("🔄 Изменить роль", this);
        changeRoleBtn->setStyleSheet("QPushButton { background-color: #6A1B9A; color: white; padding: 8px 16px; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #4A148C; }");
        connect(changeRoleBtn, &QPushButton::clicked, this, &UserManagementDialog::onChangeRole);

        QPushButton *resetPassBtn = new QPushButton("🔑 Сбросить пароль", this);
        resetPassBtn->setStyleSheet("QPushButton { background-color: #E65100; color: white; padding: 8px 16px; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #BF360C; }");
        connect(resetPassBtn, &QPushButton::clicked, this, &UserManagementDialog::onResetPassword);

        QPushButton *toggleActiveBtn = new QPushButton("⚡ Активировать/Деактивировать", this);
        toggleActiveBtn->setStyleSheet("QPushButton { background-color: #005A9E; color: white; padding: 8px 16px; border-radius: 4px; font-weight: bold; } QPushButton:hover { background-color: #004A8E; }");
        connect(toggleActiveBtn, &QPushButton::clicked, this, &UserManagementDialog::onToggleActive);

        QPushButton *refreshBtn = new QPushButton("🔄 Обновить", this);
        refreshBtn->setStyleSheet("QPushButton { padding: 8px 16px; border: 1px solid #B0B0B0; border-radius: 4px; } QPushButton:hover { background-color: #E0E0E0; }");
        connect(refreshBtn, &QPushButton::clicked, this, &UserManagementDialog::loadUsers);

        btnLayout->addWidget(addBtn);
        btnLayout->addWidget(deleteBtn);
        btnLayout->addWidget(changeRoleBtn);
        btnLayout->addWidget(resetPassBtn);
        btnLayout->addWidget(toggleActiveBtn);
        btnLayout->addStretch();
        btnLayout->addWidget(refreshBtn);

        mainLayout->addLayout(btnLayout);

        // Загрузка данных
        loadUsers();
    }

private slots:
    void loadUsers()
    {
        QList<UserInfo> users = DatabaseManager::instance().getAllUsers();

        userTable->setRowCount(users.size());

        for (int i = 0; i < users.size(); ++i) {
            const UserInfo &user = users[i];

            userTable->setItem(i, 0, new QTableWidgetItem(user.username));

            // ✅ ИСПОЛЬЗУЕМ КОНВЕРТАЦИЮ ДЛЯ ОТОБРАЖЕНИЯ
            userTable->setItem(i, 1, new QTableWidgetItem(roleToDisplay(user.role)));

            QTableWidgetItem *statusItem = new QTableWidgetItem(user.isActive ? "✅ Активен" : "❌ Заблокирован");
            statusItem->setForeground(user.isActive ? QColor("#2E7D32") : QColor("#C62828"));
            userTable->setItem(i, 2, statusItem);

            userTable->setItem(i, 3, new QTableWidgetItem(
                user.passwordChangedAt.isValid()
                    ? user.passwordChangedAt.toString("dd.MM.yyyy HH:mm")
                    : "Никогда"
            ));

            userTable->setItem(i, 4, new QTableWidgetItem(
                user.createdAt.isValid()
                    ? user.createdAt.toString("dd.MM.yyyy HH:mm")
                    : "Неизвестно"
            ));

            bool expired = DatabaseManager::instance().isPasswordExpired(user.username, 90);
            QTableWidgetItem *expiryItem = new QTableWidgetItem(expired ? "⚠️ Истёк" : "✅ Действует");
            expiryItem->setForeground(expired ? QColor("#C62828") : QColor("#2E7D32"));
            userTable->setItem(i, 5, expiryItem);
        }

        userTable->resizeColumnsToContents();
    }

    void onAddUser()
    {
        bool ok;
        QString username = QInputDialog::getText(this, "Добавить пользователя", "Имя пользователя:", QLineEdit::Normal, "", &ok);
        if (!ok || username.isEmpty()) return;

        QString password = QInputDialog::getText(this, "Добавить пользователя", "Пароль:", QLineEdit::Password, "", &ok);
        if (!ok || password.isEmpty()) return;

        // ✅ ПОКАЗЫВАЕМ РУССКИЕ РОЛИ
        QStringList displayRoles = {"Оператор", "Инженер", "Администратор"};
        QString selectedDisplay = QInputDialog::getItem(this, "Добавить пользователя", "Роль:", displayRoles, 0, false, &ok);
        if (!ok) return;

        // ✅ КОНВЕРТИРУЕМ В АНГЛИЙСКУЮ ДЛЯ СОХРАНЕНИЯ В БД
        QString role = displayToRole(selectedDisplay);

        if (DatabaseManager::instance().addUser(username, password, role)) {
            QMessageBox::information(this, "Успех", "Пользователь успешно добавлен.");
            loadUsers();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось добавить пользователя.");
        }
    }

    void onDeleteUser()
    {
        int row = userTable->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Внимание", "Выберите пользователя для удаления.");
            return;
        }

        // ✅ ТЕПЕРЬ username в столбце 0 (был 1)
        QString username = userTable->item(row, 0)->text();

        // Нужно получить ID из БД по имени пользователя
        QList<UserInfo> users = DatabaseManager::instance().getAllUsers();
        int userId = -1;
        for (const UserInfo &u : users) {
            if (u.username == username) {
                userId = u.id;
                break;
            }
        }

        if (userId < 0) {
            QMessageBox::critical(this, "Ошибка", "Не удалось найти пользователя в БД.");
            return;
        }

        if (QMessageBox::question(this, "Подтверждение",
            QString("Вы уверены, что хотите удалить пользователя '%1'?").arg(username),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

            if (DatabaseManager::instance().deleteUser(userId)) {
                QMessageBox::information(this, "Успех", "Пользователь удалён.");
                loadUsers();
            } else {
                QMessageBox::critical(this, "Ошибка", "Не удалось удалить пользователя.");
            }
        }
    }

    void onChangeRole()
    {
        int row = userTable->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Внимание", "Выберите пользователя.");
            return;
        }

        QString username = userTable->item(row, 0)->text();

        QList<UserInfo> users = DatabaseManager::instance().getAllUsers();
        int userId = -1;
        for (const UserInfo &u : users) {
            if (u.username == username) {
                userId = u.id;
                break;
            }
        }

        if (userId < 0) {
            QMessageBox::critical(this, "Ошибка", "Не удалось найти пользователя в БД.");
            return;
        }

        // ✅ ПОКАЗЫВАЕМ РУССКИЕ РОЛИ
        QStringList displayRoles = {"Оператор", "Инженер", "Администратор"};
        bool ok;
        QString selectedDisplay = QInputDialog::getItem(this, "Изменить роль", "Новая роль:", displayRoles, 0, false, &ok);
        if (!ok) return;

        // ✅ КОНВЕРТИРУЕМ В АНГЛИЙСКУЮ ДЛЯ СОХРАНЕНИЯ В БД
        QString newRole = displayToRole(selectedDisplay);

        if (DatabaseManager::instance().changeUserRole(userId, newRole)) {
            QMessageBox::information(this, "Успех", "Роль изменена.");
            loadUsers();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось изменить роль.");
        }
    }

    void onResetPassword()
    {
        int row = userTable->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Внимание", "Выберите пользователя.");
            return;
        }

        // ✅ ТЕПЕРЬ username в столбце 0
        QString username = userTable->item(row, 0)->text();

        QList<UserInfo> users = DatabaseManager::instance().getAllUsers();
        int userId = -1;
        for (const UserInfo &u : users) {
            if (u.username == username) {
                userId = u.id;
                break;
            }
        }

        if (userId < 0) {
            QMessageBox::critical(this, "Ошибка", "Не удалось найти пользователя в БД.");
            return;
        }

        bool ok;
        QString newPassword = QInputDialog::getText(this, "Сбросить пароль",
            QString("Новый пароль для '%1':").arg(username), QLineEdit::Password, "", &ok);
        if (!ok || newPassword.isEmpty()) return;

        if (DatabaseManager::instance().resetPassword(userId, newPassword)) {
            QMessageBox::information(this, "Успех", "Пароль сброшен.");
            loadUsers();
        } else {
            QMessageBox::critical(this, "Ошибка", "Не удалось сбросить пароль.");
        }
    }

    void onToggleActive()
    {
        int row = userTable->currentRow();
        if (row < 0) {
            QMessageBox::warning(this, "Внимание", "Выберите пользователя.");
            return;
        }

        // ✅ ТЕПЕРЬ username в столбце 0, статус в столбце 2
        QString username = userTable->item(row, 0)->text();
        bool currentStatus = userTable->item(row, 2)->text().contains("Активен");

        QList<UserInfo> users = DatabaseManager::instance().getAllUsers();
        int userId = -1;
        for (const UserInfo &u : users) {
            if (u.username == username) {
                userId = u.id;
                break;
            }
        }

        if (userId < 0) {
            QMessageBox::critical(this, "Ошибка", "Не удалось найти пользователя в БД.");
            return;
        }

        QString action = currentStatus ? "деактивировать" : "активировать";

        if (QMessageBox::question(this, "Подтверждение",
            QString("Вы хотите %1 пользователя '%2'?").arg(action, username),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes) {

            if (DatabaseManager::instance().setUserActive(userId, !currentStatus)) {
                QMessageBox::information(this, "Успех", QString("Пользователь %1.").arg(action + "н"));
                loadUsers();
            } else {
                QMessageBox::critical(this, "Ошибка", "Не удалось изменить статус.");
            }
        }
    }

private:
    QTableWidget *userTable;
};

#endif // USERMANAGEMENTDIALOG_H
