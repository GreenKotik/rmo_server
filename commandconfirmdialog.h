#ifndef COMMANDCONFIRMDIALOG_H
#define COMMANDCONFIRMDIALOG_H

#include <QDialog>
#include <QString>

class QLabel;
class QPushButton;

// ============================================================================
// ✅ ПЕРЕИСПОЛЬЗУЕМЫЙ ДИАЛОГ ПОДТВЕРЖДЕНИЯ КОМАНД
// Используется для всех документов (модулятор, передатчик, антенна и т.д.)
// Проверяет роль пользователя перед выполнением команды
// ============================================================================
class CommandConfirmDialog : public QDialog
{
    Q_OBJECT
public:
    // commandName   — "Переключить на Модулятор 2"
    // description   — "Произойдёт переключение активного пути..."
    // requiredRole  — "engineer" или "admin"
    // currentUser   — текущий авторизованный пользователь (может быть пустым)
    // parent        — родительское окно
    explicit CommandConfirmDialog(const QString &commandName,
                                   const QString &description,
                                   const QString &requiredRole,
                                   const QString &currentUser,
                                   QWidget *parent = nullptr);

    // Возвращает true, если пользователь подтвердил И имеет право
    bool isConfirmed() const { return m_confirmed; }

private slots:
    void onExecuteClicked();

private:
    bool m_confirmed = false;
    QString m_commandName;
    QString m_requiredRole;
    QString m_currentUser;

    QLabel *m_statusLabel;
    QPushButton *m_executeButton;

    // Проверка прав доступа
    bool hasPermission() const;
    void updateStatus();
};

#endif // COMMANDCONFIRMDIALOG_H
