#include "driverconfigmodel.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSet>
#include <QDebug>
#include <functional>

// ============================================================================
// ✅ ЗАГРУЗКА / СОХРАНЕНИЕ
// ============================================================================
bool DriverConfigModel::loadFromFile(const QString &path, QString *errorOut)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QString("Не удалось открыть %1").arg(path);
        return false;
    }
    QJsonParseError err;
    const QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();

    if (err.error != QJsonParseError::NoError) {
        if (errorOut) *errorOut = QString("JSON: %1 (offset %2)")
            .arg(err.errorString()).arg(err.offset);
        return false;
    }
    if (!doc.isObject()) {
        if (errorOut) *errorOut = "Корень JSON не объект";
        return false;
    }

    if (!fromJson(doc.object(), errorOut)) return false;
    filePath = path;
    return true;
}

bool DriverConfigModel::saveToFile(const QString &path, QString *errorOut) const
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (errorOut) *errorOut = QString("Не удалось открыть на запись %1").arg(path);
        return false;
    }
    f.write(QJsonDocument(toJson()).toJson(QJsonDocument::Indented));
    f.close();
    return true;
}

QJsonObject DriverConfigModel::toJson() const
{
    QJsonObject root;
    root["model"] = model;

    QJsonArray ids;
    for (int id : deviceIds) ids.append(id);
    root["deviceIds"] = ids;

    QJsonArray paramsArr;
    for (const Param &p : params) {
        QJsonObject o;
        o["key"]    = p.key;
        o["label"] = p.label;          // ← НОВОЕ
        o["oid"]    = p.oid;
        o["type"]   = typeToString(p.type);
        o["target"] = p.target;
        if (!qFuzzyCompare(p.scale, 1.0)) o["scale"] = p.scale;
        if (p.oidTodo) o["oidTodo"] = true;
        if (p.writable) o["writable"] = true;
        if (p.commandCode != 0) o["commandCode"] = p.commandCode;
        if (p.writable) {
            o["min"] = p.min;
            o["max"] = p.max;
            o["snmpType"] = p.snmpType;
        }
        if (p.commandCodeTodo) o["commandCodeTodo"] = true;
        paramsArr.append(o);
    }
    root["params"] = paramsArr;

    QJsonArray rulesArr;
    for (const Rule &r : alarmRules) {
        QJsonObject o;
        o["field"] = r.field;
        o["op"]    = opToString(r.op);
        o["value"] = r.value;
        o["code"]  = r.code;
        rulesArr.append(o);
    }
    root["alarmRules"] = rulesArr;

    return root;
}

bool DriverConfigModel::fromJson(const QJsonObject &obj, QString *errorOut)
{
    model.clear();
    deviceIds.clear();
    params.clear();
    alarmRules.clear();

    model = obj.value("model").toString();

    for (const QJsonValue &v : obj.value("deviceIds").toArray()) {
        deviceIds.append(v.toInt());
    }

    for (const QJsonValue &v : obj.value("params").toArray()) {
        QJsonObject o = v.toObject();
        Param p;
        p.key    = o.value("key").toString();
        p.label  = o.value("label").toString();   // ← НОВОЕ
        p.oid    = o.value("oid").toString();
        p.type   = typeFromString(o.value("type").toString("double"));
        p.target = o.value("target").toString();
        p.scale  = o.value("scale").toDouble(1.0);
        p.oidTodo = o.value("oidTodo").toBool(false);
        p.writable = o.value("writable").toBool(false);
        p.commandCode = o.value("commandCode").toInt(0);
        p.min = o.value("min").toDouble(0.0);
        p.max = o.value("max").toDouble(0.0);
        p.snmpType = o.value("snmpType").toInt(2);
        p.commandCodeTodo = o.value("commandCodeTodo").toBool(false);
        params.append(p);
    }

    for (const QJsonValue &v : obj.value("alarmRules").toArray()) {
        QJsonObject o = v.toObject();
        Rule r;
        r.field = o.value("field").toString();
        r.op    = opFromString(o.value("op").toString("!="));
        r.value = o.value("value").toDouble(0.0);
        r.code  = o.value("code").toInt(0);
        alarmRules.append(r);
    }
    return true;
}

