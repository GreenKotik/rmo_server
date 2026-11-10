#ifndef DRIVERCONFIGMODEL_H
#define DRIVERCONFIGMODEL_H

#include <QString>
#include <QStringList>
#include <QVector>
#include <QJsonObject>
#include <QJsonArray>
#include "modulatorconfig.h"   // ParamType, AlarmOp

// ============================================================================
// ✅ МОДЕЛЬ КОНФИГА ДРАЙВЕРА ДЛЯ РЕДАКТОРА
//
// Своя структура (не ModulatorConfig), чтобы:
//   • редактор не зависел от read-only загрузчика драйвера;
//   • сериализация toJson()/fromJson() была симметричной;
//   • можно было расширять под другие типы драйверов.
//
// Формат JSON — тот же, что читает ModulatorConfig::parseFile.
// ============================================================================
class DriverConfigModel
{
public:
    // Путь к файлу (если загружен с диска или сохранён)
    QString filePath;

    // Общее
    QString         model;
    QVector<int>    deviceIds;

    // Параметры (используем те же типы, что ModulatorConfig, но с сеттерами)
    struct Param {
        QString   key;
        QString   label;            // ← НОВОЕ
        QString   oid;
        ParamType type = ParamType::Double;
        QString   target;
        double    scale = 1.0;
        bool      oidTodo = false;
        bool      writable = false;
        int       commandCode = 0;
        double    min = 0.0;
        double    max = 0.0;
        int       snmpType = 2;
        bool      commandCodeTodo = false;
    };
    QVector<Param> params;

    // Правила аварий
    struct Rule {
        QString field;
        AlarmOp op = AlarmOp::Ne;
        double  value = 0.0;
        int     code = 0;
    };
    QVector<Rule> alarmRules;

    // Загрузка/сохранение
    bool loadFromFile(const QString &path, QString *errorOut = nullptr);
    bool saveToFile(const QString &path, QString *errorOut = nullptr) const;

    QJsonObject toJson() const;
    bool        fromJson(const QJsonObject &obj, QString *errorOut = nullptr);

    // Валидация. Возвращает список проблем; пустой список = всё ок.
    // knownTargets — список допустимых target из ModulatorData.
    // isAlarmCodeRegistered — предикат для проверки code в AlarmRegistry.
    QStringList validate(const QStringList &knownTargets,
                         const std::function<bool(int)> &isAlarmCodeRegistered) const;

    // Утилиты для UI
    static QString typeToString(ParamType t);
    static ParamType typeFromString(const QString &s);
    static QString opToString(AlarmOp op);
    static AlarmOp opFromString(const QString &s);
};

#endif // DRIVERCONFIGMODEL_H
