#ifndef MODULATORTABBASE_H
#define MODULATORTABBASE_H

#include <QWidget>
#include <QJsonObject>
#include <memory>

class BaseDocWidget;
class IDeviceData;

// ============================================================================
// ✅ БАЗОВЫЙ КЛАСС ДЛЯ ТАБОВ ДИАЛОГА УПРАВЛЕНИЯ МОДУЛЯТОРОМ
// Все табы наследуются от этого класса и реализуют свои методы
// ============================================================================
class ModulatorTabBase : public QWidget
{
    Q_OBJECT
public:
    explicit ModulatorTabBase(BaseDocWidget *docWidget, QWidget *parent = nullptr);
    virtual ~ModulatorTabBase() = default;

    // ✅ Вызывается диалогом для обновления данных (например, по таймеру)
    virtual void refreshData() = 0;

    // ✅ Проверка, есть ли изменения, которые нужно применить
    virtual bool hasPendingChanges() const { return false; }

    // ✅ Применение изменений (отправка команд)
    virtual void applyChanges() {}

protected:
    BaseDocWidget *m_docWidget;  // Для доступа к getDeviceData() и sendCommandToDevice()
};

#endif // MODULATORTABBASE_H