// ============================================================================
// ✅ ВАЛИДАЦИЯ
// ============================================================================
QStringList DriverConfigModel::validate(
    const QStringList &knownTargets,
    const std::function<bool(int)> &isAlarmCodeRegistered) const
{
    QStringList problems;
    const QSet<QString> knownTargetSet(knownTargets.begin(), knownTargets.end());

    // --- Общее ---
    if (model.trimmed().isEmpty())
        problems << "model: пустое значение";

    if (deviceIds.isEmpty()) {
        problems << "deviceIds: пустой список";
    } else {
        QSet<int> seen;
        for (int id : deviceIds) {
            if (id <= 0)
                problems << QString("deviceIds: некорректный id %1").arg(id);
            if (seen.contains(id))
                problems << QString("deviceIds: дубль %1").arg(id);
            seen.insert(id);
        }
    }

    // --- Параметры ---
    QSet<QString> seenKeys;
    QSet<QString> seenTargets;
    for (int i = 0; i < params.size(); ++i) {
        const Param &p = params[i];
        const QString prefix = QString("params[%1] (%2)").arg(i).arg(p.key);

        if (p.key.trimmed().isEmpty())
            problems << QString("params[%1]: пустой key").arg(i);
        else if (seenKeys.contains(p.key))
            problems << prefix + ": дубль key";
        else
            seenKeys.insert(p.key);

        if (p.oid.trimmed().isEmpty()) {
            problems << prefix + ": пустой oid";
        } else if (!p.oidTodo) {
            // Простая проверка: начинается с точки, минимум 5 компонентов
            static const QRegularExpression re("^\\.[0-9]+(\\.[0-9]+){3,}$");
            if (!re.match(p.oid).hasMatch())
                problems << prefix + ": oid не похож на корректный (и не помечен oidTodo)";
        }

        if (p.target.trimmed().isEmpty()) {
            problems << prefix + ": пустой target";
        } else if (!knownTargetSet.contains(p.target)) {
            problems << prefix + QString(": target '%1' отсутствует в ModulatorData").arg(p.target);
        } else if (seenTargets.contains(p.target)) {
            problems << prefix + QString(": target '%1' дублируется").arg(p.target);
        } else {
            seenTargets.insert(p.target);
        }

        if (p.scale == 0.0)
            problems << prefix + ": scale = 0";

        if (p.writable) {
            if (p.commandCode == 0)
                problems << prefix + ": writable, но commandCode = 0";
            if (p.min >= p.max)
                problems << prefix + ": writable, но min >= max";
            if (p.snmpType != 2 && p.snmpType != 4)
                problems << prefix + QString(": snmpType = %1 (ожидается 2 или 4)").arg(p.snmpType);
        }

        if (p.commandCode != 0 && !p.commandCodeTodo && isAlarmCodeRegistered) {
            if (!isAlarmCodeRegistered(p.commandCode))
                problems << prefix + QString(": commandCode %1 не зарегистрирован в AlarmRegistry").arg(p.commandCode);
        }
    }

    // --- Правила аварий ---
    for (int i = 0; i < alarmRules.size(); ++i) {
        const Rule &r = alarmRules[i];
        const QString prefix = QString("alarmRules[%1]").arg(i);

        if (r.field.trimmed().isEmpty()) {
            problems << prefix + ": пустой field";
        } else if (!knownTargetSet.contains(r.field)) {
            problems << prefix + QString(": field '%1' отсутствует в ModulatorData").arg(r.field);
        }

        if (r.code == 0) {
            problems << prefix + ": code = 0";
        } else if (isAlarmCodeRegistered && !isAlarmCodeRegistered(r.code)) {
            problems << prefix + QString(": code %1 не зарегистрирован в AlarmRegistry").arg(r.code);
        }
    }

    return problems;
}

// ============================================================================
// ✅ УТИЛИТЫ
// ============================================================================
QString DriverConfigModel::typeToString(ParamType t)
{
    switch (t) {
        case ParamType::String: return "string";
        case ParamType::Int:    return "int";
        case ParamType::Double: return "double";
        case ParamType::Bool:   return "bool";
    }
    return "double";
}

ParamType DriverConfigModel::typeFromString(const QString &s)
{
    const QString t = s.trimmed().toLower();
    if (t == "string") return ParamType::String;
    if (t == "int")    return ParamType::Int;
    if (t == "bool")   return ParamType::Bool;
    return ParamType::Double;
}

QString DriverConfigModel::opToString(AlarmOp op)
{
    switch (op) {
        case AlarmOp::Eq: return "==";
        case AlarmOp::Ne: return "!=";
        case AlarmOp::Gt: return ">";
        case AlarmOp::Lt: return "<";
        case AlarmOp::Ge: return ">=";
        case AlarmOp::Le: return "<=";
    }
    return "!=";
}

AlarmOp DriverConfigModel::opFromString(const QString &s)
{
    const QString t = s.trimmed();
    if (t == "==") return AlarmOp::Eq;
    if (t == "!=") return AlarmOp::Ne;
    if (t == ">")  return AlarmOp::Gt;
    if (t == "<")  return AlarmOp::Lt;
    if (t == ">=") return AlarmOp::Ge;
    if (t == "<=") return AlarmOp::Le;
    return AlarmOp::Ne;
}
