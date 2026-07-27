#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QObject>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStringList>
#include <QDateTime>
#include <QCryptographicHash>
#include <QByteArray>
#include <QDebug>
#include <QList>
#include <QRandomGenerator>
#include <algorithm>  // ✅ ДОБАВИТЬ для std::sort

// ============================================================================
// ✅ СТРУКТУРА ДЛЯ ХРАНЕНИЯ ИНФОРМАЦИИ О ПОЛЬЗОВАТЕЛЕ
// ============================================================================
struct UserInfo {
    int id;
    QString username;
    QString password;
    QString role;
    bool isActive;
    QDateTime passwordChangedAt;
    QDateTime createdAt;
};

// ============================================================================
// ✅ КЛАСС ДЛЯ ШИФРОВАНИЯ И ХЭШИРОВАНИЯ (без изменений)
// ============================================================================
class CryptoHelper
{
public:

    static QByteArray encryptionKey()
    {
        return QByteArray("Rm0_S3cur3_K3y_2026!@#$%^&*()_Pr0j3ct");
    }

    static QByteArray generateSalt(int length = 16)
    {
        QByteArray salt;
        salt.reserve(length);
        for (int i = 0; i < length; ++i) {
            salt.append(static_cast<char>(QRandomGenerator::global()->generate() % 256));
        }
        return salt.toBase64();
    }

    static QByteArray hashPassword(const QString &password, const QByteArray &salt)
    {
        QByteArray data = salt.toBase64() + password.toUtf8() + encryptionKey();
        return QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex();
    }

    static QString createPasswordHash(const QString &password)
    {
        QByteArray salt = generateSalt();
        QByteArray hash = hashPassword(password, salt);
        return QString("%1:%2").arg(QString(salt), QString(hash));
    }

    static bool verifyPassword(const QString &password, const QString &storedHash)
    {
        QStringList parts = storedHash.split(':');
        if (parts.size() != 2) return false;

        QByteArray salt = parts[0].toUtf8();
        QByteArray hash = parts[1].toUtf8();
        return hashPassword(password, salt) == hash;
    }

    static QString encrypt(const QString &plainText)
    {
        if (plainText.isEmpty()) return QString();

        QByteArray data = plainText.toUtf8();
        QByteArray key = encryptionKey();
        QByteArray encrypted;
        encrypted.reserve(data.size());

        for (int i = 0; i < data.size(); ++i) {
            encrypted.append(data[i] ^ key[i % key.size()]);
        }

        return QString::fromLatin1(encrypted.toBase64());
    }

    static QString decrypt(const QString &cipherText)
    {
        if (cipherText.isEmpty()) return QString();

        QByteArray data = QByteArray::fromBase64(cipherText.toLatin1());
        QByteArray key = encryptionKey();
        QByteArray decrypted;
        decrypted.reserve(data.size());

        for (int i = 0; i < data.size(); ++i) {
            decrypted.append(data[i] ^ key[i % key.size()]);
        }

        return QString::fromUtf8(decrypted);
    }
};

// ============================================================================
// ✅ МЕНЕДЖЕР БАЗЫ ДАННЫХ
// ============================================================================
class DatabaseManager : public QObject
{
    Q_OBJECT
public:
    static DatabaseManager& instance()
    {
        static DatabaseManager inst;
        return inst;
    }

    bool connect(const QString &host, int port,
                 const QString &dbName,
                 const QString &user,
                 const QString &password)
    {
        if (QSqlDatabase::contains("rmo_connection")) {
            m_db = QSqlDatabase::database("rmo_connection");
        } else {
            m_db = QSqlDatabase::addDatabase("QPSQL", "rmo_connection");
        }

        m_db.setHostName(host);
        m_db.setPort(port);
        m_db.setDatabaseName(dbName);
        m_db.setUserName(user);
        m_db.setPassword(password);

        if (!m_db.open()) {
            qWarning() << "[DB] Ошибка подключения:" << m_db.lastError().text();
            return false;
        }

        qDebug() << "[DB] Успешное подключение к" << dbName << "на" << host;
        return true;
    }

    bool isConnected() const { return m_db.isOpen(); }

