#ifndef ALARMREGISTRY_H
#define ALARMREGISTRY_H

#include <QString>
#include <QMap>
#include <QObject>
#include <QDebug>

// ============================================================================
// ✅ СТРУКТУРА ИНФОРМАЦИИ ОБ АВАРИИ
// ============================================================================
struct AlarmInfo {
    int code;           // Код аварии (например, 101)
    int status;         // 0 = информация (зелёный), 1 = авария (красный), 2 = команда
    QString mesRus;     // Текст сообщения на русском
};

// ============================================================================
// ✅ РЕЕСТР АВАРИЙНЫХ СООБЩЕНИЙ
// ============================================================================
class AlarmRegistry : public QObject
{
    Q_OBJECT
public:
    static AlarmRegistry& instance()
    {
        static AlarmRegistry inst;
        return inst;
    }

    // ✅ Получить информацию об аварии по коду
    AlarmInfo getInfo(int code) const
    {
        return m_alarms.value(code, AlarmInfo{code, 0, QString("Неизвестная авария (код: %1)").arg(code)});
    }

    // ✅ Получить статус аварии (0/1/2)
    int getStatus(int code) const
    {
        return m_alarms.value(code).status;
    }

    // ✅ Получить текст сообщения
    QString getMessage(int code) const
    {
        return m_alarms.value(code).mesRus;
    }

    // ✅ Проверить, существует ли код в реестре
    bool contains(int code) const
    {
        return m_alarms.contains(code);
    }

    // Зарегистрирован ли код (авария или команда)
    bool hasCode(int code) const { return m_alarms.contains(code); }

private:
    AlarmRegistry();
    ~AlarmRegistry()
    {
        qDebug() << "[AlarmRegistry] Деструктор вызван";
        // Очистка данных
        qDebug() << "[AlarmRegistry] Деструктор завершён";
    }
    AlarmRegistry(const AlarmRegistry&) = delete;
    AlarmRegistry& operator=(const AlarmRegistry&) = delete;

    void registerAlarm(int code, int status, const QString &message);

    QMap<int, AlarmInfo> m_alarms;
};

#endif // ALARMREGISTRY_H