    // ✅ НОВЫЙ МЕТОД: Получить всех пользователей (для таблицы администрирования)
    QList<UserInfo> getAllUsers()
    {
        QList<UserInfo> users;
        if (!isConnected()) return users;

        QSqlQuery query(m_db);
        query.prepare("SELECT id, username, password, role, is_active, password_changed_at, created_at FROM users ORDER BY id");

        if (query.exec()) {
            while (query.next()) {
                UserInfo user;
                user.id = query.value(0).toInt();
                user.username = CryptoHelper::decrypt(query.value(1).toString());
                user.password = query.value(2).toString();
                user.role = CryptoHelper::decrypt(query.value(3).toString());
                user.isActive = query.value(4).toBool();
                user.passwordChangedAt = query.value(5).toDateTime();
                user.createdAt = query.value(6).toDateTime();
                users.append(user);
            }
        } else {
            qWarning() << "[DB] Ошибка запроса всех пользователей:" << query.lastError().text();
        }

        // ✅ СОРТИРОВКА ПО АЛФАВИТУ (по расшифрованному имени)
        std::sort(users.begin(), users.end(), [](const UserInfo &a, const UserInfo &b) {
            return a.username.toLower() < b.username.toLower();
        });

        return users;
    }

    // ✅ НОВЫЙ МЕТОД: Добавить пользователя
    bool addUser(const QString &username, const QString &password, const QString &role = "operator")
    {
        if (!isConnected()) return false;

        QSqlQuery query(m_db);
        query.prepare(
            "INSERT INTO users (username, password, role) "
            "VALUES (:username, :password, :role)"
        );
        query.bindValue(":username", CryptoHelper::encrypt(username));
        query.bindValue(":password", CryptoHelper::createPasswordHash(password));
        query.bindValue(":role", CryptoHelper::encrypt(role));

        if (!query.exec()) {
            qWarning() << "[DB] Ошибка добавления пользователя:" << query.lastError().text();
            return false;
        }
        return true;
    }

    // ✅ НОВЫЙ МЕТОД: Удалить пользователя по ID
    bool deleteUser(int userId)
    {
        if (!isConnected()) return false;

        QSqlQuery query(m_db);
        query.prepare("DELETE FROM users WHERE id = :id");
        query.bindValue(":id", userId);

        if (!query.exec()) {
            qWarning() << "[DB] Ошибка удаления пользователя:" << query.lastError().text();
            return false;
        }
        return true;
    }

    // ✅ НОВЫЙ МЕТОД: Изменить роль пользователя
    bool changeUserRole(int userId, const QString &newRole)
    {
        if (!isConnected()) return false;

        QSqlQuery query(m_db);
        query.prepare("UPDATE users SET role = :role WHERE id = :id");
        query.bindValue(":role", CryptoHelper::encrypt(newRole));
        query.bindValue(":id", userId);

        if (!query.exec()) {
            qWarning() << "[DB] Ошибка изменения роли:" << query.lastError().text();
            return false;
        }
        return true;
    }

    // ✅ НОВЫЙ МЕТОД: Сбросить пароль пользователя
    bool resetPassword(int userId, const QString &newPassword)
    {
        if (!isConnected()) return false;

        QSqlQuery query(m_db);
        query.prepare(
            "UPDATE users SET password = :password, password_changed_at = CURRENT_TIMESTAMP "
            "WHERE id = :id"
        );
        query.bindValue(":password", CryptoHelper::createPasswordHash(newPassword));
        query.bindValue(":id", userId);

        if (!query.exec()) {
            qWarning() << "[DB] Ошибка сброса пароля:" << query.lastError().text();
            return false;
        }
        return true;
    }

    // ✅ НОВЫЙ МЕТОД: Сбросить пароль по имени пользователя
    bool resetPasswordByUsername(const QString &username, const QString &newPassword)
    {
        if (!isConnected()) return false;

        QString encryptedUsername = CryptoHelper::encrypt(username);

        QSqlQuery query(m_db);
        query.prepare(
            "UPDATE users SET password = :password, password_changed_at = CURRENT_TIMESTAMP "
            "WHERE username = :username AND is_active = TRUE"
        );
        query.bindValue(":password", CryptoHelper::createPasswordHash(newPassword));
        query.bindValue(":username", encryptedUsername);

        if (!query.exec()) {
            qWarning() << "[DB] Ошибка сброса пароля:" << query.lastError().text();
            return false;
        }
        return query.numRowsAffected() > 0;
    }

    // ✅ НОВЫЙ МЕТОД: Активировать/деактивировать пользователя
    bool setUserActive(int userId, bool active)
    {
        if (!isConnected()) return false;

        QSqlQuery query(m_db);
        query.prepare("UPDATE users SET is_active = :active WHERE id = :id");
        query.bindValue(":active", active);
        query.bindValue(":id", userId);

        if (!query.exec()) {
            qWarning() << "[DB] Ошибка изменения статуса активности:" << query.lastError().text();
            return false;
        }
        return true;
    }

    QStringList getUsernames()
    {
        QStringList list;
        if (!isConnected()) return list;

        QSqlQuery query(m_db);
        query.prepare("SELECT username FROM users WHERE is_active = TRUE ORDER BY id");

        if (query.exec()) {
            while (query.next()) {
                QString encrypted = query.value(0).toString();
                QString decrypted = CryptoHelper::decrypt(encrypted);
                if (!decrypted.isEmpty()) {
                    list << decrypted;
                }
            }
        } else {
            qWarning() << "[DB] Ошибка запроса пользователей:" << query.lastError().text();
        }

        // ✅ СОРТИРОВКА ПО АЛФАВИТУ
        list.sort();

        return list;
    }

    bool checkPassword(const QString &username, const QString &password)
    {
        if (!isConnected()) return false;

        QString encryptedUsername = CryptoHelper::encrypt(username);

        QSqlQuery query(m_db);
        query.prepare("SELECT password FROM users WHERE username = :username AND is_active = TRUE");
        query.bindValue(":username", encryptedUsername);

        if (query.exec() && query.next()) {
            QString storedHash = query.value(0).toString();
            return CryptoHelper::verifyPassword(password, storedHash);
        }
        return false;
    }

    // ✅ НОВЫЙ МЕТОД: Проверка пароля (обёртка над checkPassword для единообразия API)
    bool verifyPassword(const QString &username, const QString &password)
    {
        return checkPassword(username, password);
    }

    QDateTime getPasswordChangedAt(const QString &username)
    {
        if (!isConnected()) return QDateTime();

        QString encryptedUsername = CryptoHelper::encrypt(username);

        QSqlQuery query(m_db);
        query.prepare("SELECT password_changed_at FROM users WHERE username = :username AND is_active = TRUE");
        query.bindValue(":username", encryptedUsername);

        if (query.exec() && query.next()) {
            return query.value(0).toDateTime();
        }
        return QDateTime();
    }

    bool isPasswordExpired(const QString &username, int maxDays = 90)
    {
        QDateTime changedAt = getPasswordChangedAt(username);
        if (!changedAt.isValid()) return true;
        return changedAt.daysTo(QDateTime::currentDateTime()) > maxDays;
    }

    bool clearUsers()
    {
        if (!isConnected()) return false;

        QSqlQuery query(m_db);
        if (!query.exec("DELETE FROM users")) {
            qWarning() << "[DB] Ошибка очистки таблицы users:" << query.lastError().text();
            return false;
        }

        qDebug() << "[DB] Таблица users очищена";
        return true;
    }

    // ✅ НОВЫЙ МЕТОД: Получить роль пользователя
    QString getUserRole(const QString &username)
    {
        if (!isConnected()) return QString();

        QString encryptedUsername = CryptoHelper::encrypt(username);

        QSqlQuery query(m_db);
        query.prepare("SELECT role FROM users WHERE username = :username AND is_active = TRUE");
        query.bindValue(":username", encryptedUsername);

        if (query.exec() && query.next()) {
            QString encryptedRole = query.value(0).toString();
            return CryptoHelper::decrypt(encryptedRole);
        }
        return QString();
    }

    // Проверка прав пользователя
    bool hasPermission(const QString &username, const QString &permission)
    {
        QString role = getUserRole(username);

        qDebug() << "[DB] Проверка прав: пользователь=" << username << "роль=" << role << "право=" << permission;

        // Создаём списки прав ЯВНО для каждой роли
        QStringList operatorPerms;
        operatorPerms << "view_documents"
                      << "edit_documents"
                      << "save_documents"
                      << "open_documents";

        QStringList engineerPerms = operatorPerms;
        engineerPerms << "create_documents"
                      << "delete_documents"
                      << "view_logs"
                      << "export_config";

        QStringList adminPerms = engineerPerms;
        adminPerms << "change_screen_config"
                   << "manage_users"
                   << "reset_passwords"
                   << "change_network_settings"
                   << "change_database_settings"  // ✅ НОВОЕ ПРАВО
                   << "import_config"
                   << "view_all_logs";

        // Проверяем права в зависимости от роли
        bool hasPerm = false;
        if (role == "operator") {
            hasPerm = operatorPerms.contains(permission);
        } else if (role == "engineer") {
            hasPerm = engineerPerms.contains(permission);
        } else if (role == "admin") {
            hasPerm = adminPerms.contains(permission);
        }

        qDebug() << "[DB] Результат проверки:" << hasPerm;

        return hasPerm;
    }

    // Получить роль текущего авторизованного пользователя
    QString getCurrentUserRole() const { return m_currentUserRole; }
    void setCurrentUserRole(const QString &role) { m_currentUserRole = role; }


private:
    DatabaseManager() = default;
    ~DatabaseManager() { if (m_db.isOpen()) m_db.close(); }
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;
    // В private секции добавить:
    QString m_currentUserRole;

    QSqlDatabase m_db;
};

#endif // DATABASEMANAGER_H
